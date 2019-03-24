#include "clibase.h"
#include "comm/strparse.h"
#include "comm/sock.h"
#include "cloud/exception.h"
#include <sys/epoll.h>
#include <cstring>
#include <cerrno>
#include "rapidjson/json.hpp"
#include "cloud/const.h"

HEPCLASS_IMPL(CliBase, CliBase)
int CliBase::s_my_svrid = 0;

// static
int CliBase::Init( int mysvrid )
{
	s_my_svrid = mysvrid;
	return 0;
}

CliBase::CliBase(void): m_cliType(0), m_isLocal(true), m_era(0)
{
    
}
CliBase::~CliBase(void)
{
}


int CliBase::run( int p1, long p2 )
{
	LOGERROR("CLIBASE| msg=unexcept run");
	return -1;
}

int CliBase::onEvent( int evtype, va_list ap )
{
	LOGERROR("CLIBASE| msg=unexcept onEvent");
	return -2;
}


void CliBase::setProperty( const string& key, const string& val )
{
	m_cliProp[key] = val;
}

void CliBase::setProperty( const string& key, int val )
{
	m_cliProp[key] = std::to_string(val);
}

string CliBase::getProperty( const string& key ) const
{
	auto itr = m_cliProp.find(key);
	if (itr != m_cliProp.end())
	{
		return itr->second;
	}

	return "";
}


int CliBase::Json2Map( const Value* objnode )
{
	IFRETURN_N(!objnode->IsObject(), -1);
	int ret = 0;
    Value::ConstMemberIterator itr = objnode->MemberBegin();
    for (; itr != objnode->MemberEnd(); ++itr)
    {
        const char* key = itr->name.GetString();
        if (itr->value.IsString())
        {
        	const char* val = itr->value.GetString();
			setProperty(key, val);
        }
		else if (itr->value.IsInt())
		{
			auto val = std::to_string(itr->value.GetInt());
			setProperty(key, val);
		}
    }

    return ret;
}

int CliBase::Json2Map(const nlohmann::json &obj)
{
	IFRETURN_N(!obj.is_object(), -1);
	for (const auto &[key, val] : obj.items())
		setProperty(key, (std::string)val);
	return 0;
}

void CliBase::setIntProperty( const string& key, int val )
{
	m_cliProp[key] = std::to_string(val);
}

void CliBase::updateEra( void )
{
	m_era = 1 + m_era;
}

int CliBase::getIntProperty( const string& key ) const
{
	return atoi(getProperty(key).c_str());
}

nlohmann::json CliBase::serialize() const
{
	nlohmann::json obj;
	for (auto it = m_cliProp.begin(); it != m_cliProp.end(); ++it)
		obj[it->first] = it->second;
	return obj;
}

int CliBase::unserialize( const Value* rpJsonValue )
{
	int ret = Json2Map(rpJsonValue);
	m_era = getIntProperty("ERAN");
	if (0 == m_cliType)
	{
		m_cliType = getIntProperty(CLIENT_TYPE_KEY);
		if (1 == m_cliType)
		{
			LOGERROR("CLIUNSERIALIZE| msg=%s cannot set to clitype=1", m_idProfile.c_str());
			m_cliType = 99999999;
		}
	}
	return ret;
}

int CliBase::unserialize(const nlohmann::json &obj)
{
	int ret = Json2Map(obj);
	m_era = getIntProperty("ERAN");
	if (0 == m_cliType)
	{
		m_cliType = getIntProperty(CLIENT_TYPE_KEY);
		if (1 == m_cliType)
		{
			m_cliType = 99999999;
		}
	}
	return ret;
}
