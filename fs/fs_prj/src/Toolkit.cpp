#include "Toolkit.h"
#include <cctype>
#include "Poco/File.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"

bool CToolkit::IsAliasAcct(const std::string &ssAcct)
{
  for (std::string::const_iterator cit = ssAcct.begin(); cit != ssAcct.end(); cit++)
    if(std::isalpha(*cit)) return true;

  return false;
}

bool CToolkit::GetStrinIfSet(Poco::AutoPtr<Poco::Util::IniFileConfiguration> apCfg, const std::string &ssProperty, std::string &ssValue)
{
	ssValue = "";
	if (apCfg->hasProperty(ssProperty))
	{
		ssValue = apCfg->getString(ssProperty);
		return true;
	}

	return false;
}

std::string CToolkit::GenAcctAliasKey(const FIX::SessionID &oSessionID, const std::string &ssOnBehalfOfCompID, const std::string &ssAccountAlias)
{
  return oSessionID.toString() + "|" + ssOnBehalfOfCompID + "|" + ssAccountAlias;
}

std::string CToolkit::GenAcctAliasKey(const FIX::Message& oRecvMsg, const std::string &ssAccount)
{
	FIX::OnBehalfOfCompID onBehalfOfCompId;
	//std::string ssOnBehalfOfCompID = oRecvMsg.getHeader().getFieldIfSet(onBehalfOfCompId) ? onBehalfOfCompId.getValue() : "";
  oRecvMsg.getHeader().getFieldIfSet(onBehalfOfCompId);

	return CToolkit::GenAcctAliasKey(oRecvMsg.getSessionID(), onBehalfOfCompId.getValue(), ssAccount);
}

bool CToolkit::IsExist(const std::string &ssFilePath)
{
	Poco::File file = Poco::File(ssFilePath);
	return file.exists();
}

std::string CToolkit::GetUuid()
{
  Poco::LocalDateTime now;
  return Poco::DateTimeFormatter::format(now, "%Y%m%d%H%M%S%i");
}

std::string CToolkit::GetSessionKey(const FIX::Message& oRecvMsg)
{
  FIX::BeginString beginString;
  FIX::SenderCompID senderCompID;
  FIX::TargetCompID targetCompID;
  FIX::OnBehalfOfCompID onBehalfOfCompId;
  oRecvMsg.getHeader().getField(beginString);
  oRecvMsg.getHeader().getField(senderCompID);
  oRecvMsg.getHeader().getField(targetCompID);
  oRecvMsg.getHeader().getFieldIfSet(onBehalfOfCompId);

  FIX::SessionID oSessionID(beginString.getValue(), targetCompID.getValue(), senderCompID.getValue());
  return oSessionID.toString() + "|" + onBehalfOfCompId.getValue();
}

bool CToolkit::IsTdRequest(const FIX::MsgType &msgType)
{
  return msgType == FIX::MsgType_NewOrderSingle 
    || msgType == FIX::MsgType_OrderCancelRequest 
    || msgType == FIX::MsgType_OrderStatusRequest ? true : false;
}

bool CToolkit::IsMdRequest(const FIX::MsgType &msgType)
{
  return msgType == FIX::MsgType_MarketDataRequest ? true : false;
}

std::string CToolkit::SessionID2Prop(const std::string &ssSessionKey)
{
  return Poco::replace(ssSessionKey, "FIX.4.2", "FIX#4#2");
}

std::string CToolkit::SessionProp2ID(const std::string &ssSessionKey)
{
  return Poco::replace(ssSessionKey, "FIX#4#2", "FIX.4.2");
}

bool CToolkit::CheckIfValid(Convert::EnCvtType enSymbolType, std::string &ssErrMsg)
{
	if (enSymbolType == Convert::Reuters || enSymbolType == Convert::Original || enSymbolType == Convert::Bloomberg) return true;

	ssErrMsg = "unsupported symbol type";
	return false;
}

