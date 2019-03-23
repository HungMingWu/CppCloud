/*-------------------------------------------------------------------------
FileName     : climanage.h
Description  : 客户端连接管理类
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-01-23       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _CLIMANAGE_H_
#define _CLIMANAGE_H_
#include "comm/public.h"
#include "comm/maprange.hpp"
#include "iohand.h"
#include <cstdarg>
#include <map>
#include <memory>
#include <vector>
#include <string>

using std::map;
using std::vector;

typedef void (*CliPreCloseNotifyFunc)( CliBase* );
typedef MapRanger<std::string, CliBase*> CliMapRange;

struct CliInfo
{
    time_t t0 = 0, t1 = 0, t2 = 0; // 连接时间，活动时间，关闭时间
    bool inControl = true; // 如果为true,当调用removeAliasChild删除时会delete对象
    std::map<std::string, bool> aliasName; // 客户端的别名，其他人可以通过该别人找到此对象
};


class CliMgr
{
    SINGLETON_CLASS2(CliMgr)

public:
    enum { CLIOPLOG_SIZE = 200 };
    struct AliasCursor
    {
        CliMapRange iter_range;
        AliasCursor(const std::string& key_beg);
        AliasCursor(const std::string& key_beg, const std::string& key_end);
        CliBase* pop(bool forceFindEach=false);
        bool empty(void);
    };

public:
    // 别名引用相关的操作
    int addChild( HEpBase* chd );
    int addChild( CliBase* child, bool inCtrl = true );
    int addAlias2Child( const std::string& asname, CliBase* ptr );

    void removeAliasChild( const std::string& asname );
    void removeAliasChild( CliBase* ptr, bool rmAll );

    CliBase* getChildByName( const std::string& asname );
    CliBase* getChildBySvrid( int svrid );
    CliInfo* getCliInfo( CliBase* child );
    map<CliBase*, CliInfo>* getAllChild() { return &m_children; }

    // era相关
    std::string getLocalClisEraString( void );
    std::vector<nlohmann::json> getLocalAllCliJson();
    std::vector<nlohmann::json> getLocalCliJsonByDiffera(const std::string& differa);
    std::string diffOuterCliEra( const std::string& erastr );


    void updateCliTime( CliBase* child );

    // 获取一个范围的CliBase*
    // 使用AliasCursor struct

    // 自定义属性的操作
    void setProperty( CliBase* dst, const std::string& key, const std::string& val );
    std::string getProperty( CliBase* dst, const std::string& key );

    // 退出处理
    int progExitHanele( int flg );
    // 获取当前对象状态
    std::string selfStat( bool incAliasDetail );

    void addCliCloseConsumerFunc( CliPreCloseNotifyFunc func );
    int onChildEvent( int evtype, va_list ap );

private:
    CliMgr() = default;
    ~CliMgr(void);

protected:
    map<CliBase*, CliInfo> m_children;
    map<std::string, CliBase*> m_aliName2Child;
    vector<CliPreCloseNotifyFunc> m_cliCloseConsumer; // 客户关闭事件消费者

    std::unique_ptr<IOHand> m_waitRmPtr;
    int m_localEra = 0;
};

#endif
