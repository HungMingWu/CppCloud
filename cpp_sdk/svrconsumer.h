/*-------------------------------------------------------------------------
FileName     : svrconsumer.h
Description  : 服务发现之消费者管理
remark       : 服务消费者获取服务地址列表
Modification :
--------------------------------------------------------------------------
   1、Date  2018-11-02       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _SVRCONSUMER_H_
#define _SVRCONSUMER_H_
#include <string>
#include <map>
#include <vector>
#include <shared_mutex>
#include "comm/public.h"
#include "comm/i_taskrun.h"
#include "svr_item.h"

class SvrConsumer : public ITaskRun2
{
    struct SvrItem
    {
        std::vector<svr_item_t> svrItms;
        int weightSum = 0;
        int callcount = 0;
        int timeout_sec = 3; // 调用时的超时时间，由调用方指定
        time_t ctime;

        void rmBySvrid( int svrid, int prvdid );
        svr_item_t* randItem( void );
    };

    SINGLETON_CLASS2(SvrConsumer);
    SvrConsumer( void );
    ~SvrConsumer( void );

public:
    int OnCMD_SVRSEARCH_RSP( void* ptr, unsigned cmdid, void* param );
    int OnCMD_EVNOTIFY_REQ( void* ptr ); // provider 下线通知

    int onCMD_SVRSEARCH_RSP( void* ptr, unsigned cmdid, void* param );
    int onCMD_EVNOTIFY_REQ( void* ptr ); // provider 下线通知

    // interface ITaskRun2
    virtual int qrun( int flag, long p2 );
    virtual int run(int p1, long p2);

    int init( const std::string& svrList );
    void uninit( void );

    void setRefreshTO( int sec );
    void setInvokeTimeoutSec( int sec, const std::string& regname = "all" );
    int getInvokeTimeoutSec( const std::string& regname );

    // 获取一个服务提供者信息，用于之后发起调用。
    int getSvrPrvd( svr_item_t& pvd, const std::string& svrname );
    // 更新接口调用的统计信息
    void addStat( const svr_item_t& pvd, bool isOk, int dcount=1 );

private:
    int parseResponse( std::string& msg );
    int parseResponse( const void* ptr );

    int appendTimerq( bool force );
    int _postSvrSearch( const std::string& regname ) const;

private:
    std::map<std::string, SvrItem> m_allPrvds;
    std::map<std::string, bool> m_emptyPrvds; // 存放失去全部连接的提供者名

    int m_refresh_sec;
    int m_invoker_default_tosec;
    
    std::shared_mutex m_rwLock;
    bool m_inqueue;
};

#endif
