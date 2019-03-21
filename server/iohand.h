/*-------------------------------------------------------------------------
FileName     : iohand.h
Description  : 收发tcp协议消息
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-01-23       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _IOHAND_H_
#define _IOHAND_H_
#include <arpa/inet.h>
#include <memory>
#include "comm/hep_base.h"
#include "comm/queue.h"
#include "cloud/iobuff.h"
#include "clibase.h"

typedef long long datasize_t;

class IOHand: public CliBase
{
public:
    HEPCLASS_DECL(IOHand, IOHand);
    IOHand(void);
    virtual ~IOHand(void);

	static int Init( void );

public: // interface HEpBase
    virtual int run( int flag, long p2 );
	virtual int onEvent( int evtype, va_list ap );

	// 自定义属性的操作
    //void setProperty( const string& key, const string& val );
    //string getProperty( const string& key );
	//int getCliType(void) {return m_cliType; }
	string getERAstr( void );

	int driveClose( const string& reason );
	void setAuthFlag( int auth );

	int sendData( unsigned int cmdid, unsigned int seqid, const char* body, 
		unsigned int bodylen, bool setOutAtonce );

protected:
	int onRead( int p1, long p2 );
	int onWrite( int p1, long p2 );
	virtual int onClose( int p1, long p2 );
	void clearBuf( void );

	int authCheck( IOBuffItem*& iBufItem );
	int interceptorProcess( IOBuffItem*& iBufItem );
	int cmdProcess( IOBuffItem*& iBufItem );

private:
	static int _AddInterceptor( unsigned cmdid, const string& funcname );

protected:
    int m_cliFd;
	HEpEvFlag m_epCtrl;
	string m_closeReason; // 关掉原因
	unsigned char m_closeFlag; // 结束标记: 0连接中, 1等待发完后关闭; 2立即要关; 3关闭
	int m_authFlag; // 权限标识
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

};

#endif
