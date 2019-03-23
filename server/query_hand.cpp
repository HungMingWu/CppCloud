#include "query_hand.h"
#include "cloud/iobuff.h"
#include "comm/strparse.h"
#include "cloud/const.h"
//#include "redis/redispooladmin.h"
#include "flowctrl.h"
#include "act_mgr.h"
#include "iohand.h"
#include "cloud/homacro.h"
#include "cloud/exception.h"
#include "broadcastcli.h"
#include "route_exchange.h"
#include "hocfg_mgr.h"
#include "climanage.h"
#include "comm/json.hpp"

HEPCLASS_IMPL_FUNCX_BEG(QueryHand)
HEPCLASS_IMPL_FUNCX_MORE_S(QueryHand, ProcessOne)
HEPCLASS_IMPL_FUNCX_END(QueryHand)

static const char g_resp_strbeg[] = "{ \"code\": 0, \"desc\": \"success\", \"data\": ";
static const nlohmann::json g_resp_strbeg_obj { {"code", 0}, {"desc", "success"}};
//const char s_app_cache_id[] = "3";
//const char s_scommid_key[] = "scomm_svrid_max";

QueryHand::QueryHand(void)
{
	
}
QueryHand::~QueryHand(void)
{
	
}

/**
 *  interface for HEpBase
int QueryHand::run( int flag, long p2 )
{
	return 0;
}

// 通告事件处理
int QueryHand::onEvent( int evtype, va_list ap )
{
	int ret = 0;
	if (HEPNTF_INIT_PARAM == evtype)
	{

	}
	
	return ret;
}
*/

int QueryHand::ProcessOne( void* ptr, unsigned cmdid, void* param )
{
	CMDID2FUNCALL_BEGIN
	CMDID2FUNCALL_CALL(CMD_GETCLI_REQ);
	CMDID2FUNCALL_CALL(CMD_GETLOGR_REQ);
	CMDID2FUNCALL_CALL(CMD_GETWARN_REQ);
	CMDID2FUNCALL_CALL(CMD_GETCONFIG_REQ);
	CMDID2FUNCALL_CALL(CMD_TESTING_REQ);
	CMDID2FUNCALL_CALL(CMD_EVNOTIFY_REQ);
	
	return 0;
}


// 适配整型和字符型
int QueryHand::getIntFromJson( const string& key, const Value* doc )
{
	int ret = 0;
	string strid;
	if (Rjson::GetStr(strid, key.c_str(), doc))
	{
		Rjson::GetInt(ret, key.c_str(), doc);
	}
	else
	{
		ret = atoi(strid.c_str());
	}

	return ret;
}


int QueryHand::on_CMD_GETCLI_REQ( IOHand* iohand, const Value* doc, unsigned seqid )
{
	int ret = 0;
	int svrid = 0;
	string dstSvrId;
	string dstKey;
	nlohmann::json resp;

	svrid = getIntFromJson(CONNTERID_KEY, doc);
	Rjson::GetStr(dstKey, "key", doc);


	std::vector<nlohmann::json> profiles = Actmgr::Instance()->pickupCliProfile(svrid, dstKey);

	if (svrid > 0 && profiles.empty()) // 获取不到指定某一应用
	{
		resp["code"] = 404;
		resp["desc"] = "no exit svrid=" + std::to_string(svrid);
	}
	else
	{
		resp = g_resp_strbeg_obj;
		resp["data"] = profiles;
		resp["count"] = (int)profiles.size();
	}
	std::string respstr = resp.dump();
	//ERRLOG_IF1(0==ret, "CMD_GETCLI_REQ| msg=pickupCliProfile fail %d| get_svrid=%d| key=%s", ret, svrid, dstKey.c_str());

	iohand->sendData(CMD_GETCLI_RSP, seqid, respstr.c_str(), respstr.length(), true);
	return ret;
}


int QueryHand::on_CMD_GETLOGR_REQ( IOHand* iohand, const Value* doc, unsigned seqid )
{
	int nsize = 10;
	Rjson::GetInt(nsize, "size", doc);
	auto outobj = g_resp_strbeg_obj;
	outobj["data"] = Actmgr::Instance()->pickupCliOpLog(nsize);
	std::string outstr = outobj.dump();
	
	return iohand->sendData(CMD_GETLOGR_RSP, seqid, outstr.c_str(), outstr.length(), true);
}

// request format: { "filter_key": "warn", "filter_val":"ng"}
int QueryHand::on_CMD_GETWARN_REQ( IOHand* iohand, const Value* doc, unsigned seqid )
{
	string filter_key;
	string filter_val;
	auto outobj = g_resp_strbeg_obj;

	Rjson::GetStr(filter_key, "filter_key", doc);
	Rjson::GetStr(filter_val, "filter_val", doc);
	outobj["data"] =  Actmgr::Instance()->pickupWarnCliProfile(filter_key, filter_val);
	std::string outstr = outobj.dump();
	int ret = iohand->sendData(CMD_GETWARN_RSP, seqid, outstr.c_str(), outstr.length(), true);
	return ret;
}

/** 
 * @format: { "file_pattern": "app1-dev.json", "key_pattern":"/", 
 * 		     "incbase": 1, "gt_mtime": 154000999  }
 * @file_pattern: 文件名
 * @key_pattern: 查询键 （可选）
 * @incbase: 包含继承文件 （可选）
 * @gt_mtime:  （可选）当大于此时间的情况下才响应配置信息，否则返回1
 **/ 
int QueryHand::on_CMD_GETCONFIG_REQ( IOHand* iohand, const Value* doc, unsigned seqid )
{
	RJSON_VGETINT_D(incbase, HOCFG_INCLUDEBASE_KEY, doc);
	RJSON_VGETINT_D(gt_mtime, HOCFG_GT_MTIME_KEY, doc);
	RJSON_VGETSTR_D(file_pattern, HOCFG_FILENAME_KEY, doc);
	RJSON_VGETSTR_D(key_pattern, HOCFG_KEYPATTEN_KEY, doc);

	string result;
	int ret;
	//bool rspIncMtime = doc.HasMember("gt_mtime");
	int curMtime = HocfgMgr::Instance()->getCfgMtime(file_pattern, incbase);
	if (0 == gt_mtime || curMtime > gt_mtime)
	{
		string strtmp;
		ret = HocfgMgr::Instance()->query(strtmp, file_pattern, key_pattern, incbase);
		nlohmann::json obj {
			{"code", 0},
			{"mtime", curMtime},
			{HOCFG_FILENAME_KEY, file_pattern},
			{"contents", strtmp}
		};
		result = obj.dump();;
	}
	else
	{
		result = (0 == curMtime)? "null" : "1";
	}

	ret = iohand->sendData(CMD_GETCONFIG_RSP, seqid, result.c_str(), result.length(), true);
	return ret;
}

/**
 * 调试接口
*/
int QueryHand::on_CMD_TESTING_REQ( IOHand* iohand, const Value* doc, unsigned seqid )
{
	string cmd;
	Rjson::GetStr(cmd, "cmd", doc);
	if (cmd == "broadshow")
	{
		string rsp = BroadCastCli::GetDebugTrace();
		iohand->sendData(CMD_TESTING_RSP, seqid, rsp.c_str(), rsp.length(), true);
	}
	else if (cmd == "routeto")
	{
		RJSON_GETINT_D(cmdid, doc);
		RJSON_GETINT_D(toSvr, doc);
		RJSON_GETINT_D(fromSvr, doc);
		RJSON_GETINT_D(bto, doc);
		fromSvr = iohand->getIntProperty(CONNTERID_KEY);
		
		int ret = RouteExchage::PostToCli("{\"cmd\": \"dbg\" }", CMD_TESTING_REQ, seqid, toSvr, fromSvr, bto);
		LOGDEBUG("TESTING_REQ| msg=test PostToCli %d->%d| ret=%d", fromSvr, toSvr, ret);
	}
	else if (cmd == "dbg")
	{
		RJSON_GETINT_D(from, doc);
		
		LOGDEBUG("TESTING_REQ| msg=dbg show| %s", Rjson::ToString(doc).c_str());
		int ret = RouteExchage::PostToCli("{\"cmd\": \"dbg-resp\" }", CMD_TESTING_RSP, seqid, from, 0, 0);
		LOGDEBUG("TESTING_REQ| msg=test PostToCli ?->%d| ret=%d", from, ret);
	}

	return 0;
}

int QueryHand::on_CMD_EVNOTIFY_REQ( IOHand* iohand, const Value* doc, unsigned seqid )
{
	int code = -1;
	RJSON_GETSTR_D(notify, doc);
	if ("closelink" == notify)
	{
		RJSON_VGETINT_D(svrid, CONNTERID_KEY, doc);
		CliBase* cli = CliMgr::Instance()->getChildBySvrid(svrid);
		IOHand* iocli = dynamic_cast<IOHand*>(cli);
		code = -2;
		if (cli && iocli)
		{
			code = iocli->driveClose(_F("notify close by %s", iohand->m_idProfile.c_str()));
		}
	}

	nlohmann::json obj {
		{"code", code},
		{"notify_r", notify}
	};
	string resp = obj.dump();

	iohand->sendData(CMD_EVNOTIFY_RSP, seqid, resp.c_str(), resp.length(), true);
	return 0;
}
