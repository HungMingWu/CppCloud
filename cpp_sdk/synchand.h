/*-------------------------------------------------------------------------
FileName     : synchand.h
Description  : 用于同步方式发送消息
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-10-31       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _SYNC_HAND_H_
#define _SYNC_HAND_H_

#include <pthread.h>
#include <string>
#include <map>

class SyncHand
{
    struct MsgItem
    {
        std::string resp;
        pthread_cond_t m_cond;
        time_t expire_time;
        int timeout_sec;
        unsigned char step; // 0 初始； 1 已发送，等响应；2已响应;

        MsgItem( void );
        ~MsgItem( void );
    };

public:
    SyncHand( void );
    ~SyncHand( void );

    int putRequest( unsigned rspid, unsigned seqid, int timeout_sec );
    int waitResponse( std::string& resp, unsigned rspid, unsigned seqid );
    void delRequest( unsigned rspid, unsigned seqid );

    int notify( unsigned rspid, unsigned seqid, const std::string& msg );

private:
    std::map<unsigned, MsgItem> m_msgItems;
    pthread_mutex_t m_mutex;
};

#endif
