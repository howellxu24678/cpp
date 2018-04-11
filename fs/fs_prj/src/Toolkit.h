#ifndef __TOOLKIT_H__
#define __TOOLKIT_H__

#include <string>
#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/String.h"
#include "quickfix/Message.h"
#include "Convert.h"
#include "Const.h"

class CToolkit
{
public:
	//含有字母为别名账户
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

  static std::string GetNowTime();

  static std::string GetSessionKey(const FIX::Message& oRecvMsg);

  //SessionKey 转为SessionID和onBehalfCompID
  static void SessionKey2SessionIDBehalfCompID(const std::string &ssSessionKey, FIX::SessionID &oSessionID, std::string &ssOnBehalfCompID);

	//交易业务相关请求
  static bool IsTdRequest(const FIX::MsgType &msgType);

	//行情业务相关请求
  static bool IsMdRequest(const FIX::MsgType &msgType);

  //配置段如果出现'.'，解析会有起义，将FIX.4.2 转为 FIX#4#2
  static std::string SessionID2Prop(const std::string &ssSessionKey);

  //FIX#4#2 转为 FIX.4.2
  static std::string SessionProp2ID(const std::string &ssSessionKey);

	static bool CheckIfValid(Convert::EnCvtType enSymbolType, std::string &ssErrMsg);

	static void SetUserInfo(const STUserInfo &stuUserInfo, FIX::Message &oMsg);

  static bool Send(const FIX::Message &oRecvMsg, FIX::Message &oSendMsg);

  static bool Send(FIX::Message &oSendMsg, const FIX::SessionID &oSendSessionID, const std::string &ssOnBehalfOfCompID);

  static bool GetString(AutoPtr<IniFileConfiguration> apConfig, const std::string &ssProp, std::string &ssValue);
};
#endif // __TOOLKIT_H__
