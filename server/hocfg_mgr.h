/*-------------------------------------------------------------------------
FileName     : hocfg_mgr.h
Description  : 分布式配置管理
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-10-08       create     hejl 
-------------------------------------------------------------------------*/

#ifndef _HOCFG_MGR_H_
#define _HOCFG_MGR_H_
#include <string>
#include <map>
#include "comm/public.h"
#include "comm/json.hpp"
#include "rapidjson/json.hpp"

class IOHand;

struct AppConfig
{
	Document doc;
	time_t mtime;
	bool isDel;

	AppConfig(void):mtime(0), isDel(false) {}
};

class HocfgMgr
{
	SINGLETON_CLASS2(HocfgMgr)
	HocfgMgr( void );
	~HocfgMgr( void );

public:
	int OnSetConfigHandle( void* ptr, unsigned cmdid, void* param );
	int OnGetAllCfgName( void* ptr, unsigned cmdid, void* param );
	int OnCMD_HOCFGNEW_REQ( void* ptr, unsigned cmdid, void* param );
	int OnCMD_BOOKCFGCHANGE_REQ( void* ptr, unsigned cmdid, void* param );

public:
	int init( const std::string& conf_root );
	void uninit( void );

	int query( std::string& result, const std::string& file_pattern, const std::string& key_pattern, bool incBase ) const;
	nlohmann::json getAllCfgNameJson( int filter_flag = 2 ) const;
	int getCfgMtime( const std::string& file_pattern, bool incBase ) const;

	// 分布式配置互相同步最新配置
	int compareServHoCfg( int fromSvrid, const Value* jdoc );
	// cli初始时根据配置做一些自定制属性
	int setupPropByServConfig( IOHand* iohand ) const;

private:
	int loads( const std::string& dirpath );
	bool getBaseConfigName( std::string& baseCfg, const std::string& curCfgName ) const;
	AppConfig* getConfigByName( const std::string& curCfgName ) const;
	void remove( const std::string& cfgname, time_t mtime );

	int parseConffile( const std::string& filename, const std::string& contents, time_t mtime ); // 读出,解析
	int save2File(const std::string& filename, const std::string &doc) const;  // 持久化至磁盘

	int mergeJsonFile( Value* node0, const Value* node1, MemoryPoolAllocator<>& allc ) const; // 合并继承json
	int queryByKeyPattern( std::string& result, const Value* jdoc,
		const std::string& file_pattern, const std::string& key_pattern ) const; // 输入json-key返回对应value

	// 事件通知触发
	void notifyChange( const std::string& filename, int mtime ) const;

private:
	std::string m_cfgpath;
	std::map<std::string, AppConfig*> m_Allconfig;
	mutable int m_seqid;
};

#endif
