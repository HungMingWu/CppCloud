/*-------------------------------------------------------------------------
FileName     : config_mgr.h
Description  : 分布式配置管理类
remark       : 线程安全
Modification :
--------------------------------------------------------------------------
   1、Date  2018-10-30       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _CONFIG_MGR_H_
#define _CONFIG_MGR_H_
#include "comm/public.h"
#include <string>
#include <map>
#include <vector>

typedef void (*CONF_CHANGE_CB)(const std::string& confname);
class ConfJson;

class ConfigMgr
{
    SINGLETON_CLASS2(ConfigMgr)
public:
    int OnCMD_EVNOTIFY_REQ( void* ptr );
    int onCMD_EVNOTIFY_REQ( void* ptr );
    int OnCMD_GETCONFIG_RSP( void* ptr, unsigned cmdid, void* param );
    int onCMD_GETCONFIG_RSP( void* ptr, unsigned cmdid, void* param );
    int OnReconnectNotifyCB( void* param );
    int onReconnectNotifyCB( void* param );

    ConfigMgr( void );
    ~ConfigMgr( void );

public:
    int initLoad( const std::string& confName );
    //void setMainName( const std::string& mainConf );
    void uninit( void );
    void setChangeCallBack( CONF_CHANGE_CB cb );

    /**
     * @summery: 通过传入查询键名qkey，返回对应值
     * @param: ValT must be [string, int, map<string,string>, map<string,int>, vector<string>, vector<int>]
     * @remart: thread-safe method
     * @return: if success return 0; 
    ***/
    int query( int& oval, const std::string& fullqkey, bool wideVal );
    int query( std::string& oval, const std::string& fullqkey, bool wideVal );
    int query( std::map<std::string, std::string>& oval, const std::string& fullqkey, bool wideVal );
    int query( std::map<std::string, int>& oval, const std::string& fullqkey, bool wideVal );
    int query( std::vector<std::string>& oval, const std::string& fullqkey, bool wideVal );
    int query( std::vector<int>& oval, const std::string& fullqkey, bool wideVal );
    

private:
    void _clearCache( void );

    // ValT must be [string, int, map<string,string>, map<string,int>, vector<string>, vector<int>]
    template<class ValT>
    int _query( ValT& oval, const std::string& fullqkey, std::map<std::string, ValT >& cacheMap, bool wideVal ) const;
    template<class ValT>
    int _tryGetFromCache( ValT& oval, const std::string& fullqkey, const std::map<std::string, ValT >& cacheMap ) const;

    
private:
    std::string m_mainConfName; // 主配置文件名
    std::map<std::string, ConfJson*> m_jcfgs;
    CONF_CHANGE_CB m_changeCB;

    // 缓存
    std::map<std::string, std::string> m_cacheStr;
    std::map<std::string, int> m_cacheInt;
    std::map<std::string, std::map<std::string, std::string> > m_cacheMapStr;
    std::map<std::string, std::map<std::string, int> > m_cacheMapInt;
    std::map<std::string, std::vector<std::string> > m_cacheVecStr;
    std::map<std::string, std::vector<int> > m_cacheVecInt;
};

#endif
