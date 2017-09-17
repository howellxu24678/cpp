#ifndef __CONVERT_H__
#define __CONVERT_H__

#include <string>
#include <map>

#include "Poco/RegularExpression.h"
#include "Poco/SharedPtr.h"
#include "Poco/Util/XMLConfiguration.h"

using namespace Poco;
using namespace Poco::Util;

class Convert
{
public:
  enum EnWay { Normal, Reverse };
	enum EnSymbolType { Original, Bloomberg};

	struct STUSymbol
	{
		std::string				m_ssName;
		EnSymbolType			m_enSymbolType;
		std::string				m_ssFormat;
		SharedPtr<RegularExpression>	m_spRe;
		UINT8							m_iYearPos;
		UINT8							m_iYearLen;
		UINT8							m_iMonthPos;
		UINT8							m_iMonthLen;
	};

	Convert(const std::string ssCfgPath);
	~Convert();

	bool Init();

  char CvtDict(const int iField, const char cValue, const EnWay enWay);

	std::string CvtSymbol(const std::string &ssSymbol, EnSymbolType enDstType);

protected:
	bool InitMonthMap(AutoPtr<XMLConfiguration> apXmlConf);

	std::string CvtMonth(const std::string &ssSrcMonth) const;

	bool AddMonth(const std::string &ssKey, const std::string &ssValue);

	bool InitDict(AutoPtr<XMLConfiguration> apXmlConf);

	bool InitSymbol(AutoPtr<XMLConfiguration> apXmlConf);

	bool AddDict(const std::string &ssField, const std::string &ssIn, const std::string &ssOut, EnWay enWay);

	std::string GetDictKey(const std::string &ssField, const std::string &ssFrom, EnWay enWay) const;

	bool AddSymbol(const std::string &ssKey, const STUSymbol &stuSymbol);

	std::string GetStrType(EnSymbolType enSymbolType) const;

	std::string GetSymbolKey(const std::string &ssName, EnSymbolType enSymbolType) const;

	std::string CvtSymbol(const std::string &ssSrcSymbol, const STUSymbol &stuSrcSymbol, const STUSymbol &stuDstSymbol) const;

	std::string CvtYear(const std::string &ssSrcSymbol, const STUSymbol &stuSrcSymbol, const STUSymbol &stuDstSymbol) const;

	std::string CvtMonth(const std::string &ssSrcSymbol, const STUSymbol &stuSrcSymbol, const STUSymbol &stuDstSymbol) const;

	std::string CvtYearDigitFrom1To2(const std::string &ssSrcYear) const;

private:
	std::string														m_ssCfgPath;
	std::map<std::string, std::string>		m_mapDict;
	std::map<std::string, STUSymbol>			m_mapSymbol;
	std::map<std::string, std::string>		m_mapMonth;
};

#endif // __CONVERT_H__
