#include <set>
#include "broadcastcli.h"
#include "cloud/const.h"
#include "comm/strparse.h"
#include "cloud/homacro.h"
#include "iohand.h"
#include "cloud/switchhand.h"
#include "climanage.h"
#include "outer_cli.h"
#include "outer_serv.h"
#include "cloud/exception.h"
#include "hocfg_mgr.h"
#include "provider_manage.h"
#include "comm/json.hpp"

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
	nlohmann::json& obj() { return obj_; }
	IOHand* io() { return iohand_; }
};

HEPCLASS_IMPL_FUNCX_BEG(BroadCastCli)
HEPCLASS_IMPL_FUNCX_MORE_S(BroadCastCli, TransToAllPeer)
HEPCLASS_IMPL_FUNCX_MORE_S(BroadCastCli, OnBroadCMD)
HEPCLASS_IMPL_FUNCX_END(BroadCastCli)

#define RouteExException_IFTRUE_EASY(cond, resonstr) \
    RouteExException_IFTRUE(cond, cmdid, seqid, s_my_svrid, from, resonstr, actpath)
#if 0
#define DEBUG_TRACE(fmt, ...) StrParse::AppendFormat(s_debugTrace, fmt , ##__VA_ARGS__);s_debugTrace+="\n"; printf(">>> " fmt "\n", ##__VA_ARGS__)
#define DEBUG_PRINT printf(s_debugTrace.c_str()); s_debugTrace.clear()
#else
#define DEBUG_TRACE(fmt, ...) do{}while(0);
#define DEBUG_PRINT
#endif

int BroadCastCli::s_my_svrid = 0;
string BroadCastCli::s_debugTrace;

BroadCastCli::BroadCastCli()
{

}

void BroadCastCli::init( int my_svrid )
{
    const int broadcast_delay_s = 5+rand()%20;
    m_seqid = 0;
    s_my_svrid = my_svrid;
    
    SwitchHand::Instance()->appendQTask(this, broadcast_delay_s*1000);
    DEBUG_TRACE("1. init at Servid=%d", my_svrid);
}

int BroadCastCli::run(int p1, long p2)
{
    return -1;
}

int BroadCastCli::qrun( int flag, long p2 )
{
    int ret = 0;
    if (HEFG_PEXIT != flag)
    {
        string localEra = CliMgr::Instance()->getLocalClisEraString(); // 获取本地cli的当前版本
        string hocfgEraJson = HocfgMgr::Instance()->getAllCfgNameJson().dump();

        DEBUG_TRACE("2. broadcast self cliera out, local_erastr=%s", localEra.c_str());
        //if (!localEra.empty())
        {
            //ret = toWorld(s_my_svrid, 1, localEra, " ", "");
            nlohmann::json obj {
		{CLIS_ERASTRING_KEY, localEra},
		{HOCFG_ERASTRING_KEY, HocfgMgr::Instance()->getAllCfgNameJson()}
            };
            std::string reqmsg = obj.dump();
            ret = toWorld(reqmsg, CMD_BROADCAST_REQ, ++m_seqid, false);
        }

        SwitchHand::Instance()->appendQTask(this, BROADCASTCLI_INTERVAL_SEC*1000);
    }
    
    return ret;
}


int BroadCastCli::TransToAllPeer( void* ptr, unsigned cmdid, void* param )
{
	context_t ctx(ptr, param);
	if (!ctx.isValid())
		throw NormalExceptionOn(404, cmdid | CMDID_MID, ctx.seqid(), ctx.io()->m_idProfile + " body json invalid");

	bool includeCli = (ctx.obj().find(BROARDCAST_KEY_CLIS) != ctx.obj().end());
	return 0 == BroadCastCli::Instance()->toWorld(ctx.obj(), cmdid, ctx.seqid(), includeCli) ? 1 : 0;
}

/**
 * @summery: 包体为json的消息广播
 * @desc: 必须包含的字段 {"from": 123, "pass": " 2 3 ", "path": "1>3>", "jump": 1}
 */
int BroadCastCli::toWorld(nlohmann::json& obj, unsigned cmdid, unsigned seqid, bool includeCli)
{
	int ret = 0;
	static const string str_my_svrid = std::to_string(s_my_svrid);
	static const string str_my_svrid1 = _F("%d ", s_my_svrid);
	static const string str_my_svrid2 = _F(" %d ", s_my_svrid);

	if (!obj.is_object()) throw NormalExceptionOn(404, cmdid | CMDID_MID, seqid, "msgbody must be Object{}");
	// 处理发出源from的svrid
	int ofrom = obj.value(BROARDCAST_KEY_FROM, s_my_svrid);
	// 处理"已经走过的节点"
	std::string haspass = obj.value(BROARDCAST_KEY_PASS, "");
	if (haspass.find(str_my_svrid2) == std::string::npos)
		haspass += str_my_svrid1;

	CliMgr::AliasCursor alcr(SERV_IN_ALIAS_PREFIX);
	CliBase *cli = NULL;
	vector<CliBase*> vecCli;
	while ((cli = alcr.pop()))
	{
		string svridstr = cli->getProperty(CONNTERID_KEY);
		if (haspass.find(string(" ") + svridstr + " ") == string::npos)
		{
			haspass += svridstr + " ";
			vecCli.push_back(cli);
		}
	}

	IFRETURN_N(vecCli.empty(), 0);

	obj[BROARDCAST_KEY_PASS] = haspass;
	obj[BROARDCAST_KEY_TRAIL] = obj.value(BROARDCAST_KEY_TRAIL, "") + str_my_svrid + ">";
	// 跳数处理
	obj[BROARDCAST_KEY_JUMP] = obj.value(BROARDCAST_KEY_JUMP, 0) + 1;

	if (includeCli && obj.find(BROARDCAST_KEY_CLIS) == obj.end())
	{
		obj[BROARDCAST_KEY_CLIS] = "";
	}

	string reqbody = obj.dump();
	for (unsigned i = 0; i < vecCli.size(); ++i)
	{
		IOHand* cli = dynamic_cast<IOHand *>(vecCli[i]);
		if (cli)
		{
			cli->sendData(cmdid, seqid, reqbody.c_str(), reqbody.length(), true);
			DEBUG_TRACE("3/b. base-%d sendto pass [%s] body is %s", ofrom, cli->m_idProfile.c_str(), reqbody.c_str());
		}
		else
		{
		}
	}
	if (includeCli)
	{
		std::string clifilter = obj.at(BROARDCAST_KEY_CLIS);
		ret = toAllLocalCli(cmdid, seqid, reqbody, clifilter);
	}

	return ret;
}

// clifilter非空时过滤只发送给符合条件的cli
// clifilter format: key=val1
int BroadCastCli::toAllLocalCli( unsigned cmdid, unsigned seqid, const string& msg, const string& clifilter )
{
    CliMgr::AliasCursor alcr(INNERCLI_ALIAS_PREFIX); // 查找出所有直连的Serv节点
    CliBase *cli = NULL;

    int count = 0;
    string filter_key, filter_value;
    bool filter_enable = clifilter.empty();

    if (filter_enable)
    {
        vector<string> vfilter;
        StrParse::SpliteStr(vfilter, clifilter, '=');
    }

    vector<CliBase*> vecCli;
    while ((cli = alcr.pop()))
    {
        IOHand* iocli = dynamic_cast<IOHand *>(cli);
        if (NULL == iocli)
        {
            LOGERROR("BROADCASTCLI| msg=found no IOHand cli| cli=%s| ptr=%p", cli->m_idProfile.c_str(), cli);
            continue;
        }
        if (!filter_enable)
        {
            iocli->sendData(cmdid, seqid, msg.c_str(), msg.length(), true);
            ++count;
        }
        else
        {
            if (cli->getProperty(filter_key) == filter_value)
            {
                iocli->sendData(cmdid, seqid, msg.c_str(), msg.length(), true);
                ++count;
            }
        }
    }

    LOGDEBUG("BROADCASTCLI| msg=has send to %d cli| cmd=0x%x| seqid=%u", count, cmdid, seqid);
    return 0;
}

int BroadCastCli::toWorld( const string& jsonmsg, unsigned cmdid, unsigned seqid, bool incCli )
{
	auto obj = nlohmann::json::parse(jsonmsg, nullptr, false);
	if (obj.is_discarded())
		throw NormalExceptionOn(404, cmdid | CMDID_MID, seqid, "jsonmsg json invalid");

	if (0 == seqid) seqid = ++m_seqid;
	return toWorld(obj, cmdid, seqid, incCli);
}


int BroadCastCli::OnBroadCMD( void* ptr, unsigned cmdid, void* param )
{
	context_t ctx(ptr, param);
	if (!ctx.isValid())
		throw NormalExceptionOn(404, cmdid | CMDID_MID, ctx.seqid(), "body json invalid " __FILE__);
	if (cmdid == CMD_BROADCAST_REQ) {
		return on_CMD_BROADCAST_REQ(ctx.io(), ctx.obj(), ctx.seqid());
	}
	else if (cmdid == CMD_CLIERA_REQ) {
		return on_CMD_CLIERA_REQ(ctx.io(), ctx.obj(), ctx.seqid());
	}
	else if (cmdid == CMD_CLIERA_RSP) {
		return on_CMD_CLIERA_RSP(ctx.io(), ctx.obj(), ctx.seqid());
	}
	else if (cmdid == CMD_UPDATEERA_REQ) {
		return on_CMD_UPDATEERA_REQ(ctx.io(), ctx.obj(), ctx.seqid());
	}
	return -5;
}

// @summery: 比较外面广播过来的erastr和本地outerCli，返回不同的cli编号
string BroadCastCli::diffOuterCliEra( int servid, const string& erastr )
{
    CliMgr::AliasCursor finder(_F("%s_%d_", OUTERCLI_ALIAS_PREFIX, servid)); 
    if (finder.empty() && erastr.empty())
    {
        return "";
    }

	string ret;
	vector<string> vecitem;
    std::set<CliBase*> appset;
    CliBase* ptrtmp = NULL;
    bool retall = (finder.empty() && !erastr.empty());

    while ( (ptrtmp = finder.pop()) )
    {
        appset.insert(ptrtmp); // 先收集所有的属于连接servid的appobj, 之后用于删除未上报的
    }

	StrParse::SpliteStr(vecitem, erastr, ' ');
	for (auto itim = vecitem.begin(); itim != vecitem.end(); ++itim)
	{
		vector<string> vecsvr;
		StrParse::SpliteStr(vecsvr, *itim, ':');
		if (3 == vecsvr.size())
		{
			string& appid = vecsvr[0];
			int svrera = atoi(vecsvr[1].c_str());
			//int svratime = atoi(vecsvr[2].c_str());

			CliBase* outcli = CliMgr::Instance()->getChildByName(appid+"_i");
			if (outcli)
			{
				CliMgr::Instance()->updateCliTime(outcli);
                appset.erase(outcli);
				if (outcli->m_era == svrera) continue;
			}
            else
            {
                OuterCli* optr = new OuterCli;
                optr->init(servid);
                optr->setProperty(CONNTERID_KEY, appid);
                
                int ret = CliMgr::Instance()->addChild(optr);
                ret |= CliMgr::Instance()->addAlias2Child(appid + "_i", optr);
                ret |= CliMgr::Instance()->addAlias2Child(_F("%s_%d_%s", OUTERCLI_ALIAS_PREFIX, servid, appid.c_str()), optr);
                if (ret) // fail
                {
                    LOGERROR("CLIERA_RSP| msg=add child to CliMgr fail| svrid=%d| ret=%d", servid, ret);
                    CliMgr::Instance()->removeAliasChild(optr, true);
                }
				CliMgr::Instance()->updateCliTime(optr);
            }

			ret += *itim + " ";
		}
	}

    auto itr = appset.begin();
    for (; itr != appset.end(); ++itr) // 未上报的执行清除
    {
        CliBase* itptr = *itr;
        CliMgr::Instance()->removeAliasChild(itptr, true);
    }

	return retall? "all": ret;
}

int BroadCastCli::on_CMD_BROADCAST_REQ(IOHand* iohand, const nlohmann::json& obj, unsigned seqid)
{
    int ret;
    int osvrid = 0;
    string erastr;
    string str_osvrid;
    string excludeSvrid;
    string routepath;

    try {
        osvrid = obj.at(BROARDCAST_KEY_FROM);
        if (osvrid < 0) throw;
        erastr = obj.at(CLIS_ERASTRING_KEY);
        routepath = obj.at(BROARDCAST_KEY_TRAIL);
    }
    catch (...) {
        throw NormalExceptionOn(409, CMD_BROADCAST_RSP, seqid, string("invalid CMD_BROADCAST_REQ body ") + obj.dump());
    }

    str_osvrid = std::to_string(osvrid);

    DEBUG_TRACE("a. recv svrid=%d broadcast pass by %s, msg=%s", osvrid, iohand->m_idProfile.c_str(), Rjson::ToString(doc).c_str());
    routepath += std::to_string(s_my_svrid);

    // 自身处理
    // 1. 本Serv是否存在编号为osvrid的对象
    string near_servid = iohand->getProperty(CONNTERID_KEY); // 当前直接传的消息的serv编号
    CliBase* servptr = CliMgr::Instance()->getChildBySvrid(osvrid);
    string old_era;
    bool needall = false;

    if (NULL == servptr) // 来自一个新的Serv上报
    {
        NormalExceptionOn_IFTRUE(near_servid==str_osvrid, 410, CMD_BROADCAST_RSP,
            seqid, StrParse::Format("near serv-%s must not outer serv-%d", 
            near_servid.c_str(), osvrid)); // assert

        OuterServ* outsvr = new OuterServ;
        outsvr->init(osvrid);
        ret = outsvr->setRoutePath(routepath);
        ret |= CliMgr::Instance()->addChild(outsvr);
        ret |= CliMgr::Instance()->addAlias2Child(string(SERV_OUT_ALIAS_PREFIX) + str_osvrid + "s", outsvr);
        ret |= CliMgr::Instance()->addAlias2Child(str_osvrid + "_s", outsvr);
        CliMgr::Instance()->updateCliTime(outsvr);
        needall = !erastr.empty();

        if (ret)
        {
            CliMgr::Instance()->removeAliasChild(outsvr, true);
            throw NormalExceptionOn(411, CMD_BROADCAST_RSP, seqid,
                StrParse::Format("%s:%d addChild ret %d", __FILE__, __LINE__, ret));
        }

        // update atime

        servptr = outsvr;
        DEBUG_TRACE("c1. new OuterServ(%d)", osvrid);
    }
    else
    // 2. 编号为osvrid的对象的era是否最新，否则请求获取
    {
        if (servptr->isLocal()) // 直连的Serv发出的广播
        {
            NormalExceptionOn_IFTRUE(near_servid!=str_osvrid, 412, CMD_BROADCAST_RSP,
                seqid, StrParse::Format("first broadcast serv-%d should be near serv-%s ", 
                osvrid, near_servid.c_str())); // assert

            old_era = iohand->getProperty(CLIS_ERASTRING_KEY);
            DEBUG_TRACE("c2. near Serv(%d)", osvrid);
        }
        else
        {
            OuterServ* outsvr = dynamic_cast<OuterServ*>(servptr);
            NormalExceptionOn_IFTRUE(NULL==outsvr, 413, CMD_BROADCAST_RSP,
                seqid, StrParse::Format("servptr-%s isnot OuterServ instance ", 
                servptr->m_idProfile.c_str())); // assert

            old_era = outsvr->getProperty(CLIS_ERASTRING_KEY);
            CliMgr::Instance()->updateCliTime(outsvr);

            // 更新路由信息
            ret = outsvr->setRoutePath(routepath);
            DEBUG_TRACE("c3. OuterServ(%d) second broadcast", osvrid);
            ERRLOG_IF1(ret, "SETROUTPATH| msg=route path fail| path=%s", routepath.c_str());
        }
    }

    int last_reqera_time = servptr->getIntProperty(LAST_REQ_SERVMTIME);
    int now = time(NULL);
    const int reqall_interval_sec = 10;

    // era新旧判断
    string differa = needall ? "all" : diffOuterCliEra(osvrid, erastr);
    if (!differa.empty() && now > last_reqera_time + reqall_interval_sec)
    {
        // 请求获取某Serv下的所有cli (CMD_CLIERA_REQ)
        nlohmann::json obj {
		{ROUTE_MSG_KEY_TO, osvrid},
		{ROUTE_MSG_KEY_FROM, s_my_svrid},
		{"differa", differa},
		{ROUTE_MSG_KEY_JUMP, 1},
		{ROUTE_MSG_KEY_REFPATH, routepath},
		{ROUTE_MSG_KEY_TRAIL, std::to_string(s_my_svrid) + ">"}
        };
        std::string msgbody = obj.dump();

        ret = iohand->sendData(CMD_CLIERA_REQ, seqid, msgbody.c_str(), msgbody.size(), true);
        servptr->setIntProperty(LAST_REQ_SERVMTIME, now);
        LOGDEBUG("REQERAALL| msg=Serv-%d need %d eraall data| erastr=%s->%s| differa=%s| refpath=%s| retsend=%d",
            s_my_svrid, osvrid, old_era.c_str(), erastr.c_str(), differa.c_str(), routepath.c_str(), ret);
        DEBUG_TRACE("d. req %d-serv's cli: %s", osvrid, msgbody.c_str());
    }

    if (obj.find(HOCFG_ERASTRING_KEY) != obj.end())
    {
        ret = HocfgMgr::Instance()->compareServHoCfg(osvrid, obj.at(HOCFG_ERASTRING_KEY));
    }

    return ret;
}

/**
 * 协议格式:
 * {"to": 123, "from": 234, "differa": "xxx", "refer_path": "xxx", "path": "xx"}
 *
**/
int BroadCastCli::on_CMD_CLIERA_REQ(IOHand* iohand, const nlohmann::json& obj, unsigned seqid)
{
    int ret = 0;
    int from = 0;
    int to = 0;
    const int cmdid = CMD_CLIERA_REQ;
    string differa;
    string refpath;
    string actpath;

    try {
        to = obj.at(ROUTE_MSG_KEY_TO);
        from = obj.at(ROUTE_MSG_KEY_FROM);
        differa = obj.at("differa");
        refpath = obj.at(ROUTE_MSG_KEY_REFPATH);
        actpath = obj.at(ROUTE_MSG_KEY_TRAIL);
    }
    catch (...)
    {
        throw RouteExException(cmdid, seqid, s_my_svrid, to,
              string("leak of param ") + obj.dump(), actpath);
    }

    RouteExException_IFTRUE_EASY(to != s_my_svrid, string("dst svrid notmatch ") + obj.dump());
    DEBUG_TRACE("4. recv %d-serv need CLIERA: %s", from, Rjson::ToString(doc).c_str());

    bool bAll = (0 == differa.compare("all"));
    nlohmann::json obj1 {
	{ROUTE_MSG_KEY_TO, from},
	{ROUTE_MSG_KEY_FROM, s_my_svrid},
	{ROUTE_MSG_KEY_REFPATH, refpath},
	{ROUTE_MSG_KEY_TRAIL, StrParse::Format("%d>", s_my_svrid)},
	{"all", bAll ? 1 : 0},
	{ROUTE_MSG_KEY_JUMP, 1}
    };
    auto data_vec = bAll ? CliMgr::Instance()->getLocalAllCliJson() : CliMgr::Instance()->getLocalCliJsonByDiffera(differa);
    obj1["data"] = data_vec;
    obj1["datalen"] = (int)data_vec.size();
    std::string rspbody = obj1.dump();
    iohand->sendData(CMD_CLIERA_RSP, seqid, rspbody.c_str(), rspbody.size(), true);
    DEBUG_TRACE("5. resp to %s: %s", iohand->m_idProfile.c_str(), rspbody.c_str());
    DEBUG_PRINT;
    return ret;
}

/**
 * 协议格式:
 * {"to": 123, "from": 234, "refer_path": "xxx", "act_path": "xx", "data":[{cli1},{cli2}]}
**/
int BroadCastCli::on_CMD_CLIERA_RSP(IOHand* iohand, const nlohmann::json& obj, unsigned seqid)
{
    int ret = 0;
    int from = 0;
    nlohmann::json data;

    try {
        data = obj.at("data");
        if (data.empty()) throw;
        from = obj.at(ROUTE_MSG_KEY_FROM);
        if (from < 0) throw;
    }
    catch (...) {
	LOGERROR("CLIERA_RSP| msg=no data(%s)| my_svrid=%d", obj.dump().c_str(), s_my_svrid);
        return -30;
    }
    DEBUG_TRACE("e. get cliera resp: %s", obj.dump().c_str());

    return UpdateCliProps(data, from);
}

int BroadCastCli::UpdateCliProps(const nlohmann::json& objs, int from)
{
    int ret = 0;
    for (const auto &obj : objs)
    {
        if (obj.find(CONNTERID_KEY) == obj.end()) continue;
        std::string strappid = obj.at(CONNTERID_KEY);
        CliBase* cli = CliMgr::Instance()->getChildByName(strappid + "_i");
        if (cli)
        {
            ret = cli->unserialize(obj);
        }
        else
        {
            OuterCli* outcli = new OuterCli;
            outcli->init(from);
            outcli->unserialize(obj);
            ret = CliMgr::Instance()->addChild(outcli);
            ret |= CliMgr::Instance()->addAlias2Child(strappid + "_i", outcli);
            ret |= CliMgr::Instance()->addAlias2Child(_F("%s_%d_%s", OUTERCLI_ALIAS_PREFIX, from, strappid.c_str()), outcli);

            if (ret) // fail
            {
                 LOGERROR("CLIERA_RSP| msg=add child to CliMgr fail| svrid=%d| ret=%d", from, ret);
                 CliMgr::Instance()->removeAliasChild(outcli, true);
                 continue;
            }
            cli = outcli;
        }

        AfterUpdatePropsHandle(cli);
    }
    return ret;
}
// 客户（外部）属性改变的处理
// remark: svrreg="prvd_REGNAME%10+prvd_REGNAME2%11"
//         provname="prvd_REGNAME”
//         regName="REGNAME"; prvdid=10;
void BroadCastCli::AfterUpdatePropsHandle( CliBase* cli )
{
    // 服务注册处理
    string svrreg = cli->getProperty(SVRPROVIDER_CLI_KEY);
    if (!svrreg.empty())
    {
        vector<string> vprovName;
        StrParse::SpliteStr(vprovName, svrreg, '+');
        auto it = vprovName.begin();
        for (; it != vprovName.end(); ++it)
        {
            vector<string> vspli;
            StrParse::SpliteStr(vspli, *it, '%');
            if (vspli.empty()) continue;
            string& provname = vspli[0];
            if (provname.find(SVRPROP_PREFIX) != 0)
            {
                LOGERROR("SVRREG_UPDATEPROP| msg=provname invalid| svrreg=%s", svrreg.c_str());
                break;
            }
            
            string regName = provname.substr(sizeof(SVRPROP_PREFIX));
            int prvdid = vspli.size() > 1 ? atoi(vspli[1].c_str()) : 0;
            ProviderMgr::Instance()->updateProvider(cli, regName, prvdid);
        }
    }
}

string BroadCastCli::GetDebugTrace( void )
{
    return s_debugTrace;
}

/**
 * @summery: 主动广播推送本Serv内的Cli实时变化
 * @format: { from:12, to:23, path:"12>34>", jump: 2,
 *            down:[..], // 下线的svrid
 *            up:[..] }  // 在线的svrid
 **/
int BroadCastCli::on_CMD_UPDATEERA_REQ(IOHand* iohand, const nlohmann::json& obj, unsigned seqid)
{
	int ret = 0;

	if (obj.find(UPDATE_CLIPROP_DOWNKEY) != obj.end())
	{
		for (const int& tmpsvr : obj.at(UPDATE_CLIPROP_DOWNKEY))
		{
			CliBase* cliptr = CliMgr::Instance()->getChildBySvrid(tmpsvr);
			if (cliptr && !cliptr->isLocal())
			{
				CliMgr::Instance()->removeAliasChild(cliptr, true);
			}
		}
	}

	if (obj.find(UPDATE_CLIPROP_UPKEY) != obj.end())
	{
		int from = obj.value(BROARDCAST_KEY_FROM, 0);
		ret = UpdateCliProps(obj.at(UPDATE_CLIPROP_UPKEY), from);
	}
	return ret;
}

