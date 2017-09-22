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
    const std::string &ssTradeID); 

	static std::string GenAcctAliasKey(const std::string &ssAccount, const FIX::Message& oRecvMsg);

	static bool isExist(const std::string &ssFilePath);

  static std::string GetUuid();
};
#endif // __TOOLKIT_H__
