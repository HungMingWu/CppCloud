/*-------------------------------------------------------------------------
FileName     : shttp_invoker_mgr.h
Description  : 简单http协议调用
remark       : 线程安全
Modification :
--------------------------------------------------------------------------
   1、Date  2018-11-06       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _SHTTP_INVOKER_MGR_H_
#define _SHTTP_INVOKER_MGR_H_
#include "comm/public.h"
#include <string>

class SHttpInvokerMgr
{
	SINGLETON_CLASS2(SHttpInvokerMgr)
	SHttpInvokerMgr() = default;
	~SHttpInvokerMgr() = default;

public:
	void setLimitCount( int n );

	// 向服务提供者发出请求，并等待响应回复 （同步）
	int get( std::string& resp, const std::string& path, const std::string& qstr, const std::string& svrname );
	int post( std::string& resp, const std::string& path, const std::string& reqbody, const std::string& svrname );


private:
	std::string adjustUrlPath( const std::string& url, const std::string& path ) const;

private:
	int m_eachLimitCount = 5;
	int m_invokerTimOut_sec = 3;
};

#endif
