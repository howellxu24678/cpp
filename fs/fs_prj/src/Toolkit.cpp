#include "Toolkit.h"
#include <cctype>
#include "Poco/File.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"

bool CToolkit::isAliasAcct(const std::string &ssAcct)
{
  for (std::string::const_iterator cit = ssAcct.begin(); cit != ssAcct.end(); cit++)
    if(std::isalpha(*cit)) return true;

  return false;
}

bool CToolkit::getStrinIfSet(Poco::AutoPtr<Poco::Util::IniFileConfiguration> apCfg, const std::string &ssProperty, std::string &ssValue)
{
	ssValue = "";
	if (apCfg->hasProperty(ssProperty))
	{
		ssValue = apCfg->getString(ssProperty);
		return true;
	}

	return false;
}

std::string CToolkit::GenAcctAliasKey(const FIX::SessionID &oSessionID, const std::string &ssOnBehalfOfCompID, const std::string &ssTradeID)
{
  return oSessionID.toString() + "|" + ssOnBehalfOfCompID + "|" + ssTradeID;
}

std::string CToolkit::GenAcctAliasKey(const std::string &ssAccount, const FIX::Message& oRecvMsg)
{
	FIX::OnBehalfOfCompID onBehalfOfCompId;
	std::string ssOnBehalfOfCompID = oRecvMsg.getHeader().getFieldIfSet(onBehalfOfCompId) ? onBehalfOfCompId.getValue() : "";

  FIX::BeginString beginString;
  FIX::SenderCompID senderCompID;
  FIX::TargetCompID targetCompID;

  oRecvMsg.getHeader().getField(beginString);
  oRecvMsg.getHeader().getField(senderCompID);
  oRecvMsg.getHeader().getField(targetCompID);

	return CToolkit::GenAcctAliasKey(FIX::SessionID(beginString, targetCompID, senderCompID), ssOnBehalfOfCompID, ssAccount);
}

bool CToolkit::isExist(const std::string &ssFilePath)
{
	Poco::File file = Poco::File(ssFilePath);
	return file.exists();
}

std::string CToolkit::GetUuid()
{
  Poco::LocalDateTime now;
  return Poco::DateTimeFormatter::format(now, Poco::DateTimeFormat::ISO8601_FRAC_FORMAT);
}

