#include "provider_manage.h"
#include "clibase.h"
#include "cloud/exception.h"
#include "climanage.h"
#include "cloud/homacro.h"
#include "broadcastcli.h"
#include "act_mgr.h"
#include "rapidjson/json.hpp"
#include "comm/strparse.h"
#include "cloud/const.h"

class context_t {
	IOHand* iohand_;
	IOBuffItem* iBufItem;
	unsigned seqid_;
	nlohmann::json obj_;
public:
	context_t(void *ptr, void *param) : iohand_((IOHand *)ptr), iBufItem((IOBuffItem *)param)
	{
		seqid_ = iBufItem->head()->seqid;
		const char* body = iBufItem->body();
		std::string_view body1(body, iBufItem->totalLen - HEADER_LEN);
		obj_ = nlohmann::json::parse(body1, nullptr, false);
	}
	unsigned seqid() const { return seqid_; }
	bool isValid() const { return !obj_.is_discarded(); }
	nlohmann::json& obj()  { return obj_; }
	IOHand* io() { return iohand_; }
};

HEPCLASS_IMPL_FUNCX_BEG(ProviderMgr)
HEPCLASS_IMPL_FUNCX_END(ProviderMgr)

ProviderMgr::ProviderMgr( void )
{
	CliMgr::Instance()->addCliCloseConsumerFunc(OnCliCloseHandle);
	m_seqid = 0;
	HEPCLASS_IMPL_FUNCX_MORE(ProviderMgr, OnCMD_SVRREGISTER_REQ)
	HEPCLASS_IMPL_FUNCX_MORE(ProviderMgr, OnCMD_SVRSEARCH_REQ)
	HEPCLASS_IMPL_FUNCX_MORE(ProviderMgr, OnCMD_SVRSHOW_REQ)
	HEPCLASS_IMPL_FUNCX_MORE(ProviderMgr, OnCMD_SVRSTAT_REQ)
}

ProviderMgr::~ProviderMgr( void )
{
	auto itr =  m_providers.begin();
	for (; itr != m_providers.end(); ++itr)
	{
		ServiceProvider* second = itr->second;
		IFDELETE(second);
	}
	m_providers.clear();
}

void ProviderMgr::OnCliCloseHandle( CliBase* cli )
{
	ProviderMgr::Instance()->onCliCloseHandle(cli);
}

// 接收所有客户应用断开连接通知
void ProviderMgr::onCliCloseHandle( CliBase* cli )
{
	if (cli->getCliType() > 1)
	{
		auto itr = m_providers.begin();
		for (; itr != m_providers.end(); ++itr)
		{
			ServiceProvider* second = itr->second;
			bool exist = second->removeItme(cli);
			if (exist)
			{
				notify2Invoker(itr->first, cli->getIntProperty(CONNTERID_KEY), 0);
			}
		}
	}
}

void ProviderMgr::notify2Invoker( const std::string& regname, int svrid, int prvdid )
{
	std::string preKey(SVRBOOKCH_ALIAS_PREFIX);
	preKey += "_" + regname + "@";
	CliMgr::AliasCursor finder(preKey);
	CliBase *invokerCli = NULL;
	std::string msg;
	while ((invokerCli = finder.pop()))
	{
		IOHand *invokerIO = dynamic_cast<IOHand *>(invokerCli);
		if (NULL == invokerIO) continue;
		if (msg.empty())
		{
			msg = _F("{\"notify\": \"provider_down\", \"svrid\": %d, \"prvdid\": %d, \"regname\": \"%s\"}",
					 svrid, prvdid, regname.c_str());
		}

		int ret = invokerIO->sendData(CMD_EVNOTIFY_REQ, ++m_seqid, msg.c_str(), msg.length(), true);
		LOGDEBUG("NOTIFYINVOKER| msg=notify provider_down to %d| ret=%d| msg=%s",
				 invokerCli->getIntProperty(CONNTERID_KEY), ret, msg.c_str());
	}
}

/** 服务提供者注册服务
 * request: by CMD_SVRREGISTER_REQ CMD_SVRREGISTER2_REQ
 * format: { "regname": "app1", "svrid": 100, 
 * 		"svrprop": {"prvdid":1, "url": "tcp://x", ..} }
 * remark: 客户应用上报不需要带svrid
 **/
int ProviderMgr::OnCMD_SVRREGISTER_REQ( void* ptr, unsigned cmdid, void* param )
{
	context_t ctx(ptr, param);
	if (!ctx.isValid())
		throw NormalExceptionOn(404, cmdid | CMDID_MID, ctx.seqid(), "body json invalid " __FILE__);
	std::string regname = ctx.obj().value("regname", "");
	NormalExceptionOn_IFTRUE(regname.empty(), 400, CMD_SVRREGISTER_RSP, ctx.seqid(), "leak of regname");

	int ret = 0;
	int svrid;
	CliBase* cli = NULL;

	if (ctx.obj().find(CONNTERID_KEY) != ctx.obj().end()) // 来自其他Peer-Serv的广播
	{
		svrid = ctx.obj().at(CONNTERID_KEY);
		cli = CliMgr::Instance()->getChildBySvrid(svrid);
		NormalExceptionOn_IFTRUE(NULL == cli, 400, CMD_SVRREGISTER_RSP, ctx.seqid(), _F("invalid svrid %d", svrid));
	}
	else // 来自客户应用的上报
	{
		cli = ctx.io();
		svrid = ctx.io()->getIntProperty(CONNTERID_KEY);
		ctx.obj()[CONNTERID_KEY] = svrid;
		BroadCastCli::Instance()->toWorld(ctx.obj(), CMD_SVRREGISTER2_REQ, ++this->m_seqid, false);
	}

	auto prop = ctx.obj().at(SVRREG_PROP_KEY);
	int prvdid = prop.value("prvdid", 0);

	std::string regname2 = _F("%s%s%%%d", SVRPROP_PREFIX, regname.c_str(), prvdid);

	int urlChange = CheckValidUrlProtocol(cli, prop, regname2, ctx.seqid());
	int enableBeforValue = cli->getIntProperty(regname2 + ":enable");
	ret = setProviderProperty(cli, prop, regname2);

	int enableAfterValue = cli->getIntProperty(regname2 + ":enable");
	bool enableChange = (1 == enableBeforValue && 0 == enableAfterValue);
	bool isNewPrvd = !this->hasProviderItem(cli, regname, prvdid);

	if (0 == ret && (urlChange || enableChange))  // 禁用服务时触发
	{
		this->notify2Invoker(regname, svrid, prvdid);
	}

	this->updateProvider(cli, regname, prvdid);
	nlohmann::json respobj{
		{"code", 0},
		{"desc", std::string("reg ") + regname2 + "result " + std::to_string(ret)}
	};
	std::string resp = respobj.dump();
	ctx.io()->sendData(CMD_SVRREGISTER_RSP, ctx.seqid(), resp.c_str(), resp.length(), true);

	if (isNewPrvd)
	{
		Actmgr::Instance()->appendCliOpLog(_F("PRVDREG| regname=%s| opcli=%s| reqmsg=",
			regname2.c_str(), cli->m_idProfile.c_str()) + ctx.obj().dump());
	}

	return ret;
}

void ProviderMgr::updateProvider( CliBase* cli,  const std::string& regname, int prvdid)
{
	ServiceProvider* provider = m_providers[regname];
	if (NULL == provider)
	{
		provider = new ServiceProvider(regname);
		m_providers[regname] = provider;
	}

	provider->setItem(cli, prvdid);
}

// url&protocol合法性检查
int ProviderMgr::CheckValidUrlProtocol(CliBase* cli, const nlohmann::json& prop, const std::string& regname2, unsigned seqid)
{
	std::string url0 = cli->getProperty(regname2 + ":url");
	std::string url1 = prop.value("url", url0);
	int protocol1 = prop.value("protocol", cli->getIntProperty(regname2 + ":protocol"));

	if (!url1.empty() && 0 != protocol1)
	{
		static const int protocolLen = 4;
		static const std::string urlPrefix[protocolLen + 1] = { "tcp", "udp", "http", "https", "x" };
		NormalExceptionOn_IFTRUE(protocol1 > protocolLen || protocol1 <= 0, 400,
			CMD_SVRREGISTER_RSP, seqid, _F("invalid protocol %d", protocol1));
		NormalExceptionOn_IFTRUE(0 != url1.find(urlPrefix[protocol1 - 1]), 400,
			CMD_SVRREGISTER_RSP, seqid, "url protocol not match");
	}

	return (url0 != url1);
}

int ProviderMgr::setProviderProperty(CliBase* cli, const nlohmann::json &prop, const std::string& regname2)
{
	for (const auto& [key, value] : prop.items())
		cli->setProperty(regname2 + ":" + key, value.dump());
	return 0;
}

// format: {"regname": "app1", version: 1, idc: 1, rack: 2, bookchange: 1 }
int ProviderMgr::OnCMD_SVRSEARCH_REQ( void* ptr, unsigned cmdid, void* param )
{
	context_t ctx(ptr, param);
	if (!ctx.isValid())
		throw NormalExceptionOn(404, cmdid | CMDID_MID, ctx.seqid(), "body json invalid " __FILE__);
	std::string regname = ctx.obj().value("regname", "");
	NormalExceptionOn_IFTRUE(regname.empty(), 400, CMD_SVRSEARCH_RSP, ctx.seqid(), "leak of regname");
	int version = ctx.obj().value("version", 0);
	int idc = ctx.obj().value("idc", 0);
	int rack = ctx.obj().value("rack", 0);
	int bookchange = ctx.obj().value("bookchange", 0);
	int limit = ctx.obj().value("limit", 4);

	if (0 == idc)
	{
		idc = ctx.io()->getIntProperty("idc");
		rack = ctx.io()->getIntProperty("rack");
	}

	auto vec = getOneProviderJson(regname, idc, rack, version, limit);
	nlohmann::json obj{
		{"data", vec},
		{"count", (int)vec.size()},
		{"desc", _F("total resp %d providers", vec.size())},
		{"code", 0}
	};
	std::string resp = obj.dump();

	if (bookchange) // 订阅改变事件（cli下线）
	{
		std::string alias = std::string(SVRBOOKCH_ALIAS_PREFIX) + "_" + regname + "@" + ctx.io()->getProperty(CONNTERID_KEY);
		CliMgr::Instance()->addAlias2Child(alias, ctx.io());
	}

	return ctx.io()->sendData(CMD_SVRSEARCH_RSP, ctx.seqid(), resp.c_str(), resp.length(), true);
}

// format: {"regname": "all"}
int ProviderMgr::OnCMD_SVRSHOW_REQ( void* ptr, unsigned cmdid, void* param )
{
	context_t ctx(ptr, param);
	if (!ctx.isValid())
		throw NormalExceptionOn(404, cmdid | CMDID_MID, ctx.seqid(), "body json invalid " __FILE__);
	std::string regname = ctx.obj().value("regname", "");
	//RJSON_GETINT_D(onlyname, &doc);

	bool ball = (regname.empty() || "all" == regname);
	auto vec = ball ? getAllJson() : getOneProviderJson(regname);
	nlohmann::json obj{
		{"data", vec},
		{"len", (int)vec.size()}
	};
	std::string resp = obj.dump();
	return ctx.io()->sendData(CMD_SVRSHOW_RSP, ctx.seqid(), resp.c_str(), resp.length(), true);
}

// format: [{"prvdid": 1, "pvd_ok": 10}]
int ProviderMgr::OnCMD_SVRSTAT_REQ( void* ptr, unsigned cmdid, void* param )
{
	MSGHANDLE_PARSEHEAD(false);
	int i = 0;
	const Value* svrItm = NULL;

	for (; 0 == Rjson::GetObject(&svrItm, i, &doc); ++i )
	{
		RJSON_GETSTR_D(regname, svrItm);
		RJSON_VGETINT_D(svrid, CONNTERID_KEY, svrItm); 
		RJSON_GETINT_D(prvdid, svrItm);
		RJSON_GETINT_D(pvd_ok, svrItm);
		RJSON_GETINT_D(pvd_ng, svrItm);
		RJSON_GETINT_D(ivk_ok, svrItm);
		RJSON_GETINT_D(ivk_ng, svrItm);
		RJSON_GETINT_D(ivk_dok, svrItm);
		RJSON_GETINT_D(ivk_dng, svrItm);

		if (regname.empty()) continue;
		if ((ivk_dok > 0 || ivk_dng) && 0 == svrid)
		{
			throw NormalExceptionOn(400, CMD_SVRSTAT_RSP, seqid, 
				"leak of svrid on ivk_ok or ivk_ng");
		}

		if (pvd_ok || pvd_ng || ivk_dok || ivk_dng)
		{
			CliBase* provider = iohand;
			if (svrid > 0)
			{
				provider = CliMgr::Instance()->getChildBySvrid(svrid);
				if (NULL == provider) continue;
			}

			ServiceProvider* svrPrvder = this->getProviderPtr(regname);
			if (svrPrvder)
			{
				svrPrvder->setStat(provider, prvdid, pvd_ok, pvd_ng, ivk_dok, ivk_dng);
			}			
		}

		if (ivk_ok || ivk_ng) // 设置自身作为消费者时自身调用服务的统计信息
		{
			// 注意：这里的prvdid非自身（iohand）的，而是被调者的
			std::string regname2 = _F("%s%s%%%d-%d", SVRPROP_PREFIX, regname.c_str(), svrid, 0);
			if (ivk_ok) iohand->setProperty(regname2 + ":ivk_ok", ivk_ok);
			if (ivk_ng) iohand->setProperty(regname2 + ":ivk_ng", ivk_ng);
		}
	}

	return 0;
}


ServiceProvider* ProviderMgr::getProviderPtr( const std::string& regname ) const
{
	ServiceProvider* ret = NULL;
	auto itr = m_providers.find(regname);
	if (itr != m_providers.end())
	{
		ret = itr->second;
	}

	return ret;
}

bool ProviderMgr::hasProviderItem( CliBase* cli, const std::string& regname, int prvdid ) const
{
	bool ret = false;
	ServiceProvider* prvd = getProviderPtr(regname);
	if (prvd)
	{
		ret = prvd->hasItem(cli, prvdid);
	}

	return ret;
}

// return provider个数
nlohmann::json ProviderMgr::getAllJson() const
{
	nlohmann::json obj;
	for (auto itr = m_providers.begin(); itr != m_providers.end(); ++itr)
		obj[itr->first] = itr->second->getAllJson();
	return obj;
}

// return item个数
nlohmann::json ProviderMgr::getOneProviderJson(const std::string& regname) const
{
	auto itr = m_providers.find(regname);
	if (itr != m_providers.end())
		return { itr->first, itr->second->getAllJson() };
	return {};
}

// return item个数
std::vector<nlohmann::json> ProviderMgr::getOneProviderJson(const std::string& regname, short idc, short rack, short version, short limit) const
{
	auto itr = m_providers.find(regname);
	if (itr != m_providers.end() && limit > 0)
	{
		return itr->second->query(idc, rack, version, limit);
	}
	else
	{
		return {};
	}
}
