#include "Convert.h"
#include "Log.h"
#include "Poco/Util/XMLConfiguration.h"
using namespace Poco;
using namespace Poco::Util;

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
  AbstractConfiguration::Keys kDict, kInner;
  
  std::string ssDicts = "dicts", ssDictKey = "", ssNum = "";
  
  apXmlConf->keys(ssDicts, kDict);
  LOG(DEBUG_LOG_LEVEL, "dicts size:%d", kDict.size());

  for (AbstractConfiguration::Keys::iterator itDict = kDict.begin(); itDict != kDict.end(); itDict++)
  {
    ssDictKey = ssDicts + "." + *itDict;
    ssNum = apXmlConf->getString(ssDictKey + "[@number]");
    apXmlConf->keys(ssDictKey, kInner);
    for (AbstractConfiguration::Keys::iterator itInner = kInner.begin(); itInner != kInner.end(); itInner++)
    {
      //apXmlConf->getString(ssDictKey + )
    }
    LOG(DEBUG_LOG_LEVEL, "xml it:%s", itDict->c_str());
  }

	return true;
}

char Convert::GetCvt(const int iField,const char cValue)
{
  return ' ';
}
