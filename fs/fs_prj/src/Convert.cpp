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

	if (!InitExchange(apXmlConf))
	{
		LOG(ERROR_LOG_LEVEL, "Failed to InitExchange");
		return false;
	}

	return true;
}

char Convert::CvtDict(const int iField, const char cFrom, const EnDictType enDstDictType)
{
	return CvtDict(iField, Poco::format("%c", cFrom), enDstDictType)[0];
}

std::string Convert::CvtDict(const int iField, const std::string &ssFrom, const EnDictType enDstDictType)
{
	std::string ssKey = GetDictKey(Poco::format("%d", iField), ssFrom, enDstDictType);
	std::map<std::string, std::string>::const_iterator citFind = m_mapDict.find(ssKey);
	if (citFind != m_mapDict.end())
	{
		return citFind->second;
	}
	else
	{
		LOG(ERROR_LOG_LEVEL, "Can not find key:%s in dict", ssKey.c_str());
		return "*";
	}
}

bool Convert::AddDict(const std::string &ssField, const std::string &ssFix, const std::string &ssSgit, EnDictType enDstDictType)
{
	std::string ssKey = GetDictKey(ssField, enDstDictType == Sgit ? ssFix : ssSgit, enDstDictType);
	std::string ssValue = enDstDictType == Sgit ? ssSgit : ssFix;

	//LOG(DEBUG_LOG_LEVEL, "key:%s,value:%s", ssKey.c_str(), ssValue.c_str());

	std::pair<std::map<std::string, std::string>::iterator, bool> ret = 
		m_mapDict.insert(std::pair<std::string, std::string>(ssKey, ssValue));

	if (!ret.second)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to insert dict field:%s,in:%s,out:%s,way:%d, may be repeated", 
			ssField.c_str(), ssFix.c_str(), ssSgit.c_str(), enDstDictType);
	}
	return ret.second;
}

std::string Convert::GetDictKey(const std::string &ssField, const std::string &ssFrom, EnDictType enDstDictType)
{
	std::string ssPrefix = enDstDictType == Sgit ? "F->S." : "S->F.";
	return ssPrefix + ssField + "." + ssFrom;
}

bool Convert::InitDict(AutoPtr<XMLConfiguration> apXmlConf)
{
	AbstractConfiguration::Keys kDict, kItem;

	std::string ssDicts = "dicts", ssDictKey = "", ssItemKey = "", ssField = "", ssFix = "", ssSgit = "";

	apXmlConf->keys(ssDicts, kDict);
	LOG(DEBUG_LOG_LEVEL, "%s size:%d", ssDicts.c_str(), (int)kDict.size());

	for (AbstractConfiguration::Keys::iterator itDict = kDict.begin(); itDict != kDict.end(); itDict++)
	{
		ssDictKey = ssDicts + "." + *itDict;
		ssField = apXmlConf->getString(ssDictKey + "[@number]");

		apXmlConf->keys(ssDictKey, kItem);
		for (AbstractConfiguration::Keys::iterator itItem = kItem.begin(); itItem != kItem.end(); itItem++)
		{
			ssItemKey = ssDictKey + "." + *itItem;

			ssFix = apXmlConf->getString(ssItemKey + "[@fix]");
			ssSgit = apXmlConf->getString(ssItemKey + "[@sgit]");

			if (apXmlConf->hasOption(ssItemKey + "[@way]"))
			{
				if(!AddDict(ssField, ssFix, ssSgit, apXmlConf->getInt(ssItemKey + "[@way]") > 0 ? Sgit : Fix)) return false;
			}
			else
			{
				if(!AddDict(ssField, ssFix, ssSgit, Sgit)) return false;
				if(!AddDict(ssField, ssFix, ssSgit, Fix)) return false;
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
	LOG(DEBUG_LOG_LEVEL, "%s size:%d", ssSymbols.c_str(), (int)kSymbol.size());

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
			stuSymbol.m_enSymbolType = (EnCvtType)apXmlConf->getUInt(ssItemKey + "[@type]");
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
	//LOG(DEBUG_LOG_LEVEL, "ssKey:%s,format:%s", ssKey.c_str(), stuSymbol.m_ssFormat.c_str());

	std::pair<std::map<std::string, STUSymbol>::iterator, bool> ret = 
		m_mapSymbol.insert(std::pair<std::string, STUSymbol>(ssKey, stuSymbol));

	if (!ret.second)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to insert symbol, key:%s may be repeated", ssKey.c_str());
	}

	return ret.second;
}

std::string Convert::GetStrType(EnCvtType enSymbolType)
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

std::string Convert::CvtSymbol(const std::string &ssSymbol, const EnCvtType enDstType)
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

	return ssSymbol;
}

std::string Convert::GetSymbolKey(const std::string &ssName, EnCvtType enCvtType)
{
	return ssName + "." + GetStrType(enCvtType);
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

	if (stuSrcSymbol.m_iYearLen > stuDstSymbol.m_iYearLen && stuSrcSymbol.m_iYearLen == 2) return Poco::format("%c", ssSrcYear[1]);

	if (stuSrcSymbol.m_iYearLen < stuDstSymbol.m_iYearLen && stuSrcSymbol.m_iYearLen == 1) return CvtYearDigitFrom1To2(ssSrcYear);

	return "unknown";
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
	LOG(DEBUG_LOG_LEVEL, "%s size:%d", ssMonth.c_str(), (int)kItem.size());

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

std::string Convert::CvtYearDigitFrom1To2(const std::string &ssSrcYear)
{
	DateTime now;
	int iYear = now.year() % 100;
	int iSrcYear = ssSrcYear[0] - '0';
	if (iSrcYear < 0 || iSrcYear >= 10)
	{
		LOG(ERROR_LOG_LEVEL, "srcYear %s invalid", ssSrcYear.c_str());
		return "unknown";
	}

	//有合约是跨年的，所以这里要加到最后一位与原始的一致才正确
	while(iYear % 10 != iSrcYear) 
		iYear++;

	return Poco::format("%d", iYear);
}

bool Convert::InitExchange(AutoPtr<XMLConfiguration> apXmlConf)
{
	AbstractConfiguration::Keys kExchange, kItem;

	std::string ssExchanges = "exchanges", ssSymbolKey = "", ssItemKey = "", ssName = "", ssValue = "";

	apXmlConf->keys(ssExchanges, kExchange);
	LOG(DEBUG_LOG_LEVEL, "%s size:%d", ssExchanges.c_str(), (int)kExchange.size());

	for (AbstractConfiguration::Keys::iterator itExchange = kExchange.begin(); itExchange != kExchange.end(); itExchange++)
	{
		ssSymbolKey = ssExchanges + "." + *itExchange;
		ssName = apXmlConf->getString(ssSymbolKey + "[@name]");

		apXmlConf->keys(ssSymbolKey, kItem);
		for (AbstractConfiguration::Keys::iterator itItem = kItem.begin(); itItem != kItem.end(); itItem++)
		{
			ssItemKey = ssSymbolKey + "." + *itItem;

			STUExchange stuExchange;
			stuExchange.m_ssName = ssName;
			stuExchange.m_enCvtType = (EnCvtType)apXmlConf->getUInt(ssItemKey + "[@type]");
			stuExchange.m_ssExchange = apXmlConf->getString(ssItemKey + "[@value]");

			if(!AddExchange(GetExchangeKey(stuExchange.m_ssName, stuExchange.m_enCvtType), stuExchange)) return false;
		}
	}
	return true;
}

std::string Convert::CvtExchange(const std::string &ssExchange, const EnCvtType enDstType)
{
	for(std::map<std::string, STUExchange>::const_iterator cit = m_mapExchange.begin(); cit != m_mapExchange.end(); cit++)
	{
		if (cit->second.m_ssExchange == ssExchange)
		{
			const STUExchange &stuExchange = cit->second;
			if (stuExchange.m_enCvtType == enDstType) return ssExchange;

			std::map<std::string, STUExchange>::const_iterator citFind = m_mapExchange.find(GetExchangeKey(stuExchange.m_ssName, enDstType));
			if (citFind != m_mapExchange.end())
			{
				return citFind->second.m_ssExchange;
			}
		}
	}

	return ssExchange;
}

std::string Convert::GetExchangeKey(const std::string &ssName, EnCvtType enCvtType)
{
	return ssName + "." + GetStrType(enCvtType);
}

bool Convert::AddExchange(const std::string &ssKey, const STUExchange &stuExchange)
{
	//LOG(DEBUG_LOG_LEVEL, "ssKey:%s,ssExchange:%s", ssKey.c_str(), stuExchange.m_ssExchange.c_str());

	std::pair<std::map<std::string, STUExchange>::iterator, bool> ret = 
		m_mapExchange.insert(std::pair<std::string, STUExchange>(ssKey, stuExchange));

	if (!ret.second)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to insert exchange, key:%s may be repeated", ssKey.c_str());
	}

	return ret.second;
}

