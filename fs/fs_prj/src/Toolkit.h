#ifndef __TOOLKIT_H__
#define __TOOLKIT_H__

#include <string>
#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/String.h"
#include "quickfix/Message.h"



class CToolkit
{
public:
  static bool IsAliasAcct(const std::string &ssAcct);

	static bool GetStrinIfSet(
    Poco::AutoPtr<Poco::Util::IniFileConfiguration> apCfg, 
    const std::string &ssProperty, 
    std::string &ssValue);
  
  static std::string GenAcctAliasKey(
    const FIX::SessionID &oSessionID, 
    const std::string &ssOnBehalfOfCompID, 
    const std::string &ssAccountAlias); 

	static std::string GenAcctAliasKey(const FIX::Message& oRecvMsg, const std::string &ssAccount);

	static bool IsExist(const std::string &ssFilePath);

  static std::string GetUuid();

  static std::string GetSessionKey(const FIX::Message& oRecvMsg);

  static bool IsTdRequest(const FIX::MsgType &msgType);

  static bool IsMdRequest(const FIX::MsgType &msgType);

  //配置段如果出现'.'，解析会有起义，将FIX.4.2 转为 FIX#4#2
  static std::string SessionID2Prop(const std::string &ssSessionKey);

  //FIX#4#2 转为 FIX.4.2
  static std::string SessionProp2ID(const std::string &ssSessionKey);
};
#endif // __TOOLKIT_H__
