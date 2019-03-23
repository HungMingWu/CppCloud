#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "strparse.h"

// ��dv��Ϊ�ָ�����str�����õ��Ľ����ŵ�data����
int StrParse::SpliteInt(std::vector<int>& data, const std::string& str, char dv, int nildef)
{
    std::vector<std::string> vstr;

    SpliteStr(vstr, str, dv);
    auto it = vstr.begin();
    for (; it != vstr.end(); ++it)
    {
        if (it->empty())
        {
            data.push_back(nildef);
        }
        else
        {
            data.push_back(atoi(it->c_str()));
        }
    }

    return 0;
}

// ��dv��Ϊ�ָ�����str�����õ��Ľ����ŵ�data����
int StrParse::SpliteStr(std::vector<std::string>& data, const std::string& str, char dv)
{
    if (!str.empty())
    {
        const char* pstr = str.data();
        size_t len = str.size();
        return SpliteStr(data, pstr, len, dv);
    }

    return 0;
}

int StrParse::SpliteStr(std::vector<std::string>& data, const char* pstr, unsigned int len, char dv)
{
    if (NULL == pstr) return -1;

    unsigned int i = 0, begi = 0;
    for (; i < len; ++i)
    {
        if (pstr[i] == dv)
        {
            std::string item(pstr+begi, i-begi);
            data.push_back(item);

            begi = i + 1;
        }
    }

    {
        std::string item(pstr+begi, len-begi);
        data.push_back(item);
    }

    return 0;
}

// @summery: query_string��������, ����qstr��"key1=va11&key2=vv2&key3=123"��ʽ;
// @param: outPar [out] ���ս�Ҫ���outPar[key1]="val1"..
int StrParse::SpliteQueryString( std::map<std::string, std::string>& outPar, const std::string& querystr )
{
    int ret = 0;

    bool findkey = true;
    int begidx = 0;
    std::string key;
    std::string value;
    const char* qstr = querystr.data();
    size_t qslen = querystr.length();

    for (int i = 0; i < (int)qslen; )
    {
        if (findkey)
        {
            if ('=' == qstr[i])
            {
                key.assign(qstr+begidx, i-begidx);
                begidx = i + 1;
                findkey = false;
            }
        }
        else
        {
            if ('&' == qstr[i])
            {
                value.assign(qstr+begidx, i-begidx);
                begidx = i + 1;
                findkey = true;
                outPar[key] = value;
                //printf("[%s] -> %s\n", key.c_str(), value.c_str());
            }
        }

        // last part
        if (0 == qstr[++i])
        {
            if (findkey)
            {
                // skip
            }
            else
            {
                value.assign(qstr+begidx, i-begidx);
                outPar[key] = value;
                //printf("[%s] -> %s\n", key.c_str(), value.c_str());
            }

            break;
        }
    }

    return ret;
}

// 解析url各部分, proto://host:port/path?qparam
int StrParse::SplitURL(std::string& proto, std::string& host, int& port, std::string& path, std::string& qparam, const std::string& url)
{
    std::string::size_type pos1 = url.find("://");
    if (std::string::npos == pos1) return -1;
    proto = url.substr(0, pos1);
    
    std::string hostend = url.substr(pos1 + 3);
    std::string::size_type pos2 = hostend.find_first_of(":/");
    if (std::string::npos == pos2 || '/' == hostend[pos2])
    {
        if (proto == "http") port = 80;
        else if (proto == "https") port = 443;
        else return false;
    }
    else
    {
        std::string portend = hostend.substr(pos2 + 1);
        std::string::size_type pos3 = portend.find_first_of("/?");
        std::string strport = portend.substr(0, pos3);
        port = atoi(strport.c_str());
    }

    host = hostend.substr(0, pos2);
    std::string::size_type pos4 = hostend.find("?");
    std::string::size_type pos5 = hostend.find("/");
    if (std::string::npos != pos4)
    {
        qparam = hostend.substr(pos4 + 1);
    }

    if (std::string::npos != pos5)
    {
        path = hostend.substr(pos5, pos4 - pos5);
    }

    return 0;
}

int StrParse::PickOneJson(std::string& ostr, const std::string& src, const std::string& name)
{
    // תСд
    std::string tempstr;
    std::string tmpname;
    int ret = 1;
    bool found = false;
    size_t namelen = name.length();
    size_t len = src.length();
    const char* pchstr = src.data();
    const char* pchname = name.data();

    for (int i = 0; i < (int)len; ++i)
    {
        char rch = pchstr[i];
        if (rch >= 'A' && rch <= 'Z')
        {
            rch = 'a' + rch - 'A';
        }

        tempstr.append(1, rch);
    }

    for (int i = 0; i < (int)namelen; ++i)
    {
        char rch = pchname[i];
        if (rch >= 'A' && rch <= 'Z')
        {
            rch = 'a' + rch - 'A';
        }

        tmpname.append(1, rch);
    }
    

    do 
    {
        char chkch;
        size_t namepos = tempstr.find(tmpname);

        // �ҵ�һ����ȫƥ���key
        while (namepos != std::string::npos)
        {
            pchstr = tempstr.data();
            found = true;
            if (namepos > 0)
            {
                chkch = pchstr[namepos-1];
                if ((chkch >= 'a' && chkch <= 'z') ||
                    (chkch >= '0' && chkch <= '9') ||
                    ('_' == chkch || '-' == chkch) )
                {
                    found = false;
                }
            }

            if (found && namepos+namelen < len)
            {
                chkch = pchstr[namepos+namelen];
                if ((chkch >= 'a' && chkch <= 'z') ||
                    (chkch >= '0' && chkch <= '9') ||
                    ('_' == chkch || '-' == chkch) )
                {
                    found = false;
                }
            }

            if (found) break;

            namepos = tempstr.find(tmpname, namepos+namelen);
        }

        if (!found) break; // �Ҳ���key

        size_t valpos = tempstr.find_first_of(":=", namepos+namelen);
        if (std::string::npos == valpos) break;

        for (++valpos ;valpos < len && ' ' == pchstr[valpos]; ++valpos) // skip space
        {
        }

        if (valpos >= len) break;

        // 2016-06-14 ���Ӵ���˫���ŵĹ���
        size_t valend;
        if ('\"' == pchstr[valpos])
        {
            bool found = false;
            ++valpos;
            valend = valpos;
            for (; valend < len; ++valend)
            {
                if ('\"' == pchstr[valend])
                {
                    found = true;
                    break;
                }
                
                if ('\\' == pchstr[valend])
                {
                    ++valend;
                }
            }

            if (!found)
            {
                --valpos;
                valend = std::string::npos;
            }
        }
        else
        {
            valend = tempstr.find_first_of(" ,}\r\n", valpos);
        }

        // ��ȡ���ֵ
        if (std::string::npos == valend)
        {
            ostr = tempstr.substr(valpos);
        }
        else
        {
            ostr = tempstr.substr(valpos, valend - valpos);
        }

        ret = 0;
    }
    while (0);

    return ret;
}

void StrParse::AdjustPath( std::string& path, bool useend, char dv /*= '/'*/ )
{
	if (!path.empty())
	{
		size_t len = path.length();
		if (useend)
		{
			if (dv != path.at(len-1))
			{
				path.append(1, dv);
			}
		}
		else
		{
			while (dv == path.at(len-1))
			{
				path.erase(len-1);
				len -= 1;
			}
		}
	}
}

bool StrParse::IsCharacter(const std::string& str, bool inc_digit)
{
    bool ret = true;
    size_t len = str.length();
    const char* pstr = str.c_str();

    for (size_t i = 0; i < len; ++i)
    {
        char ch = pstr[i];
        if (!( (ch >= 'a' && ch <= 'z') ||
             (ch >= 'A' && ch <= 'Z') ||
             (inc_digit && ch >= '0' && ch <= '9') ))
        {
            ret = false;
            break;
        }
    }

    return ret;
}

bool StrParse::IsCharacter(const std::string& str, const std::string& excludeStr, bool inc_digit)
{
    bool ret = true;
    size_t len = str.length();
    size_t lenExclu = excludeStr.length();
    const char* pstr = str.c_str();

    for (size_t i = 0; i < len; ++i)
    {
        char ch = pstr[i];
        if (!( (ch >= 'a' && ch <= 'z') ||
             (ch >= 'A' && ch <= 'Z') ||
             (inc_digit && ch >= '0' && ch <= '9')))
        {
            size_t j = 0;
            for (; j < lenExclu; ++j)
            {
                if( ch == excludeStr[j])
                {
                    break;
                }
            }

            if (0 == lenExclu || j >= lenExclu)
            {
                ret = false;
                break;
            }
        }
    }

    return ret;
}

bool StrParse::IsNumberic(const std::string& str)
{
    bool ret = true;
    size_t len = str.length();
    const char* pstr = str.c_str();

    for (size_t i = 0; i < len; ++i)
    {
        char ch = pstr[i];
        if ( !( ch >= '0' && ch <= '9') )
        {
            ret = false;
            break;
        }
    }

    return ret;
}

int StrParse::AppendFormat(std::string& ostr, const char* fmt, ...)
{
    va_list ap;
    int ret;
    unsigned int n;
    char buff[512] = {0};

    va_start(ap, fmt);
    n = vsnprintf(buff, sizeof(buff)-1, fmt, ap);
    va_end(ap);

    ostr.append(buff);
    ret = (n>=sizeof(buff));

    return ret;
}

std::string StrParse::Format( const char* fmt, ... )
{
	long ret;
    va_list ap;
    char buff[2048] = {0};
	char* buff_alloc = NULL;

    va_start(ap, fmt);
    ret=vsnprintf(buff, sizeof(buff)-1, fmt, ap);
    va_end(ap);

	// ����ʱ������vasprintf
	if (ret >= (int)sizeof(buff))
	{
		va_list ap2;
		va_start(ap2, fmt);
		ret = vasprintf(&buff_alloc, fmt, ap2);
		va_end(ap2);
	}

	return buff_alloc?buff_alloc:buff;
}
