#include "svrprop.h"
#include "comm/strparse.h"
#include "comm/json.hpp"

SvrProp::SvrProp( void ): svrid(0), prvdid(0), tmpnum(0),
	 protocol(0),version(0), weight(0), idc(0), rack(0), islocal(false), enable(false)
{

}

bool SvrProp::valid( void ) const
{
	return enable && protocol > 0 && svrid > 0 && weight > 0 && !url.empty();
}

nlohmann::json SvrProp::jsonStr() const
{
	nlohmann::json obj{
		{"regname", regname},
		{"url", url},
		{"protocol", protocol},
		{"version", version},
		{"enable", enable}
	};
	if (!desc.empty()) obj["desc"] = desc;
	if (svrid > 0) obj["svrid"] = svrid;
	if (prvdid > 0) obj["prvdid"] = prvdid;
	if (weight > 0) obj["weight"] = weight;
	if (idc > 0) obj["idc"] = idc;
	if (rack > 0) obj["rack"] = rack;
	return obj;
}
