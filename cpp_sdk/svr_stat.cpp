#include "svr_stat.h"
#include "comm/strparse.h"
#include "cloud/switchhand.h"
#include "cloudapp.h"
#include "comm/json.hpp"

std::string SvrStat::CountEntry::jsonStr( void ) const
{
    nlohmann::json obj {
	{"svrid", svrid},
	{"pvd_ok", pvd_ok},
	{"pvd_ng", pvd_ng},
	{"ivk_ok", ivk_ok},
	{"ivk_ng", ivk_ng},
	{"ivk_dok", ivk_dok},
	{"ivk_dng", ivk_dng},
	{"prvdid", prvdid},
	{"regname", regname}
    };
    return obj.dump();
}

void SvrStat::CountEntry::cleanDeltaCount( void )
{
    ivk_dok = 0;
    ivk_dng = 0;
}

SvrStat::SvrStat( void )
{
    m_inqueue = false;
    m_delayTimeSec = 15;
}
SvrStat::~SvrStat( void )
{
    
}

SvrStat::CountEntry* SvrStat::_getEntry( const std::string& regname, int svrid, int prvdid )
{
    const std::string key = regname + _F("-%d-%d", svrid, prvdid);
    auto it = m_stat.find(key);

    if (it != m_stat.end())
    {
        return &it->second;
    }

    m_stat[key].svrid = svrid;
    m_stat[key].prvdid = prvdid;
    m_stat[key].regname = regname;
    return &m_stat[key];
}

void SvrStat::addPrvdCount( const std::string& regname, bool isOk, int prvdid, int svrid, int dcount )
{
    std::unique_lock lock(m_rwLock);
    CountEntry* stat = _getEntry(regname, svrid, prvdid);
    if (isOk)
    {
        stat->pvd_ok += dcount;
    }
    else
    {
        stat->pvd_ng += dcount;
    }
    
    appendTimerq();
}

// param: svrid 要设置服务提供方里的增量成员ivk_dok，必须要提供目标svrid
void SvrStat::addInvkCount( const std::string& regname, bool isOk, int prvdid, int svrid, int dcount )
{
    IFRETURN(m_delayTimeSec <= 0);
    
    std::unique_lock lock(m_rwLock);
    CountEntry* stat = _getEntry(regname, svrid, prvdid);
    
    if (isOk)
    {
        stat->ivk_ok += dcount;
        if (svrid > 0) stat->ivk_dok += dcount;
    }
    else
    {
        stat->ivk_ng += dcount;
        if (svrid > 0) stat->ivk_dng += dcount;
    }

    appendTimerq();
}

void SvrStat::setDelaySecond( int sec )
{
    m_delayTimeSec = sec;
}

int SvrStat::qrun( int flag, long p2 )
{
    int  ret = 0;
	m_inqueue = false;
	if (0 == flag)
    {
        std::string msg("[");

        int cnt = 0;
        {
            std::shared_lock lock(m_rwLock);
            for (auto it : m_stat)
            {
                std::string entryJson = it.second.jsonStr();
                if (!entryJson.empty())
                {
                    if (cnt > 0) msg += ",";

                    it.second.cleanDeltaCount();
                    msg += entryJson;
                    cnt++;
                }
            }            
        }

        msg += "]";
        if (cnt > 0)
        {
            ret = CloudApp::Instance()->postRequest(CMD_SVRSTAT_REQ, msg);
            ERRLOG_IF1(ret, "SVRSTAT| msg=postmsg fail %d| count=%d", ret, cnt);
        }
    }

    return ret;
}

int SvrStat::run(int p1, long p2)
{
    return -1;
}

// 驱动定时检查任务，qrun()
int SvrStat::appendTimerq( void )
{
	int ret = 0;
	if (!m_inqueue)
	{
		int wait_time_msec =m_delayTimeSec*1000;
		ret = SwitchHand::Instance()->appendQTask(this, wait_time_msec );
		m_inqueue = (0 == ret);
		ERRLOG_IF1(ret, "APPENDQTASK| msg=append fail| ret=%d", ret);
	}
	return ret;
}
