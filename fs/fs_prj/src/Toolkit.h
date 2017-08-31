#ifndef __TOOLKIT_H__
#define __TOOLKIT_H__

#include <string>
#include "Poco/Util/IniFileConfiguration.h"

class CToolkit
{
public:
  static bool isAliasAcct(const std::string &ssAcct);
	static bool getStrinIfSet(Poco::AutoPtr<Poco::Util::IniFileConfiguration> apCfg, const std::string &ssProperty, std::string &ssValue);
};
#endif // __TOOLKIT_H__
