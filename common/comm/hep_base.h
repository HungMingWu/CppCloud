/*-------------------------------------------------------------------------
FileName     : hep_base.h
Description  : hepoll运行具体任务对象
remark       :
Modification :
--------------------------------------------------------------------------
   1、Date  2018-01-23       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _HEP_BASE_H_
#define _HEP_BASE_H_
#include <string>
#include <stdarg.h>
#include <functional>
#include "public.h"
#include "i_taskrun.h"

// epoll 文件描述符和epoll_event状态
struct HEpEvFlag
{
    int m_epfd;
    int m_actFd;
    int m_eventFg;  // use: EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT
    ITaskRun2* m_ptr;

    HEpEvFlag( void );
    ~HEpEvFlag( void );
    void oneShotUpdate( void );
	
	void setEPfd( int epfd );
	int setActFd( int actfd );

    int addEvt( int eventFg );
    int addEvt( int eventFg, ITaskRun2* ptr );
    int rmEvt( int eventFg );
    int setEvt( int eventFg, ITaskRun2* ptr ); // eventFg=0 remove ev
};


#define HEPCLASS_DECL(clsname, regname)              \
    clsname(const char* name) ;                      \
    const char* name( void ) { return #regname; }    \
    virtual HEpBase* clone(void){return new clsname;}

#define HEPCLASS_IMPL(clsname, regname)              \
    static clsname g_##clsname(#regname);            \
    clsname::clsname(const char* name): HEpBase(name) {}

#define HEPMUTICLASS_IMPL(clsname, regname, basecls) \
    static clsname g_##clsname(#regname);            \
    clsname::clsname(const char* name): basecls(name) {}

#define HEPCLASS_IMPL_FUNC(clsname, static_method_name)    \
    static clsname g_##clsname("_"); \
    clsname::clsname(const char* name): HEpBase(name) \
    {RegisterFunc(#clsname "::" #static_method_name, &clsname::static_method_name);}

// 注册多个func (注意链接顺序：hep_base.o放于g++文件列表的右边, 因右边的比左边的先初始化)
#define HEPCLASS_IMPL_FUNCX_BEG(clsname) \
    static struct Reg##clsname {\
        Reg##clsname(){

#define HEPCLASS_IMPL_FUNCX_MORE(clsname, member_method_name);    \
    HEpBase::RegisterFunc(#clsname "::" #member_method_name, \
		    std::bind(&clsname::member_method_name, this, \
			    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

#define HEPCLASS_IMPL_FUNCX_MORE_S(clsname, static_method_name);    \
    HEpBase::RegisterFunc(#clsname "::" #static_method_name, \
                    std::bind(&clsname::static_method_name, \
                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

#define HEPCLASS_IMPL_FUNCX_END(clsname)    \
    }} g_reg_##clsname;

#define HEPCLS_STATIC_TIMER_FUN(clsname, qidx, delay_ms) \
    FlowCtrl::Instance()->appendTask(&g_##clsname, qidx, delay_ms)

    
enum NOTIFY_EVENT_TYPE
{
    HEPNTF_INIT_PARAM = 1,
    HEPNTF_SOCK_CLOSE = 2,
    HEPNTF_NOTIFY_CHILD  = 3,
    HEPNTF_SET_ALIAS  = 4, // X
    HEPNTF_SEND_MSG   = 5,
    HEPNTF_SET_EPOUT  = 6,
    HEPNTF_GET_PROPT  = 7, // X
    HEPNTF_GET_PROPT_JSONALL = 8, // X
    HEPNTF_INIT_FINISH=9,
    HEPNTF_UPDATE_TIME = 10,
    
	/*
	HEFG_READ = 201,
	HEFG_WRITE = 202,
    HEFG_RDWR = 203, // = (HEFG_READ&HEFG_WRITE)
	HEFG_OTHER, 
    */
    HEFG_PEXIT = 300,
	HEFG_QUEUERUN,
	HEFG_QUEUEEXIT,
    HEFG_PROGEXIT = HEFG_QUEUEEXIT, // 进程收到退出信号时Notify通知
};


class HEpBase: public ITaskRun2
{
 public:
    using ProcOneFunT = std::function<int(void*, unsigned, void*)>;
    static void RegisterClass(const char* regname, HEpBase* stdptr);
    static void RegisterFunc(const char* regname, ProcOneFunT func);
    static ProcOneFunT GetProcFunc(const char* regname);

    static HEpBase* New(const char* regname);
    static void BindSon(HEpBase* parent, HEpBase* son);
    static int Notify( HEpBase* dst, int evtype, ... );

    static int SendMsg(HEpBase* dst, unsigned int cmdid, unsigned int seqid, const std::string& strda, bool setOutAtonce=true);
    static int SendMsg(HEpBase* dst, unsigned int cmdid, unsigned int seqid, bool setOutAtonce, const char* bodyfmt, ...);
    static int SendMsgEasy(HEpBase* dst, unsigned int cmdid, unsigned int seqid, int ecode, const std::string& desc, bool setOutAtonce=true);

public:
    HEpBase( const char* name ); // macro已实现
    HEpBase( void );
    virtual ~HEpBase( void );

    // interface ITaskRun2
    virtual int run( int flag, long p2 );
    virtual int qrun( int flag, long p2 );
    
protected:
    virtual const char* name( void ) = 0; // macro已实现
    virtual HEpBase* clone( void ) = 0; // macro已实现

//#define Notify(dstptr, evtype, ...)  dstptr->onEvent(evtype, ##__VA_ARGS__); 
    // 事件通知, 由Notify()发出通知
    virtual int onEvent( int evtype, va_list ap );
	virtual int setParam(int param1, int param2);

    int transEvent( HEpBase* dst, int evtype, va_list ap );
    
public:
    std::string m_idProfile; // 客户端标识字符串,用于日志跟踪

protected:
    HEpBase* m_parent;
    HEpBase* m_child;

};


#endif
