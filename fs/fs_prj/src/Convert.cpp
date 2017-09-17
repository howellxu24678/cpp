#include "Convert.h"
#include "Log.h"

#include "Poco/Format.h"
#include "Poco/DateTime.h"



Convert::Convert(const std::string ssCfgPath)
	: m_ssCfgPath(ssCfgPath)
{

}


Convert::~Convert()
{

}

bool Convert::Init()
{
  AutoPtr<XMLConfiguration> apXmlConf = new XMLConfiguration(m_ssCfgPath);

	if (!InitMonthMap(apXmlConf))
	{
		LOG(ERROR_LOG_LEVEL, "Failed to InitMonthMap");
		return false;
	}

	if(!InitDict(apXmlConf))
	{
		LOG(ERROR_LOG_LEVEL, "Failed to InitDict");
		return false;
	}

	if(!InitSymbol(apXmlConf))
	{
		LOG(ERROR_LOG_LEVEL, "Failed to InitSymbol");
		return false;
	}

	return true;
}

char Convert::CvtDict(const int iField, const char cValue, const EnWay enWay)
{
	std::string ssKey = GetDictKey(format("%d", iField), format("%c", cValue), enWay);
	std::map<std::string, std::string>::const_iterator citFind = m_mapDict.find(ssKey);
	if (citFind != m_mapDict.end())
	{
		return citFind->second[0];
	}
	else
	{
		LOG(ERROR_LOG_LEVEL, "Can not find key:%s in dict", ssKey.c_str());
		return '*';
	}
}

bool Convert::AddDict(const std::string &ssField, const std::string &ssIn, const std::string &ssOut, EnWay enWay)
{
	std::string ssKey = GetDictKey(ssField, enWay == Normal ? ssIn : ssOut, enWay);
	std::string ssValue = enWay == Normal ? ssOut : ssIn;

	LOG(DEBUG_LOG_LEVEL, "%s key:%s,value:%s", __FUNCTION__, ssKey.c_str(), ssValue.c_str());

	std::pair<std::map<std::string, std::string>::iterator, bool> ret = 
		m_mapDict.insert(std::pair<std::string, std::string>(ssKey, ssValue));
	
	if (!ret.second)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to insert dict field:%s,in:%s,out:%s,way:%d, may be repeated", 
			ssField.c_str(), ssIn.c_str(), ssOut.c_str(), enWay);
	}
	return ret.second;
}

std::string Convert::GetDictKey(const std::string &ssField, const std::string &ssFrom, EnWay enWay) const
{
	std::string ssPrefix = enWay == Normal ? "N." : "R.";
	return ssPrefix + ssField + "." + ssFrom;
}

bool Convert::InitDict(AutoPtr<XMLConfiguration> apXmlConf)
{
	AbstractConfiguration::Keys kDict, kItem;

	std::string ssDicts = "dicts", ssDictKey = "", ssItemKey = "", ssField = "", ssIn = "", ssOut = "";

	apXmlConf->keys(ssDicts, kDict);
	LOG(DEBUG_LOG_LEVEL, "%s size:%d", ssDicts.c_str(), kDict.size());

	for (AbstractConfiguration::Keys::iterator itDict = kDict.begin(); itDict != kDict.end(); itDict++)
	{
		ssDictKey = ssDicts + "." + *itDict;
		ssField = apXmlConf->getString(ssDictKey + "[@number]");

		apXmlConf->keys(ssDictKey, kItem);
		for (AbstractConfiguration::Keys::iterator itItem = kItem.begin(); itItem != kItem.end(); itItem++)
		{
			ssItemKey = ssDictKey + "." + *itItem;

			ssIn = apXmlConf->getString(ssItemKey + "[@in]");
			ssOut = apXmlConf->getString(ssItemKey + "[@out]");

			if (apXmlConf->hasOption(ssItemKey + "[@way]"))
			{
				if(!AddDict(ssField, ssIn, ssOut, apXmlConf->getInt(ssItemKey + "[@way]") > 0 ? Normal : Reverse)) return false;
			}
			else
			{
				if(!AddDict(ssField, ssIn, ssOut, Normal)) return false;
				if(!AddDict(ssField, ssIn, ssOut, Reverse)) return false;
			}
		}
	}

	return true;
}

bool Convert::InitSymbol(AutoPtr<XMLConfiguration> apXmlConf)
{
	AbstractConfiguration::Keys kSymbol, kItem;

	std::string ssSymbols = "symbols", ssSymbolKey = "", ssItemKey = "", ssName = "", ssIn = "", ssOut = "";

	apXmlConf->keys(ssSymbols, kSymbol);
	LOG(DEBUG_LOG_LEVEL, "%s size:%d", ssSymbols.c_str(), kSymbol.size());

	for (AbstractConfiguration::Keys::iterator itSymbol = kSymbol.begin(); itSymbol != kSymbol.end(); itSymbol++)
	{
		ssSymbolKey = ssSymbols + "." + *itSymbol;
		ssName = apXmlConf->getString(ssSymbolKey + "[@name]");

		apXmlConf->keys(ssSymbolKey, kItem);
		for (AbstractConfiguration::Keys::iterator itItem = kItem.begin(); itItem != kItem.end(); itItem++)
		{
			ssItemKey = ssSymbolKey + "." + *itItem;

			STUSymbol stuSymbol;
			stuSymbol.m_ssName = ssName;
			stuSymbol.m_enSymbolType = (EnSymbolType)apXmlConf->getUInt(ssItemKey + "[@type]");
			stuSymbol.m_ssFormat = apXmlConf->getString(ssItemKey + "[@format]");
			stuSymbol.m_spRe = new RegularExpression(apXmlConf->getString(ssItemKey + "[@re]"));
			stuSymbol.m_iYearPos = apXmlConf->getUInt(ssItemKey + "[@yearpos]");
			stuSymbol.m_iYearLen = apXmlConf->getUInt(ssItemKey + "[@yearlen]");
			stuSymbol.m_iMonthPos = apXmlConf->getUInt(ssItemKey + "[@monthpos]");
			stuSymbol.m_iMonthLen = apXmlConf->getUInt(ssItemKey + "[@monthlen]");

			if(!AddSymbol(GetSymbolKey(stuSymbol.m_ssName, stuSymbol.m_enSymbolType), stuSymbol)) return false;
		}
	}
	return true;
}

bool Convert::AddSymbol(const std::string &ssKey, const STUSymbol &stuSymbol)
{
	LOG(DEBUG_LOG_LEVEL, "%s ssKey:%s,format:%s", __FUNCTION__, ssKey.c_str(), stuSymbol.m_ssFormat.c_str());

	std::pair<std::map<std::string, STUSymbol>::iterator, bool> ret = 
		m_mapSymbol.insert(std::pair<std::string, STUSymbol>(ssKey, stuSymbol));

	if (!ret.second)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to insert symbol, key:%s may be repeated", ssKey.c_str());
	}

	return ret.second;
}

std::string Convert::GetStrType(EnSymbolType enSymbolType) const
{
	switch(enSymbolType)
	{
	case Original:
		return "org";
	case Bloomberg:
		return "blg";
	default:
		return "unknown";
	}
}

std::string Convert::CvtSymbol(const std::string &ssSymbol, EnSymbolType enDstType)
{
	for (std::map<std::string, STUSymbol>::const_iterator cit = m_mapSymbol.begin(); cit != m_mapSymbol.end(); cit++)
	{
		if (cit->second.m_spRe->match(ssSymbol))
		{
			const STUSymbol &stuSymbol = cit->second;

			if(stuSymbol.m_enSymbolType == enDstType) return ssSymbol;

			std::map<std::string, STUSymbol>::const_iterator citFind = m_mapSymbol.find(GetSymbolKey(stuSymbol.m_ssName, enDstType));
			if (citFind != m_mapSymbol.end())
			{
				return CvtSymbol(ssSymbol, stuSymbol, citFind->second);
			}
		}
	}

	return "unknown" + ssSymbol;
}

std::string Convert::GetSymbolKey(const std::string &ssName, EnSymbolType enSymbolType) const
{
	return ssName + "." + GetStrType(enSymbolType);
}

std::string Convert::CvtSymbol(const std::string &ssSrcSymbol, const STUSymbol &stuSrcSymbol, const STUSymbol &stuDstSymbol) const
{
	std::string ssDstSymbol = stuDstSymbol.m_ssFormat;

	ssDstSymbol.replace(stuDstSymbol.m_iYearPos, stuDstSymbol.m_iYearLen, CvtYear(ssSrcSymbol, stuSrcSymbol, stuDstSymbol));
	ssDstSymbol.replace(stuDstSymbol.m_iMonthPos, stuDstSymbol.m_iMonthLen, CvtMonth(ssSrcSymbol, stuSrcSymbol, stuDstSymbol));

	return ssDstSymbol;
}

std::string Convert::CvtYear(const std::string &ssSrcSymbol, const STUSymbol &stuSrcSymbol, const STUSymbol &stuDstSymbol) const
{
	std::string ssSrcYear = ssSrcSymbol.substr(stuSrcSymbol.m_iYearPos, stuSrcSymbol.m_iYearLen);
	//长度相同时用原始代码的年份替代目的代码的年份
	if (stuSrcSymbol.m_iYearLen == stuDstSymbol.m_iYearLen) return ssSrcYear;

	if (stuSrcSymbol.m_iYearLen > stuDstSymbol.m_iYearLen && stuSrcSymbol.m_iYearLen == 2) return format("%c", ssSrcYear[1]);

	if (stuSrcSymbol.m_iYearLen < stuDstSymbol.m_iYearLen && stuSrcSymbol.m_iYearLen == 1) return CvtYearDigitFrom1To2(ssSrcYear);

	return "**";
}

std::string Convert::CvtMonth(const std::string &ssSrcSymbol, const STUSymbol &stuSrcSymbol, const STUSymbol &stuDstSymbol) const
{
	std::string ssSrcMonth = ssSrcSymbol.substr(stuSrcSymbol.m_iMonthPos, stuSrcSymbol.m_iMonthLen);
	
	//长度相同时用原始代码的月份替代目的代码的月份
	if (stuSrcSymbol.m_iMonthLen == stuDstSymbol.m_iMonthLen) return ssSrcMonth;

	return CvtMonth(ssSrcMonth);
}

std::string Convert::CvtMonth(const std::string &ssSrcMonth) const
{
	std::map<std::string, std::string>::const_iterator citFind = m_mapMonth.find(ssSrcMonth);
	if (citFind != m_mapMonth.end())
	{
		return citFind->second;
	}

	return "unknown";
}

bool Convert::InitMonthMap(AutoPtr<XMLConfiguration> apXmlConf)
{
	AbstractConfiguration::Keys kItem;

	std::string ssMonth = "month", ssItemKey = "", ssIn = "", ssOut = "";

	apXmlConf->keys(ssMonth, kItem);
	LOG(DEBUG_LOG_LEVEL, "%s size:%d", ssMonth.c_str(), kItem.size());

	for (AbstractConfiguration::Keys::iterator it = kItem.begin(); it != kItem.end(); it++)
	{
		ssItemKey = ssMonth + "." + *it;
		ssIn = apXmlConf->getString(ssItemKey + "[@in]");
		ssOut = apXmlConf->getString(ssItemKey + "[@out]");

		if (!AddMonth(ssIn, ssOut)) return false;
		if (!AddMonth(ssOut, ssIn)) return false;
	}

	return true;
}

bool Convert::AddMonth(const std::string &ssKey, const std::string &ssValue)
{
	std::pair<std::map<std::string, std::string>::iterator, bool> ret = 
		m_mapMonth.insert(std::pair<std::string, std::string>(ssKey, ssValue));

	if (!ret.second)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to insert month key:%s value:%s, may be repeated", ssKey.c_str(), ssValue.c_str());
	}

	return ret.second;
}

std::string Convert::CvtYearDigitFrom1To2(const std::string &ssSrcYear) const
{
	DateTime now;
	int iYear = now.year() % 100;
	int iSrcYear = ssSrcYear[0] - '0';

	//有合约是跨年的，所以这里要加到最后一位与原始的一致才正确
	while(iYear % 10 != iSrcYear) 
		iYear++;

	return format("%d", iYear);
}

