/*-------------------------------------------------------------------------
FileName     : tcpaio_invoker.h
Description  : 服务消费者异步IO处理
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-12-30       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _INVOKER_AIO_H_
#define _INVOKER_AIO_H_
#include <arpa/inet.h>
#include <memory>
#include <tuple>
#include <shared_mutex>
#include "comm/hep_base.h"
#include "comm/queue.h"
#include "cloud/iobuff.h"

typedef long long datasize_t;
typedef void (*InvkCBFunc)( int result, int seqid, const char* msg );

struct InvokerException
{
	string reson;
	int code;

	InvokerException(int retcode, const string& reson): 
		reson(reson), code(retcode) { }
};

class TcpAioInvoker: ITaskRun2
{
public:
    TcpAioInvoker( const string& dsthostp );
    virtual ~TcpAioInvoker( void );


public:
	int init( int epfd, int timeout_sec );
    virtual int run( int flag, long p2 );
	virtual int qrun( int flag, long p2 );

	time_t getAtime( void );
	int getIOStatJson( string& rspjson ) const;


	int driveClose( const string& reason );
	void setEpThreadID( void );
	void setEpThreadID( pthread_t thid );

	int request( string& resp, int cmdid, const string& body );
	int request( InvkCBFunc cb_func, int cmdid, const string& body );
	

protected:
	int _connect( void );
	int onRead( int p1, long p2 );
	int onWrite( int p1, long p2 );
	int onClose( int p1, long p2 );
	void clearBuf( void );
	void clearReqQueue( void );

	int stageCheck( void );
	int cmdProcess( IOBuffItem*& iBufItem );
	int sendData( unsigned int cmdid, unsigned int seqid, const char* body, 
		unsigned int bodylen, bool setOutAtonce );

	void appendTimerQWait( int dtsec );

protected:
	string m_dstHost;
	int m_dstPort;
	int m_stage; // 所处状态：0 未连接；1 连接中；2 已连接；3连接失败
    int m_cliFd;
	int m_seqid;
	int m_timeout_interval_sec;
	time_t m_atime;
	HEpEvFlag m_epCtrl;
	pthread_t m_epThreadID; // epoll-IO复用的线程id
	string m_cliName;
	string m_closeReason; // 关掉原因
	unsigned char m_closeFlag; // 结束标记: 0连接中, 1等待发完后关闭; 2立即要关; 3关闭

	datasize_t m_recv_bytes;
	datasize_t m_send_bytes;
	static datasize_t serv_recv_bytes;
	static datasize_t serv_send_bytes;
	int m_recvpkg_num;
	int m_sendpkg_num;
	static int serv_recvpkg_num;
	static int serv_sendpkg_num;

	IOBuffItem* m_iBufItem;
	std::unique_ptr<IOBuffItem> m_oBufItem;
	Queue<std::unique_ptr<IOBuffItem>> m_oBuffq;
	std::map< int, std::shared_ptr< Queue<string> > > m_reqQueue;
	std::map< int, std::tuple<time_t, InvkCBFunc> > m_reqCBQueue;
	bool m_inTimerq;
	std::shared_mutex m_qLock;
};

#endif
