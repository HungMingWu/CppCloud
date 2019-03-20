/*-------------------------------------------------------------------------
FileName     : peer_serv.h
Description  : 分布式的除自身外的服务端
remark       : 远端serv的别名引用是serv_nnn
Modification :
--------------------------------------------------------------------------
   1、Date  2018-09-10       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _PEER_SERV_H_
#define _PEER_SERV_H_
#include "iohand.h"
#include <string>

class PeerServ: public IOHand
{
public:
    HEPCLASS_DECL(PeerServ, PeerServ);
    PeerServ(void);
    virtual ~PeerServ(void);

	static void Init( int mysvrid );
	int init( const std::string& rhost, int port, int epfd );
	void setSvrid( int svrid );
	int appendTimerq( void );
	void reset( void );

public: // interface HEpBase
    virtual int qrun( int flag, long p2 );
	virtual int onEvent( int evtype, va_list ap );

protected:
	int taskRun( int flag, long p2 );
	int onClose( int p1, long p2 );

	int prepareWhoIam( void );
	int exitRun( int flag, long p2 );

public:
	static int s_my_svrid;

protected:
	int m_stage;
	int m_seqid;
	int m_svrid;
	int m_epfd;
	int m_port;
	bool m_inqueue;
	bool m_existLink; // 是否已连接中
	std::string m_rhost;
};

#endif
