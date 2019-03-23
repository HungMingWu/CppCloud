#include "svrconsumer.h"
#include "rapidjson/json.hpp"
#include "comm/strparse.h"
#include "cloud/msgid.h"
#include "cloud/homacro.h"
#include "cloud/exception.h"
#include "cloud/switchhand.h"
#include "svr_stat.h"
#include "cloudapp.h"
#include "comm/json.hpp"

void SvrConsumer::SvrItem::rmBySvrid( int svrid, int prvdid )
{
    auto it = svrItms.begin();
    for (; it != svrItms.end(); )
    {
        if (svrid == it->svrid)
        {
            if (0 == prvdid || prvdid == it->prvdid)
            {
                this->weightSum -= it->weight;
                it = svrItms.erase(it);
            }
            else
            {
                ++it;
            }
        }
        else
        {
            ++it;
        }
    }
}

svr_item_t* SvrConsumer::SvrItem::randItem( void )
{
    IFRETURN_N(svrItms.empty(), NULL);
    IFRETURN_N(weightSum <= 1, &svrItms[0]);
    
    int nrd = rand()%weightSum;
    int tmpsum = 0;
    auto it0 = svrItms.begin();
    for (; it0 != svrItms.end(); ++it0)
    {
        svr_item_t* pitm = &(*it0);
        tmpsum += pitm->weight;
        if (tmpsum > nrd)
        {
            return pitm;
        }
    }

    LOGWARN("RANDSVR| msg=logic err flow| nrd=%d| weightSum=%d| tmpsum=%d",
            nrd, weightSum, tmpsum);
    return &svrItms[0];
}

SvrConsumer::SvrConsumer( void )
{
    m_refresh_sec = 10*60;
    m_invoker_default_tosec = 3;
    m_inqueue = false;
}

SvrConsumer::~SvrConsumer( void )
{
    uninit();
}

int SvrConsumer::OnCMD_SVRSEARCH_RSP( void* ptr, unsigned cmdid, void* param )
{
    return onCMD_SVRSEARCH_RSP(ptr, cmdid, param);
}

int SvrConsumer::OnCMD_EVNOTIFY_REQ( void* ptr ) // provider 下线通知
{
    return onCMD_EVNOTIFY_REQ(ptr);
}

int SvrConsumer::onCMD_SVRSEARCH_RSP( void* ptr, unsigned cmdid, void* param )
{
    MSGHANDLE_PARSEHEAD(false);
    int ret = parseResponse(&doc);

    return ret;
}

int SvrConsumer::onCMD_EVNOTIFY_REQ( void* ptr )
{
    const Document* doc = (const Document*)ptr;
    RJSON_GETSTR_D(notify, doc);
    RJSON_GETSTR_D(regname, doc);
    RJSON_GETINT_D(svrid, doc);
    RJSON_GETINT_D(prvdid, doc);

    ERRLOG_IF1RET_N(notify!="provider_down" || 0==svrid, -113, 
        "EVNOTIFY| msg=%s", Rjson::ToString(doc).c_str());
    
    std::unique_lock lock(m_rwLock);
    auto it = m_allPrvds.find(regname);
    if (it != m_allPrvds.end())
    {
        auto &[name, item] = *it;
        item.rmBySvrid(svrid, prvdid);
        if (item.svrItms.size() <= 0)
        {
            m_emptyPrvds[name] = true;
            appendTimerq(true);
        }
    }

    return 0;
}

// @param: svrList 是空格分隔启动时需要获得的服务, 服务和版本间冒号分开
int SvrConsumer::init( const string& svrList )
{
    static const char seperator = ' ';
    static const char seperator2 = ':';
    //static const int timeout_sec = 3;
    std::vector<std::string> vSvrName;

    int ret = StrParse::SpliteStr(vSvrName, svrList, seperator);
    ERRLOG_IF1RET_N(ret, -110, "CONSUMERINIT| msg=splite to vector fail %d| svrList=%s", 
        ret, svrList.c_str());

    auto cit = vSvrName.begin();
    for (; cit != vSvrName.end(); ++cit)
    {
        std::vector<std::string> vSvrNver;
        StrParse::SpliteStr(vSvrNver, *cit, seperator2);

        const string& svrname = vSvrNver[0];
        nlohmann::json obj {
		{"regname", svrname},
		{"version", atoi(vSvrNver[1].c_str())},
		{"bookchange", 1}
        };

        string resp;
        ret = CloudApp::Instance()->begnRequest(resp, CMD_SVRSEARCH_REQ, 
                obj.dump(),
                false); 
        IFBREAK(ret);

        ret = parseResponse(resp);
        IFBREAK(ret);
    }

    ERRLOG_IF1RET_N(m_allPrvds.size() < vSvrName.size(), -117, 
            "CONSUMERINIT| msg=less of provider| num=%zu/%zu| svrList=%s", 
            m_allPrvds.size(), vSvrName.size(), svrList.c_str());
    if (0 == ret)
    {
        // srand(time(NULL))
        auto regfunc = [this](auto&& ...params) { return OnCMD_EVNOTIFY_REQ(std::forward<decltype(params)>(params)...); };
        auto cmdfunc = [this](auto&& ...params) { return OnCMD_SVRSEARCH_RSP(std::forward<decltype(params)>(params)...); };
        CloudApp::Instance()->setNotifyCB("provider_down", std::move(regfunc));
        ret = CloudApp::Instance()->addCmdHandle(CMD_SVRSEARCH_RSP, std::move(cmdfunc)) ? 0 : -111;
        appendTimerq(false);
    }

    return ret;
}

int SvrConsumer::parseResponse( string& msg )
{
    Document doc;
    ERRLOG_IF1RET_N(doc.ParseInsitu((char*)msg.data()).HasParseError(), -111, 
        "COMSUMERPARSE| msg=json invalid");
    
    return parseResponse(&doc);
}

int SvrConsumer::parseResponse( const void* ptr )
{
    const Document* doc = (const Document*)ptr;

    RJSON_GETINT_D(code, doc);
    RJSON_GETSTR_D(desc, doc);
    const Value* pdata = NULL;
    Rjson::GetArray(&pdata, "data", doc);
    ERRLOG_IF1RET_N(0 != code || NULL == pdata, -112, 
        "COMSUMERPARSE| msg=resp fail %d| err=%s", code, desc.c_str());
    
    int ret = 0;
    Value::ConstValueIterator itr = pdata->Begin();
    string regname;
    string prvdLog;
    std::optional<SvrItem> prvds;

    for (int i = 0; itr != pdata->End(); ++itr, ++i)
    {
        svr_item_t svitm;
        const Value* node = &(*itr);
        if (!prvds)
        {
            prvds = SvrItem();
            prvds->ctime = time(NULL);
            prvds->timeout_sec = m_invoker_default_tosec;
            RJSON_GETSTR(regname, node);
        }
        
        svitm.regname = regname;
        RJSON_VGETSTR(svitm.url, "url", node);
        RJSON_VGETINT(svitm.svrid, "svrid", node);
        RJSON_VGETINT(svitm.prvdid, "prvdid", node);
        RJSON_GETINT_D(weight, node);
        RJSON_GETINT_D(protocol, node);
        bool validurl = svitm.parseUrl();
        ERRLOG_IF1BRK(!validurl, -113, "COMSUMERPARSE| msg=invalid url found| "
            "url=%s| svrname=%s", svitm.url.c_str(), regname.c_str());

        svitm.weight = weight;
        svitm.protocol = protocol;
        prvds->weightSum += weight;
        prvds->svrItms.push_back(svitm);

        if (i <= 3)
        {
            StrParse::AppendFormat(prvdLog, "%d%%%d@%s,", svitm.svrid, svitm.prvdid, svitm.url.c_str());
        }
    }

    LOGOPT_EI(prvdLog.empty(), "PROVDLIST| regname=%s| svitem=%s", regname.c_str(), prvdLog.c_str());
    if (ret)
    {
    }
    else
    {
        if (prvds)
        {
            std::unique_lock lock(m_rwLock);
            auto oldit = m_allPrvds.find(regname);
            if (oldit != end(m_allPrvds))
                prvds->timeout_sec = oldit->second.timeout_sec;
            m_allPrvds[regname] = prvds.value();
            if (prvds->svrItms.size() > 0)
            {
                m_emptyPrvds.erase(regname);
            }
        }
    }

    return ret;
}

void SvrConsumer::uninit( void )
{
    std::unique_lock lock(m_rwLock);
    m_allPrvds.clear();
}

void SvrConsumer::setRefreshTO( int sec )
{
    m_refresh_sec = sec;
}

void SvrConsumer::setInvokeTimeoutSec( int sec, const string& regname )
{
    std::unique_lock lock(m_rwLock);
    if (regname.empty() || "all" == regname)
    {
        for (auto &[name, item] : m_allPrvds)
            item.timeout_sec = sec;

        m_invoker_default_tosec = sec;
    }
    else
    {
        auto it = m_allPrvds.find(regname);
        if ( m_allPrvds.end() != it )
        {
            it->second.timeout_sec = sec;
        }
    }
}


int SvrConsumer::getInvokeTimeoutSec( const string& regname )
{
    std::shared_lock lock(m_rwLock);
    auto it = m_allPrvds.find(regname);
    if ( m_allPrvds.end() != it )
    {
        return it->second.timeout_sec;
    }
    return m_invoker_default_tosec;
}

int SvrConsumer::getSvrPrvd( svr_item_t& pvd, const string& svrname )
{
    std::shared_lock lock(m_rwLock);
    auto it = m_allPrvds.find(svrname);
    IFRETURN_N(it == m_allPrvds.end(), -1);
    svr_item_t* itm = it->second.randItem();
    int ret = -114;
    if (itm)
    {
        pvd = *itm;
        ret = 0;
    }

    return ret;
}

void SvrConsumer::addStat( const svr_item_t& pvd, bool isOk, int dcount )
{
    SvrStat::Instance()->addInvkCount(pvd.regname, isOk, pvd.prvdid, pvd.svrid, dcount);
}

int SvrConsumer::_postSvrSearch( const string& svrname ) const
{
    int ret = CloudApp::Instance()->postRequest( CMD_SVRSEARCH_REQ, 
        _F("{\"regname\": \"%s\", \"bookchange\": 1}", 
                svrname.c_str()) );

    ERRLOG_IF1(ret, "POSTREQ| msg=post CMD_SVRSEARCH_REQ fail| "
            "ret=%d| regname=%s", ret, svrname.c_str());
    
    return ret;
}

// 驱动定时检查任务，qrun()
int SvrConsumer::appendTimerq( bool force )
{
	int ret = 0;

    if (force && m_inqueue)
    {
        SwitchHand::Instance()->remove(this);
        m_inqueue = false;
    }

	if (!m_inqueue)
	{
		int wait_time_msec = m_emptyPrvds.empty()? m_refresh_sec*1000 : 5000; // 如果有某服务提供者全部断链时，5秒尝试重新获得
		ret = SwitchHand::Instance()->appendQTask(this, wait_time_msec );
		m_inqueue = (0 == ret);
		ERRLOG_IF1(ret, "APPENDQTASK| msg=append fail| ret=%d", ret);
	}
	return ret;
}

// 定时任务：检查是否需刷新服务提供者信息
int SvrConsumer::qrun( int flag, long p2 )
{
	int  ret = 0;
	m_inqueue = false;
	if (0 == flag)
	{
        time_t now = time(NULL);
        std::shared_lock lock(m_rwLock);
        string desc;
        for (auto &[name, svitm] : m_allPrvds)
            if (svitm.svrItms.empty() || svitm.ctime < now - m_refresh_sec)
            {
                ret = _postSvrSearch(name);
                desc += name + " ";
            }

        LOGDEBUG("SVRCONSUMER| msg=post out checking svr req| svrlist=%s", desc.c_str());
        appendTimerq(false);
	}
	else if (1 == flag)
	{
		// exit handle
	}

	return ret;
}

int SvrConsumer::run(int p1, long p2)
{
    LOGERROR("INVALID_FLOW");
    return -1;
}
