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
  //�ֵ�ת������Sgit��Fix
  enum EnDictType { Sgit, Fix };
  //������𣺽�����ԭʼ���룬������
	enum EnCvtType { Original, Bloomberg};

	struct STUSymbol
	{
		std::string				m_ssName;
		EnCvtType			m_enSymbolType;
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

  char CvtDict(const int iField, const char cValue, const EnDictType enDstDictType);

	std::string CvtSymbol(const std::string &ssSymbol, const EnCvtType enDstType);

	std::string CvtExchange(const std::string &ssExchange, const EnCvtType enSrcType, const EnCvtType enDstType);

protected:
	bool InitMonthMap(AutoPtr<XMLConfiguration> apXmlConf);

	std::string CvtMonth(const std::string &ssSrcMonth) const;

	bool AddMonth(const std::string &ssKey, const std::string &ssValue);

	bool InitDict(AutoPtr<XMLConfiguration> apXmlConf);

	bool InitSymbol(AutoPtr<XMLConfiguration> apXmlConf);

	bool InitExchange(AutoPtr<XMLConfiguration> apXmlConf);

	bool AddDict(const std::string &ssField, const std::string &ssFix, const std::string &ssSgit, EnDictType enDstDictType);

	bool AddSymbol(const std::string &ssKey, const STUSymbol &stuSymbol);

	bool AddExchange(const std::string &ssKey, const std::string &ssValue);

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
	std::map<std::string, std::string>		m_mapExchange;
};

#endif // __CONVERT_H__
