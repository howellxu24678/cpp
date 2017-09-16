#include "Convert.h"
#include "Log.h"

#include "Poco/Format.h"



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

char Convert::GetCvtDict(const int iField, const char cValue, const EnWay enWay)
{
	std::string ssKey = GetDictKey(format("%d", iField), format("%c", cValue), enWay);
	std::map<std::string, std::string>::iterator itFind = m_mapDict.find(ssKey);
	if (itFind != m_mapDict.end())
	{
		return itFind->second[0];
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
	LOG(DEBUG_LOG_LEVEL, "symbols size:%d", kSymbol.size());

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
			stuSymbol.m_spRe = new RegularExpression(stuSymbol.m_ssFormat);
			stuSymbol.m_iYearPos = apXmlConf->getUInt(ssItemKey + "[@yearpos]");
			stuSymbol.m_iYearLen = apXmlConf->getUInt(ssItemKey + "[@yearlen]");
			stuSymbol.m_iMonthPos = apXmlConf->getUInt(ssItemKey + "[@monthpos]");
			stuSymbol.m_iMonthLen = apXmlConf->getUInt(ssItemKey + "[@monthlen]");

			if(!AddSymbol(ssName + "." + GetStrType(stuSymbol.m_enSymbolType), stuSymbol)) return false;
		}
	}
	return true;
}

bool Convert::AddSymbol(const std::string ssKey, const STUSymbol &stuSymbol)
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
		return "unknow";
	}
}

std::string Convert::GetCvtSymbol(const std::string &ssSymbol, EnSymbolType enTargetType)
{
	for (std::map<std::string, STUSymbol>::iterator it = m_mapSymbol.begin(); it != m_mapSymbol.end(); it++)
	{
		if (it->second.m_spRe->match(ssSymbol))
		{
			return "found " + it->second.m_ssFormat;
		}
	}

	return "unknow" + ssSymbol;
}
