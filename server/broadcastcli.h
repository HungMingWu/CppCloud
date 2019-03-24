/*-------------------------------------------------------------------------
FileName     : broadcastcli.h
Description  : 广播本Serv下的cli
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-09-13       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _BROADCAST_CLI_H_
#define _BROADCAST_CLI_H_
#include "comm/hep_base.h"
#include "rapidjson/json.hpp"
#include "comm/json.hpp"
class CliBase;
class IOHand;


class BroadCastCli: public HEpBase
{
public:
    HEPCLASS_DECL(BroadCastCli, BroadCastCli);
    SINGLETON_CLASS2(BroadCastCli)
    BroadCastCli();

public:
    static int OnBroadCMD( void* ptr, unsigned cmdid, void* param );
    static int TransToAllPeer( void* ptr, unsigned cmdid, void* param );
    static int on_CMD_BROADCAST_REQ(IOHand* iohand, const nlohmann::json& obj, unsigned seqid);
    static int on_CMD_CLIERA_REQ(IOHand* iohand, const nlohmann::json& obj, unsigned seqid);
    static int on_CMD_CLIERA_RSP(IOHand* iohand, const nlohmann::json& obj, unsigned seqid);
    static int on_CMD_UPDATEERA_REQ(IOHand* iohand, const nlohmann::json& obj, unsigned seqid);

    void init( int my_svrid );
    
    static string GetDebugTrace( void );
    static int setJsonMember( const string& key, const string& val, Document* node );

    // 消息广播方法
    int toWorld(nlohmann::json& obj, unsigned cmdid, unsigned seqid, bool includeCli);
    int toWorld( const string& jsonmsg, unsigned cmdid, unsigned seqid, bool includeCli );
    int toAllLocalCli( unsigned cmdid, unsigned seqid, const string& msg, const string& clifilter );

protected: // interface IEPollRun
	virtual int run(int p1, long p2);
    virtual int qrun( int flag, long p2 );

    static string diffOuterCliEra( int servid, const string& erastr );
    static int UpdateCliProps(const nlohmann::json& objs, int from);
    static void AfterUpdatePropsHandle( CliBase* cli );

protected:
    static int s_my_svrid;
    int m_seqid;
    static string s_debugTrace;
};

#endif
