#include "act_mgr.h"
#include "comm/strparse.h"
#include "comm/timef.h"
#include "cloud/exception.h"
#include "cloud/const.h"
#include "cloud/homacro.h"
#include "clibase.h"
#include "climanage.h"

HEPCLASS_IMPL_FUNCX_BEG(Actmgr)
HEPCLASS_IMPL_FUNCX_END(Actmgr)

Actmgr::Actmgr(void)
{
	m_opLogSize = CLIOPLOG_SIZE;
	m_pchildren = CliMgr::Instance()->getAllChild();
	HEPCLASS_IMPL_FUNCX_MORE(Actmgr, NotifyCatch)
}
Actmgr::~Actmgr(void)
{
}

std::vector<nlohmann::json> Actmgr::pickupWarnCliProfile(const std::string& filter_key, const std::string& filter_val)
{
	std::vector<nlohmann::json> result;

	for (auto itr = m_warnLog.begin(); itr != m_warnLog.end(); )
	{
		CliBase* ptr = itr->second;

		if (m_pchildren->find(ptr) == m_pchildren->end()) // 被清除了的历史session
		{
			auto itr0 = itr; ++itr;
			m_warnLog.erase(itr0);
			continue;
		}

		if (!filter_key.empty()) // 需要过滤条件
		{
			std::string valdata = ptr->getProperty(filter_key);
			if (!filter_val.empty() && valdata != filter_val)
			{
				++itr;
				continue;
			}
		}

		result.push_back(getJsonProp(ptr, ""));
		++itr;
	}

	return result;
}

// @summery: 获取客户端的属性信息
// @param: svrid 为0时获取所有,否则单个
// @param: key 为空时获取所有属性
// @return 获取到的cli个数
std::vector<nlohmann::json> Actmgr::pickupCliProfile(int svrid, const std::string& key)
{
	std::vector<nlohmann::json> result;
	if (0 == svrid)
	{
		for (auto itr = m_pchildren->begin(); itr != m_pchildren->end();  ++itr)
			result.push_back(getJsonProp(itr->first, key));
	}
	else
	{
		CliBase* ptr = CliMgr::Instance()->getChildBySvrid(svrid);
		if (ptr)
			result.push_back(getJsonProp(ptr, key));
	}

	return result;
}
// 获取某个app的一项或全部属性，以json字符串返回
nlohmann::json Actmgr::getJsonProp(CliBase* cli, const std::string& key)
{
	nlohmann::json obj;
	if (key.empty())
	{
		for (auto &[key, value] : cli->m_cliProp)
			obj[key] = value;
	}
	else
	{
		auto itr = cli->m_cliProp.find(key);
		if (itr != cli->m_cliProp.end())
			obj[itr->first] = itr->second;
	}
	return obj;
}

void Actmgr::setCloseLog( int svrid, const std::string& cloLog )
{
	if (cloLog.empty())
	{
		m_closeLog.erase(svrid);
	}
	else 
	{
		m_closeLog[svrid] = cloLog;
	}
}

void  Actmgr::rmCloseLog( int svrid )
{
	if (svrid > 0) m_closeLog.erase(svrid);
	else m_closeLog.clear();
}

int Actmgr::appCloseFound( CliBase* son, int clitype, const CliInfo& cliinfo )
{
	int ret = -100;
	IFRETURN_N(NULL==son, ret);

	std::string& strsvrid = son->m_cliProp[CONNTERID_KEY];
	if (1 == clitype)
	{
		int svrid = atoi(strsvrid.c_str());
		nlohmann::json obj {
			{CONNTERID_KEY, strsvrid},
			{"name", son->m_cliProp["name"]},
			{SVRNAME_KEY, son->m_cliProp[SVRNAME_KEY]},
			{"shell", son->m_cliProp["shell"]},
			{"progi", son->m_idProfile},
			{"begin_time", TimeF::StrFTime("%F %T", cliinfo.t0)},
			{"end_time", TimeF::StrFTime("%F %T", cliinfo.t2) }
		};
		m_closeLog[svrid] = obj.dump();
	}

	// 记录操作
	std::string logstr = StrParse::Format("CLIENT_CLOSE| clitype=%d| svrid=%s| prog=%s| localsock=%s",
		clitype, strsvrid.c_str(), son->m_idProfile.c_str(), son->m_cliProp["name"].c_str());
	appendCliOpLog(logstr);

	ret = 0;

	return ret;
}

void Actmgr::appendCliOpLog( const std::string& logstr )
{
	std::string stritem = TimeF::StrFTime("%F %T", time(NULL));
	int i = 0;

	stritem.append(": ");
	for (const char* ch = logstr.c_str(); 0 != ch[i]; ++i ) // 去除引号
	{
		stritem.append(1, '\"' == ch[i]? ' ' : ch[i]);
	}

	m_cliOpLog.push_back(stritem);
	if ((int)m_cliOpLog.size() > m_opLogSize)
	{
		m_cliOpLog.pop_front(); 
	}
}

// 获取已掉线的客户信息
std::vector<std::string> Actmgr::pickupCliCloseLog()
{
	std::vector<std::string> result;
	for (auto itr = m_closeLog.begin(); itr != m_closeLog.end(); ++itr)
		result.push_back(itr->second);
	return result;
}

// 获取客户行为日志信息
std::vector<std::string> Actmgr::pickupCliOpLog(int nSize)
{
	std::vector<std::string> result;
	auto itr = m_cliOpLog.rbegin();
	for (int i = 0; i < nSize && itr != m_cliOpLog.rend(); ++itr, ++i)
		result.push_back(*itr);
	return result;
}

void Actmgr::setWarnMsg( const std::string& taskkey, CliBase* ptr )
{
	m_warnLog[taskkey] = ptr;
}

void Actmgr::clearWarnMsg( const std::string& taskkey )
{
	if (0 == taskkey.compare("all"))
	{
		m_warnLog.clear();
	}
	else
	{
		m_warnLog.erase(taskkey);
	}
	
}

int Actmgr::NotifyCatch( void* ptr, unsigned cmdid, void* param )
{
	MSGHANDLE_PARSEHEAD(true)
	RJSON_GETSTR_D(notify, &doc);
	RJSON_VGETINT_D(svrid, CONNTERID_KEY, &doc);
	RJSON_VGETINT_D(to, ROUTE_MSG_KEY_TO, &doc);

	appendCliOpLog( _F("NOTIFY| notify=%s| target=%d| reqmsg=%s",
			notify.c_str(), to>0? to: svrid, body) );

	return 1;
}
