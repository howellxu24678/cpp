#ifndef __CONVERT_H__
#define __CONVERT_H__

#include <string>
#include <map>

#include "Poco/RegularExpression.h"
#include "Poco/SharedPtr.h"
#include "Poco/Util/XMLConfiguration.h"
#include "quickfix/Values.h"

using namespace Poco;
using namespace Poco::Util;

class Convert
{
public:
  //字典转换方向：Sgit，Fix
  enum EnDictType { Sgit, Fix };
  //代码类别：路透代码，交易所原始代码，彭博代码，取值对应fix tag22 对应的字典取值
	enum EnCvtType { Unknow = 0, Reuters = 5, Original = 8, Bloomberg = 100};

	struct STUSymbol
	{
		std::string				            m_ssName;
		EnCvtType			                m_enSymbolType;
		std::string				            m_ssFormat;
		SharedPtr<RegularExpression>	m_spRe;
		UINT8							            m_iYearPos;
		UINT8							            m_iYearLen;
		UINT8							            m_iMonthPos;
		UINT8							            m_iMonthLen;
	};

  struct STUExchange
  {
    std::string       m_ssName;
    EnCvtType         m_enCvtType;
    std::string       m_ssExchange;
  };

	Convert(const std::string ssCfgPath);
	~Convert();

	bool Init();

  char CvtDict(const int iField, const char cValue, const EnDictType enDstDictType);

	std::string CvtSymbol(const std::string &ssSymbol, const EnCvtType enDstType);

	std::string CvtExchange(const std::string &ssExchange, const EnCvtType enDstType);

protected:
	bool InitMonthMap(AutoPtr<XMLConfiguration> apXmlConf);

	std::string CvtMonth(const std::string &ssSrcMonth) const;

	bool AddMonth(const std::string &ssKey, const std::string &ssValue);

	bool InitDict(AutoPtr<XMLConfiguration> apXmlConf);

	bool InitSymbol(AutoPtr<XMLConfiguration> apXmlConf);

	bool InitExchange(AutoPtr<XMLConfiguration> apXmlConf);

	bool AddDict(const std::string &ssField, const std::string &ssFix, const std::string &ssSgit, EnDictType enDstDictType);

	bool AddSymbol(const std::string &ssKey, const STUSymbol &stuSymbol);

	bool AddExchange(const std::string &ssKey, const STUExchange &stuExchange);

	std::string CvtSymbol(const std::string &ssSrcSymbol, const STUSymbol &stuSrcSymbol, const STUSymbol &stuDstSymbol) const;

	std::string CvtYear(const std::string &ssSrcSymbol, const STUSymbol &stuSrcSymbol, const STUSymbol &stuDstSymbol) const;

	std::string CvtMonth(const std::string &ssSrcSymbol, const STUSymbol &stuSrcSymbol, const STUSymbol &stuDstSymbol) const;

  static std::string GetDictKey(const std::string &ssField, const std::string &ssFrom, EnDictType enDstDictType);

  static std::string GetStrType(EnCvtType enSymbolType);

  static std::string GetSymbolKey(const std::string &ssName, EnCvtType enCvtType);

	static std::string CvtYearDigitFrom1To2(const std::string &ssSrcYear);

	static std::string GetExchangeKey(const std::string &ssName, EnCvtType enCvtType);
private:
	std::string														m_ssCfgPath;
	std::map<std::string, std::string>		m_mapDict;
	std::map<std::string, STUSymbol>			m_mapSymbol;
	std::map<std::string, std::string>		m_mapMonth;
	std::map<std::string, STUExchange>		m_mapExchange;
};

#endif // __CONVERT_H__
