/*-------------------------------------------------------------------------
FileName     : client_c.hpp
Description  : cppcloud c++ sdk接口文件
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-11-07       create     hejl 
-------------------------------------------------------------------------*/
#include <string>

using std::string;
struct svr_item_t; // defind in "svr_item.h"
struct msg_prop_t; // define in "msgprop.h"
typedef int (*CMD_HAND_FUNC)(msg_prop_t*, const char*);

namespace client_c
{
    // 初始化，连接cppcloud-serv，获得主配置文件名
    int Init( const string& appName, const string& servHostPort, int appid = 0 );

    // 加载配置指定文件(download)
    int LoadConfFile( const string& fname );
    string GetMConfName( void ); // 获取主配置文件名
    // 配置查询 
    // T=[string, int, map<string,string>, map<string,int>, vector<string>, vector<int>]
    template<class T> int Query( T& oval, const string& fullqkey );

    // TCP服务提供者
    int InitTcpProvider( int listenPort );
    int InitTcpProvider( const string& host, int listenPort, int qlen );
    
    // tcp服务提供CallBack
    int AddProvdFunction( CMD_HAND_FUNC func );
    int AddCmdFunction( unsigned cmdid, CMD_HAND_FUNC func );
    int ProvdSendMsg( const msg_prop_t* msgprop, const string& msg );
    int ProvdSendMsgAsync( const msg_prop_t* msgprop, const string& msg );
    
    // 发布服务
    int regProvider( const string& regname, short protocol, int port, const string& path="" );
    int regProvider( const string& regname, short protocol, const string& url );
    void setDesc( const string& regname, const string& desc );
    void setWeight( const string& regname, short weight );
    void setVersion( const string& regname, short ver );
    void setEnable( const string& regname, bool enable );
    void addOkCount( const string& regname, int dcount );
    void addNgCount( const string& regname, int dcount );
    int postOut( const string& regname );
    int postOut( const string& regname, bool enable );


    // 服务消费者
    int InitInvoker( const string& svrList );
    void SetRefreshTimeOut( int sec );
    int GetSvrPrvd( svr_item_t& pvd, const string& svrname );

    // tcp服务消费者
    int TcpRequest( string& resp, const string& reqmsg, const string& svrname );

    // http服务消费者
    int HttpGet( string& resp, const string& reqmsg, const string& svrname );
    int HttpPost( string& resp, const string& reqmsg, const string& svrname );

    int NotifyExit( void* ptr );
    int Run( bool runBackgroud );

    // 销毁和退出
    void Destroy( void );
};