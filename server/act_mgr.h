/*-------------------------------------------------------------------------
FileName     : act_mgr.h
Description  : 客户端行为信息管理类(告警，上下机等)
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-01-23       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _ACT_MGR_H_
#define _ACT_MGR_H_
#include "comm/public.h"
#include "comm/json.hpp"
#include <cstdarg>
#include <map>
#include <list>
#include <string>

using std::map;
using std::list;

class CliBase;
struct CliInfo;

class Actmgr
{
    SINGLETON_CLASS2(Actmgr)
    enum { CLIOPLOG_SIZE = 200 };

public:
    int NotifyCatch( void* ptr, unsigned cmdid, void* param );

public:
    // 获取连接中的客户端属性信息
    std::vector<nlohmann::json> pickupCliProfile(int svrid, const std::string& key);
    // 获取已掉线的客户信息
    std::vector<std::string> pickupCliCloseLog();
    // 获取客户行为日志信息
    std::vector<std::string> pickupCliOpLog(int nSize);
    // 获取所有告警状态的客户机信息
    std::vector<nlohmann::json> pickupWarnCliProfile(const std::string& filter_key, const std::string& filter_val);

    int appCloseFound( CliBase* son, int clitype, const CliInfo& cliinfo );
    void setCloseLog( int svrid, const std::string& cloLog );
    void rmCloseLog( int svrid );
    void appendCliOpLog( const std::string& logstr );
    /////////////
    void setWarnMsg( const std::string& taskkey, CliBase* ptr );
    void clearWarnMsg( const std::string& taskkey );

private:
    nlohmann::json getJsonProp(CliBase* cli, const std::string& key);
private:
    Actmgr(void);
    ~Actmgr(void);

protected:
    map<CliBase*, CliInfo>* m_pchildren; // 对CliMgr.m_children的引用
    map<std::string, CliBase*> m_warnLog; // 客户机告警的任务
    map<int, std::string> m_closeLog; // 记录掉线了的客户机信息
    list<std::string> m_cliOpLog; // 客户机的操作行为记录
    int m_opLogSize;
};

#endif
