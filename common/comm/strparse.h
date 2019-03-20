/******************************************************************* 
 *  summery:        字符串解析处理类
 *  author:         hejl
 *  date:           2016-05-17
 *  description:    
 ******************************************************************/

#ifndef _STRPARSE_H_
#define _STRPARSE_H_
#include <vector>
#include <map>
#include <string>

class StrParse
{
public:
    // 以dv作为分隔符拆分str串返得到的结果存放到data容器
    static int SpliteInt(std::vector<int>& data, const std::string& str, char dv, int nildef = 0);
    static int SpliteStr(std::vector<std::string>& data, const std::string& str, char dv);
    static int SpliteStr(std::vector<std::string>& data, const char* pstr, unsigned int len, char dv);

    // 解析http的url参数
    static int SpliteQueryString(std::map<std::string, std::string>& outPar, const std::string& qstr);
    // 解析url各部分, proto://host:port/path?qparam
    static int SplitURL(std::string& proto, std::string& host, int& port, std::string& path, std::string& qparam, const std::string& url);
    // 简单json解析, src全部转化成小写处理;
    static int PickOneJson(std::string& ostr, const std::string& src, const std::string& name);
    // 加入json中一项数据
    static bool PutOneJson(std::string& jstr, const std::string& jkey, const std::string& jvalue, bool comma_end = false);
    static bool PutOneJson(std::string& jstr, const std::string& jkey, int jval, bool comma_end = false);

    // 处理路径尾部字符
	static void AdjustPath(std::string& path, bool useend, char dv = '/');

    // 判断字符串是否不含特殊字符
    static bool IsCharacter(const std::string& str, bool inc_digit);
    static bool IsCharacter(const std::string& str, const std::string& excludeStr, bool inc_digit);
    static bool IsNumberic(const std::string& str);

    // 字符串格式化,类似sprintf作用
    static int AppendFormat(std::string& ostr, const char* fmt, ...);
    static std::string Format(const char* fmt, ...);
    static std::string Itoa(int n);
};

#define _F StrParse::Format
#define _N StrParse::Itoa

#endif
