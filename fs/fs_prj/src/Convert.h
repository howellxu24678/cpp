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

  char GetCvtDict(const int iField, const char cValue, const EnWay enWay);

	std::string GetCvtSymbol(const std::string &ssSymbol, EnSymbolType enTargetType);

protected:
	bool InitDict(AutoPtr<XMLConfiguration> apXmlConf);

	bool InitSymbol(AutoPtr<XMLConfiguration> apXmlConf);

	bool AddDict(const std::string &ssField, const std::string &ssIn, const std::string &ssOut, EnWay enWay);

	std::string GetDictKey(const std::string &ssField, const std::string &ssFrom, EnWay enWay) const;

	bool AddSymbol(const std::string ssKey, const STUSymbol &stuSymbol);

	std::string GetStrType(EnSymbolType enSymbolType) const;

private:
	std::string														m_ssCfgPath;
	std::map<std::string, std::string>		m_mapDict;
	std::map<std::string, STUSymbol>			m_mapSymbol;
};

#endif // __CONVERT_H__
