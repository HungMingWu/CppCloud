/*-------------------------------------------------------------------------
FileName     : service_provider.h
Description  : 服务提供者
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-10-16       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _SERVICE_PROVIDER_H_
#define _SERVICE_PROVIDER_H_
#include <string>
#include <map>
#include "cloud/svrprop.h"

class CliBase;

struct ServiceItem : public SvrProp
{
	std::string regname2;
	unsigned pvd_ok;
	unsigned pvd_ng;
	unsigned ivk_ok; // 调用者统计 全量
    unsigned ivk_ng; // 调用者统计 全量
	ServiceItem( void );

	int parse0( const std::string& name, CliBase* cli, int prvdid );
	int parse( CliBase* cli );
	void getJsonStr( std::string& strjson, int oweight = 0 ) const;
	void getCalcJson( std::string& strjson , int oweight) const;

	int score( short idc, short rack ) const;
};

typedef std::map<int, ServiceItem*> SVRITEM_MAP;

class ServiceProvider
{
public:
	ServiceProvider( const std::string& svrName );
	~ServiceProvider( void );

	bool hasItem( CliBase* cli, int prvdid ) const;

	int setItem( CliBase* cli, int prvdid );
	void setStat( CliBase* cli, int prvdid, int pvd_ok, int pvd_ng, int ivk_dok, int ivk_dng );
	bool removeItme( CliBase* cli );

	// 计算返回可用服务
	int query( std::string& jstr, short idc, short rack, short version, short limit ) const;
	int getAllJson( std::string& strjson ) const;

private:

private:
	const std::string m_regName;
	std::map<CliBase*, SVRITEM_MAP> m_svrItems;
};

#endif
