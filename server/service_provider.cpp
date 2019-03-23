#include "service_provider.h"
#include "clibase.h"
#include "rapidjson/json.hpp"
#include "cloud/const.h"
#include "comm/strparse.h"

ServiceItem::ServiceItem( void ): pvd_ok(0), pvd_ng(0), ivk_ok(0), ivk_ng(0)
{

}

int ServiceItem::parse0( const std::string& regname, CliBase* cli, int prvdid )
{
	svrid = cli->getIntProperty("svrid");
	ERRLOG_IF1RET_N(regname.empty() || svrid <= 0, -70, 
		"SERVITEMPARSE| msg=parse fail| cli=%s", cli->m_idProfile.c_str());

	this->regname = regname;
	this->regname2 = _F("%s%s%%%d", SVRPROP_PREFIX, regname.c_str(), prvdid);
	this->prvdid = prvdid;
	idc = cli->getIntProperty("idc");
	rack = cli->getIntProperty("rack");
	islocal = cli->isLocal();
	return 0;
}

int ServiceItem::parse( CliBase* cli )
{
	// 固定部份-不会随运行过程改变的
	if (0 == version || 0 == protocol || url.empty())
	{
		url = cli->getProperty(regname2 + ":url");
		protocol = cli->getIntProperty(regname2 + ":protocol");
		version = cli->getIntProperty(regname2 + ":version");
	}

	// 变化部分
	desc = cli->getProperty(regname2 + ":desc");
	//okcount = cli->getIntProperty(regname2 + ":okcount");
	//ngcount = cli->getIntProperty(regname2 + ":ngcount");
	weight = cli->getIntProperty(regname2 + ":weight");
	enable = cli->getIntProperty(regname2 + ":enable");

	if (prvdid > 0)
	{
		int ntmp = 0;
		if ( (ntmp = cli->getIntProperty(regname2 + "idc")) )
		{
			idc = ntmp;
		}
		if ( (ntmp = cli->getIntProperty(regname2 + "rack")) )
		{
			rack = ntmp;
		}
	}

	return 0;
}

nlohmann::json ServiceItem::getJsonStr(int oweight) const
{
	return {
		{"regname", regname},
		{"url", url},
		{"desc", desc},
		{"svrid", svrid},
		{"prvdid", prvdid},
		{"pvd_ok", pvd_ok},
		{"pvd_ng", pvd_ng},
		{"protocol", protocol},
		{"version", version},
		{"idc", idc},
		{"rack", rack},
		{"enable", enable},
		{"weight", oweight > 0 ? oweight : weight}
	};
}

int ServiceItem::score( short oidc, short orack ) const
{
	static const int match_mult1 = 4;
	static const int match_mult2 = 8;
	int score = 0;
	int calc_weight = this->weight>0? this->weight : 1;

	if (oidc > 0 && oidc == this->idc) // 同一机房
	{
		score += (orack > 0 && orack == this->rack)? calc_weight*match_mult2 : calc_weight*match_mult1;
	}

	if (this->islocal) // 优先同一注册中心的服务
	{
		score += calc_weight;
	}

	score += calc_weight;
	score += (rand()&0x7);
	return score;
}

ServiceProvider::ServiceProvider( const std::string& svrName ): m_regName(svrName)
{

}

ServiceProvider::~ServiceProvider( void )
{
	auto itr = m_svrItems.begin();
	for (; itr != m_svrItems.end(); ++itr)
	{
		//CliBase* first = itr->first;
		for (auto iit : itr->second)
		{
			IFDELETE(iit.second);
		}
	}
}

bool ServiceProvider::hasItem( CliBase* cli, int prvdid ) const
{
	auto itfind = m_svrItems.find(cli);
	if (itfind != m_svrItems.end())
	{
		auto second = itfind->second;
		return (second.find(prvdid) != second.end());
	}

	return false;
}

int ServiceProvider::setItem( CliBase* cli, int prvdid )
{
	ServiceItem*& pitem = m_svrItems[cli][prvdid];

	if (NULL == pitem)
	{
		std::string regname2 = _F("%s%s%%%d", SVRPROP_PREFIX, m_regName.c_str(), prvdid);

		pitem = new ServiceItem;
		int ret = pitem->parse0(m_regName, cli, prvdid);

		if (ret)
		{
			IFDELETE(pitem);
			m_svrItems[cli].erase(prvdid);
			return ret;
		}

		// 设置属性标记，以便在广播给其他serv时能够还原提供的服务
		std::string provval = cli->getProperty(SVRPROVIDER_CLI_KEY);
		cli->setProperty( SVRPROVIDER_CLI_KEY, 
			provval.empty()? regname2: (regname2 + "+" + provval) );
	}
	
	return  pitem->parse(cli);
}

void ServiceProvider::setStat( CliBase* cli, int prvdid, int pvd_ok, int pvd_ng, int ivk_dok, int ivk_dng )
{
	auto itr = m_svrItems.find(cli);
	if (itr != m_svrItems.end())
	{
		auto itr2 = itr->second.find(prvdid);
		if ( itr2 != itr->second.end() )
		{
			std::string regname2 = _F("%s%s%%%d", SVRPROP_PREFIX, m_regName.c_str(), prvdid);
			ServiceItem* itm = itr2->second;

			#define IFUPDATESTAT(name) if (name > 0) { itm->name = name; cli->setIntProperty(regname2+":" #name, name); }
			#define IFUPDATESTAT_DELTA(param, memName) if (param > 0) { itm->memName += param; \
				cli->setIntProperty(regname2+":" #memName, param + cli->getIntProperty(regname2+":" #memName)); }

			IFUPDATESTAT(pvd_ok);
			IFUPDATESTAT(pvd_ng);
			IFUPDATESTAT_DELTA(ivk_dok, ivk_ok);
			IFUPDATESTAT_DELTA(ivk_dng, ivk_ok);
			// keyname example "prvd_httpApp1%9"
		}
	}
}


bool ServiceProvider::removeItme( CliBase* cli )
{
	bool ret = false;
	auto itr = m_svrItems.find(cli);
	if (itr != m_svrItems.end())
	{
		for (auto itMap : itr->second)
		{
			IFDELETE(itMap.second);
		}
		
		m_svrItems.erase(itr);
		ret = true;
	}

	return ret;
}

// 返回json-array[]形式
std::vector<nlohmann::json> ServiceProvider::getAllJson() const
{
	std::vector<nlohmann::json> result;
	for (auto itr : m_svrItems)
		for (auto itMap : itr.second)
			result.push_back(itMap.second->getJsonStr());
	return result;
}

/**
 * @summery: 消息者查询可用服务列表
 * @remark: 调用时采用客户端负载均衡方式
 * @return: 返回可用服务个数
 * @param: strjson [out] 返回json字符串[array格式]
 **/
std::vector<nlohmann::json> ServiceProvider::query(short idc, short rack, short version, short limit) const
{
	// sort all item by score
	std::map<int, ServiceItem*> sortItemMap;
	auto itr = m_svrItems.begin();
	for (; itr != m_svrItems.end(); ++itr)
	{
		for (auto itMap : itr->second)
		{
			ServiceItem* ptr = itMap.second;
			if (!ptr->valid()) continue;
			if (version > 0 && version != ptr->version) continue;

			int score = ptr->score(idc, rack);
			ptr->tmpnum = score;
			while (NULL != sortItemMap[score])
			{
				score++;
			}
			sortItemMap[score] = ptr;
		}

	}

	std::vector<nlohmann::json> result;
	auto ritr = sortItemMap.rbegin();
	for (; result.size() < limit && sortItemMap.rend() != ritr; ++ritr)
	{
		ServiceItem* ptr = ritr->second;
		result.push_back(ptr->getJsonStr(ptr->tmpnum));
	}

	return result;
}
