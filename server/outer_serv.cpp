#include "outer_serv.h"
#include "comm/strparse.h"
#include "iohand.h"
#include "climanage.h"


HEPMUTICLASS_IMPL(OuterServ, OuterServ, CliBase)

OuterServ::OuterServ()
{
    m_cliType = 1;
    m_isLocal = false;
    m_svrid = 0;
}

void OuterServ::init( int svrid )
{
    m_svrid = svrid;
}

/**
 * @summery: 设置外围Serv的路由
 * @remark: rp参数格式: 1>4>6>8>
 **/
int OuterServ::setRoutePath( const std::string& strpath )
{
    int ret = 0;
    RoutePath routpath;
    
    ret = StrParse::SpliteInt(routpath.path, strpath, '>');
    unsigned pathn = routpath.path.size();

    if (0 == ret && pathn > 0)
    {
        ERRLOG_IF1RET_N(m_svrid!=routpath.path[0], -37, 
            "SETROUTPATH| msg=invalid path| path=%s| servid=%d", strpath.c_str(), m_svrid);
        
        bool valid = false;
        vector<int>::reverse_iterator itrNxt = routpath.path.rbegin();
        for (; itrNxt != routpath.path.rend(); ++itrNxt)
        {
            if (*itrNxt != s_my_svrid)
            {
                routpath.next_svrid = *itrNxt;
                valid = true;
                break;
            }
        }

        if (valid)
        {
            routpath.mtime = time(NULL);
            std::string key = StrParse::Format("P%d_%s", pathn, strpath.c_str());
            m_routpath[key] = routpath;           
        }
        else
        {
            LOGERROR("SETROUTPATH| msg=invalid nextsvrid in path| path=%s(%d->%d)", strpath.c_str(), m_svrid, s_my_svrid);
            ret = -38;
        }
    }

    return ret;
}

/**
 * @summery: 获取较优的下一跳路由点
 * @remark:
 **/
IOHand* OuterServ::getNearSendServ( void )
{
    static const int alive_interval_sec1 = 60*5;
    static const int alive_interval_sec2 = 60*15;
    IOHand* ret = NULL;
    int now = time(NULL);

    for (auto it = m_routpath.begin(); it != m_routpath.end(); )
    {
        auto it0 = it++;
        RoutePath& ref = it0->second;
        CliBase* servptr = CliMgr::Instance()->getChildBySvrid(ref.next_svrid);
        if (servptr)
        {
            IOHand* tmpptr = dynamic_cast<IOHand*>(servptr);
            if (!servptr->isLocal() || NULL==tmpptr)
            {
                LOGERROR("GETNEARSERV| msg=logic error| servptr=%s", servptr->m_idProfile.c_str());
                m_routpath.erase(it0);
                continue;
            }
            
            if (now < ref.mtime + alive_interval_sec1)
            {
                ret = tmpptr; // 得到最优下一跳，即刻返回
                break;
            }
            if (now > ref.mtime + alive_interval_sec2)
            {
                m_routpath.erase(it0);
                break;
            }
            
            if (NULL == ret)
            {
                ret = tmpptr;
            }
        }
        else
        {
            m_routpath.erase(it0);
        }
    }

    return ret;
}

