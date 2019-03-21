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
#include <vector>
#include <string>

using std::map;
using std::vector;

typedef void (*CliPreCloseNotifyFunc)( IOHand* );
typedef MapRanger<std::string, IOHand*> CliMapRange;

struct CliInfo
{
    time_t t0 = 0, t1 = 0, t2 = 0; // 连接时间，活动时间，关闭时间
    bool inControl = true; // 如果为true,当调用removeAliasChild删除时会delete对象
    std::map<std::string, bool> aliasName; // 客户端的别名，其他人可以通过该别人找到此对象
    std::map<std::string, std::string>* cliProp = nullptr; // 客户属性
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
        IOHand* pop(bool forceFindEach=false);
        bool empty(void);
    };

public:
    // 别名引用相关的操作
    int newChild( int clifd, int epfd );
    int addChild( IOHand* child, bool inCtrl = true );
    int addAlias2Child( const std::string& asname, IOHand* ptr );

    void removeAliasChild( const std::string& asname );
    void removeAliasChild( IOHand* ptr, bool rmAll );

    IOHand* getChildByName( const std::string& asname );
    IOHand* getChildBySvrid( int svrid );
    CliInfo* getCliInfo( IOHand* child );
    map<IOHand*, CliInfo>* getAllChild() { return &m_children; }
    int getLocalAllCliJson( std::string& jstr );

    void updateCliTime( IOHand* child );


    // 自定义属性的操作
    void setProperty( IOHand* dst, const std::string& key, const std::string& val );
    std::string getProperty( IOHand* dst, const std::string& key );

    // 退出处理
    int progExitHanele( int flg );
    int onChildEvent( int evtype, va_list ap );
    static int OnChildEvent( int evtype, va_list ap );
    // 获取当前对象状态
    std::string selfStat( bool incAliasDetail );


private:
    CliMgr(void);
    ~CliMgr(void);

protected:
    map<IOHand*, CliInfo> m_children;
    map<std::string, IOHand*> m_aliName2Child;
    vector<CliPreCloseNotifyFunc> m_cliCloseConsumer; // 客户关闭事件消费者

    IOHand* m_waitRmPtr;
};

#endif
