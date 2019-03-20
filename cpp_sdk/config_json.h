/*-------------------------------------------------------------------------
FileName     : config_json.h
Description  : 分布式配置客户端类
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-10-30       create     hejl 
-------------------------------------------------------------------------*/
#ifndef _CONFIG_JSON_H_
#define _CONFIG_JSON_H_
#include "comm/public.h"
#include <shared_mutex>
#include <string>
#include <vector>
#include <map>
#include "rapidjson/json.hpp"

class ConfJson
{
public:
    ConfJson( const std::string& fname );
    int update( const Value* data );

    int query( int& oval, const std::string& qkey, bool wideVal ) const;
    int query( string& oval, const std::string& qkey, bool wideVal ) const;
    int query( std::map<std::string, std::string>& oval, const std::string& qkey, bool wideVal ) const;
    int query( std::map<std::string, int>& oval, const std::string& qkey, bool wideVal ) const;
    int query( std::vector<std::string>& oval, const std::string& qkey, bool wideVal ) const;
    int query( std::vector<int>& oval, const std::string& qkey, bool wideVal ) const;

    time_t getMtime( void ) const;

private:
    inline const Value* _findNode( const std::string& qkey ) const;
    inline int _parseVal( int& oval, const Value* node, bool wideVal ) const;
    inline int _parseVal( std::string& oval, const Value* node, bool wideVal ) const;

    template<class ValT>
    int queryMAP( std::map<std::string, ValT>& oval, const std::string& qkey, bool wideVal ) const;
    template<class ValT>
    int queryVector( std::vector<ValT>& oval, const std::string& qkey, bool wideVal ) const;

    
private:
    time_t m_mtime;
    Document m_doc;
    std::string m_fname;
    mutable std::shared_mutex m_rwLock;
};

#endif
