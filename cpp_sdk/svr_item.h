/*-------------------------------------------------------------------------
FileName     : svr_item.h
Description  : 一项服务提供者信息
remark       : 
Modification :
--------------------------------------------------------------------------
   1、Date  2018-11-07       create     hejl 
-------------------------------------------------------------------------*/

#ifndef _SVR_ITEM_H_
#define _SVR_ITEM_H_
#include <string>

struct svr_item_t
{
    std::string regname;
    std::string url;
    std::string version;
    std::string host;
    
    int port = 0;
    int svrid = 0;
    int prvdid = 0;
    short protocol = 0; // tcp=1 udp=2 http=3 https=4
    short weight = 0;

    bool parseUrl( void );
};

#endif
