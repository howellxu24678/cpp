#ifndef __TOOLKIT_H__
#define __TOOLKIT_H__

#include <string>
#include "Poco/Util/IniFileConfiguration.h"
#include "quickfix/Message.h"

class CToolkit
{
public:
  static bool isAliasAcct(const std::string &ssAcct);

	static bool getStrinIfSet(
    Poco::AutoPtr<Poco::Util::IniFileConfiguration> apCfg, 
    const std::string &ssProperty, 
    std::string &ssValue);
  
  static std::string GenAcctAliasKey(
    const FIX::SessionID &oSessionID, 
    const std::string &ssOnBehalfOfCompID, 
    const std::string &ssAccountAlias); 

	static std::string GenAcctAliasKey(const FIX::Message& oRecvMsg, const std::string &ssAccount);

	static bool isExist(const std::string &ssFilePath);

  static std::string GetUuid();

  static std::string GetSessionKey(const FIX::Message& oRecvMsg);
};
#endif // __TOOLKIT_H__
