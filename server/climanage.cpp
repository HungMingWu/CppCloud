#include "climanage.h"
#include "comm/strparse.h"
#include "comm/timef.h"
#include "act_mgr.h"
#include "broadcastcli.h"
#include "cloud/const.h"

const char g_jsonconf_file[] = ".scomm_svrid.txt";

CliMgr::AliasCursor::AliasCursor( const std::string& key_beg ):
	iter_range(CliMgr::Instance()->m_aliName2Child, key_beg, key_beg+"~")
{
}

CliMgr::AliasCursor::AliasCursor( const std::string& key_beg, const std::string& key_end ):
	iter_range(CliMgr::Instance()->m_aliName2Child, key_beg, key_end)
{
}

CliBase* CliMgr::AliasCursor::pop(bool forceFindEach)
{
	return iter_range.pop(forceFindEach);
}

bool CliMgr::AliasCursor::empty(void)
{
	return iter_range.empty();
}

CliMgr::~CliMgr(void)
{
 	map<CliBase*, CliInfo>::iterator it = m_children.begin();
	for (; it != m_children.end(); ++it)
	{
		delete it->first;
	}

	m_children.clear();
	m_aliName2Child.clear();
}

int CliMgr::addChild( HEpBase* chd )
{
	CliBase* child = dynamic_cast<CliBase*>(chd);
	return child? addChild(child) : -22;
}

int CliMgr::addChild( CliBase* child, bool inCtrl )
{
	CliInfo& cliinfo = m_children[child];
	ERRLOG_IF1(cliinfo.t0 > 0, "ADDCLICHILD| msg=child has exist| newchild=%p| t0=%d", child, (int)cliinfo.t0);
	cliinfo.t0 = time(NULL);
	cliinfo.inControl = inCtrl;
	return 0;
}

int CliMgr::addAlias2Child( const std::string& asname, CliBase* ptr )
{
	int ret = 0;

	map<CliBase*, CliInfo>::iterator it = m_children.find(ptr);
	if (m_children.end() == it) // 非直接子对象，暂时不处理
	{
		LOGWARN("ADDALIASCHILD| msg=ptr isnot listen's child| name=%s| ptr=%p", asname.c_str(), ptr);
		return -11;
	}

	it->second.aliasName[asname] = true;

	CliBase*& secondVal = m_aliName2Child[asname];
	if (NULL != secondVal && ptr != secondVal) // 已存在旧引用的情况下，要先清理掉旧的设置
	{
		LOGWARN("ADDALIASCHILD| msg=alias child name has exist| name=%s", asname.c_str());
		map<CliBase*, CliInfo>::iterator itr = m_children.find(secondVal);
		if (m_children.end() != itr)
		{
			itr->second.aliasName.erase(asname);
		}
		
		ret = 1;
	}
	
	secondVal = ptr;

	return ret;
}

void CliMgr::removeAliasChild( const std::string& asname )
{
	auto it = m_aliName2Child.find(asname);
	if (m_aliName2Child.end() != it)
	{
		map<CliBase*, CliInfo>::iterator itr = m_children.find(it->second);
		if (m_children.end() != itr)
		{
			itr->second.aliasName.erase(asname);
		}

		m_aliName2Child.erase(it);
	}
}

void CliMgr::removeAliasChild( CliBase* ptr, bool rmAll )
{
	map<CliBase*, CliInfo>::iterator it = m_children.find(ptr);
	if (it != m_children.end()) // 移除所有别名引用
	{
		std::string asnamestr;
		CliInfo& cliinfo = it->second;
		for (auto itr = cliinfo.aliasName.begin(); itr != cliinfo.aliasName.end(); ++itr)
		{
			asnamestr.append( asnamestr.empty()?"":"&" ).append(itr->first);
			m_aliName2Child.erase(itr->first);
		}

		cliinfo.aliasName.clear();

		if (rmAll) // 移除m_children指针
		{
			if (!m_cliCloseConsumer.empty())
			{
				vector<CliPreCloseNotifyFunc>::iterator itrConsumer = m_cliCloseConsumer.begin();
				for (; itrConsumer != m_cliCloseConsumer.end(); ++itrConsumer)
				{
					(*itrConsumer)(it->first);
				}
			}

			m_children.erase(it);
			if (ptr != (CliBase*)m_waitRmPtr.get())
			{
				m_waitRmPtr.reset(); // 清理前一待删对象(为避免同步递归调用)
			}

			LOGINFO("CliMgr_CHILDRM| msg=a iohand close| life=%ds| asname=%s", int(time(NULL)-cliinfo.t0), asnamestr.c_str());
			if (it->second.inControl)
			{
				m_waitRmPtr.reset(static_cast<IOHand*>(ptr));
			}
		}
	}
}

// 分布式的情况下cli有多种对象，所以以不同后缀区别
CliBase* CliMgr::getChildBySvrid( int svrid )
{
	static const char* svr_subfix[] = {"_C", "_S", "_I", "_s", "_i", NULL};
	CliBase* ret = NULL;
	std::string strsvrid = std::to_string(svrid);

	for (int i = 0; NULL != svr_subfix[i] && NULL == ret; ++i)
	{
		ret = getChildByName(strsvrid + svr_subfix[i]);
	}

	return ret;
}

CliBase* CliMgr::getChildByName( const std::string& asname )
{
	CliBase* ptr = NULL;
	auto it = m_aliName2Child.find(asname) ;
	if (it != m_aliName2Child.end())
	{
		ptr = it->second;
	}
	return ptr;
}

CliInfo* CliMgr::getCliInfo( CliBase* child )
{
	map<CliBase*, CliInfo>::iterator itr = m_children.find(child);
	return (itr!=m_children.end() ? &itr->second : NULL);
}

/**
 * era格式: svrid:erano:atime = 10:2:15412341234 
 */
std::string CliMgr::getLocalClisEraString( void )
{
	std::string ret;
	map<CliBase*, CliInfo>::iterator it = m_children.begin();
	for (; it != m_children.end(); ++it)
	{
		CliBase* ptr = it->first;
		if (ptr->isLocal() && ptr->getCliType() > 1)
		{
			StrParse::AppendFormat(ret, "%s:%d:%d ", 
				ptr->getProperty(CONNTERID_KEY).c_str(), ptr->m_era, it->second.t1);
		}
	}

	return ret;
}

std::vector<nlohmann::json> CliMgr::getLocalCliJsonByDiffera(const std::string& differa)
{
	std::vector<nlohmann::json> result;

	vector<std::string> vecitem;
	StrParse::SpliteStr(vecitem, differa, ' ');
	for (auto itim = vecitem.begin(); itim != vecitem.end(); ++itim)
	{
		vector<std::string> vecsvr;
		StrParse::SpliteStr(vecsvr, *itim, ':');
		if (3 == vecsvr.size())
		{
			std::string& svrid = vecsvr[0];
			//int svrera = atoi(vecsvr[1].c_str());
			//int svratime = atoi(vecsvr[2].c_str());

			CliBase* ptr = getChildByName(svrid + "_I");
			if (ptr)
			{
				result.push_back(ptr->serialize());
				result.back()["ERAN"] = ptr->m_era;
			}
		}
	}
	return result;
}

std::vector<nlohmann::json> CliMgr::getLocalAllCliJson()
{
	std::vector<nlohmann::json> result;
	for (auto it = m_children.begin(); it != m_children.end(); ++it)
	{
		CliBase* ptr = it->first;
		if (ptr->isLocal() && ptr->getCliType() > 1)
		{
			result.push_back(ptr->serialize());
			result.back()["ERAN"] = ptr->m_era;
		}
	}

	return result;
}

void CliMgr::updateCliTime( CliBase* child )
{
	CliInfo* clif = getCliInfo(child);
	ERRLOG_IF1RET(NULL==clif, "UPDATECLITIM| msg=null pointer at %p", child);
	std::string uniqstr = child->isLocal()? child->getProperty("fd"): StrParse::Format("%p",child);
	time_t now = time(NULL);

	if (uniqstr.empty()) return;
	if (now < clif->t1 + 20) return; // 超20sec才刷新

	if (clif->t1 >= clif->t0)
	{
		std::string atimkey = StrParse::Format("%s%ld@", CLI_PREFIX_KEY_TIMEOUT, clif->t1);
		atimkey += uniqstr;
		removeAliasChild(atimkey);
	}

	clif->t1 = now;
	child->setIntProperty("atime", now);
	std::string atimkey = StrParse::Format("%s%ld@", CLI_PREFIX_KEY_TIMEOUT, clif->t1);
	atimkey += uniqstr;
	int ret = addAlias2Child(atimkey, child);
	ERRLOG_IF1(ret, "UPDATECLITIM| msg=add alias atimkey fail| ret=%d| mi=%s", ret, child->m_idProfile.c_str());
}

void CliMgr::setProperty( CliBase* dst, const std::string& key, const std::string& val )
{
	dst->m_cliProp[key] = val;
}

std::string CliMgr::getProperty( CliBase* dst, const std::string& key )
{
	return dst->getProperty(key);
}

int CliMgr::onChildEvent( int evtype, va_list ap )
{
	int ret = -100;
	if (HEPNTF_SOCK_CLOSE == evtype) // 删除时要注意避免递归
	{
		IOHand* son = va_arg(ap, IOHand*);
		int clitype = va_arg(ap, int);

		ERRLOG_IF1RET_N(NULL==son || m_children.find(son)==m_children.end(), -101, 
		"CHILDEVENT| msg=HEPNTF_SOCK_CLOSE ev-param invalid| son=%s| clitype=%d", son->m_idProfile.c_str(), clitype); 

		CliInfo& cliinfo = m_children[son];
		cliinfo.t2 = time(NULL);
		Actmgr::Instance()->appCloseFound(son, clitype, cliinfo);

		// 广播通知其他Serv某一cli下线
		std::string broadcast_msg = _F("{\"%s\":[%d]}", UPDATE_CLIPROP_DOWNKEY, son->getIntProperty(CONNTERID_KEY));
		removeAliasChild(son, true);
		son = NULL;
		BroadCastCli::Instance()->toWorld(broadcast_msg, CMD_UPDATEERA_REQ, 0, false);
		ret = 0;
	}
	else if (HEPNTF_SET_ALIAS == evtype)
	{
		IOHand* son = va_arg(ap, IOHand*);
		const char* asname = va_arg(ap, const char*);
		ret = addAlias2Child(asname, son);
	}

	return ret;
}

void CliMgr::addCliCloseConsumerFunc( CliPreCloseNotifyFunc func )
{
	if (func)
	{
		m_cliCloseConsumer.push_back(func);
	}
}

// ep线程调用此方法通知各子对象退出
int CliMgr::progExitHanele( int flg )
{
	LOGDEBUG("CLIMGREXIT| %s", selfStat(true).c_str());
	for (auto it = m_children.begin(); it != m_children.end(); it++)
		if (it->first->isLocal())
			it->first->run(HEFG_PEXIT, 2); /// #PROG_EXITFLOW(5)


	LOGDEBUG("CLIMGREXIT| %s", selfStat(true).c_str());
	return 0;
}

std::string CliMgr::selfStat( bool incAliasDetail )
{
	std::string adetail;

	if (incAliasDetail)
	{
		auto it = m_aliName2Child.begin();
		for (; it != m_aliName2Child.end(); ++it)
		{
			adetail += it->first;
			StrParse::AppendFormat(adetail, "->%p ", it->second);
		}
	}

	return StrParse::Format("child_num=%zu| era=%d| wptr=%p| alias_num=%zu(%s)", 
		m_children.size(), m_localEra, m_waitRmPtr.get(), m_aliName2Child.size(), adetail.c_str());
}	
