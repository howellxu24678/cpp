#include "SgitApiManager.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "Log.h"
#include "quickfix/SessionID.h"
#include "Toolkit.h"

using namespace Poco::Util;

CSgitApiManager::CSgitApiManager(const std::string &ssSgitCfgPath)
  : m_ssSgitCfgPath(ssSgitCfgPath)
{

}

CSgitApiManager::~CSgitApiManager()
{

}

void CSgitApiManager::Init()
{
  AutoPtr<IniFileConfiguration> m_apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);

  std::string ssFlowPath = "";
	CToolkit::getStrinIfSet(m_apSgitConf, "global.FlowPath", ssFlowPath);

  std::string ssTradeServerAddr = m_apSgitConf->getString("global.TradeServerAddr");

  std::string ssTradeIdListKey = "global.TradeIDList";
  if (!m_apSgitConf->hasProperty(ssTradeIdListKey))
  {
    LOG(ERROR_LOG_LEVEL, "Can not find property:%s in %s", ssTradeIdListKey.c_str(), m_ssSgitCfgPath.c_str());
  }

  StringTokenizer st(m_apSgitConf->getString(ssTradeIdListKey), ",", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  for(StringTokenizer::Iterator it = st.begin(); it != st.end(); it++)
  {
    LinkAcct2Spi(CreateSpi(ssFlowPath, ssTradeServerAddr, *it), *it);
  }
}

SharedPtr<CSgitTradeSpi> CSgitApiManager::CreateSpi(const std::string &ssFlowPath, const std::string &ssTradeServerAddr, const std::string ssTradeId)
{
  CThostFtdcTraderApi *pTradeApi = CThostFtdcTraderApi::CreateFtdcTraderApi(ssFlowPath.c_str());
  SharedPtr<CSgitTradeSpi> spTradeSpi = new CSgitTradeSpi(pTradeApi, m_ssSgitCfgPath, ssTradeId);

  pTradeApi->IsReviveNtyCapital(false);
  pTradeApi->RegisterSpi(spTradeSpi);
  pTradeApi->SubscribePublicTopic(THOST_TERT_QUICK);
  pTradeApi->SubscribePrivateTopic(THOST_TERT_QUICK);

  pTradeApi->RegisterFront(const_cast<char*>(ssTradeServerAddr.c_str()));
  pTradeApi->Init();

  LOG(INFO_LOG_LEVEL, "Create api instance for TradeID:%s, RegisterFront tradeServerAddr:%s", 
    ssTradeId.c_str(), ssTradeServerAddr.c_str());

  return spTradeSpi;
}

void CSgitApiManager::LinkAcct2Spi(SharedPtr<CSgitTradeSpi> spTradeSpi, const std::string ssTradeId)
{
	StringTokenizer stAccounts(m_apSgitConf->getString(ssTradeId + ".Accounts"), ",", 
		StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

	for(StringTokenizer::Iterator it = stAccounts.begin(); it != stAccounts.end(); it++)
	{
		m_mapAcct2Spi[*it] = spTradeSpi;
	}

	std::string ssAccountAliasKey = ssTradeId + ".AccountAlias";
	if (!m_apSgitConf->hasProperty(ssAccountAliasKey)) return;

	//TargetCompID + OnBehalfOfCompID(如有)
	std::string ssOnBehalfOfCompID = "";
	CToolkit::getStrinIfSet(m_apSgitConf, ssTradeId + ".OnBehalfOfCompID", ssOnBehalfOfCompID);
	std::string ssPrefix = m_apSgitConf->getString(ssTradeId + ".TargetCompID") + ssOnBehalfOfCompID;

	StringTokenizer stAccountAlias(m_apSgitConf->getString(ssAccountAliasKey), ",", 
		StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

	for(StringTokenizer::Iterator it = stAccountAlias.begin(); it != stAccountAlias.end(); it++)
	{
		m_mapAlias2Acct[ssPrefix + *it] = m_apSgitConf->getString(ssTradeId + *it);

		m_mapAcct2Spi[ssPrefix + *it] = spTradeSpi;
	}
}

SharedPtr<CSgitTradeSpi> CSgitApiManager::GetApi(const FIX::Message& oMsg)
{
	FIX::Account account;
	oMsg.getField(account);

	//  message.getHeader().get(onBehalfOfCompId).getValue() : "";
	if (CToolkit::isAliasAcct(account.getValue()))
	{
		//如果account为账户别名，即包含字母，则要通过 SenderCompID + OnBehalfOfCompID + 别名 获取对应的Spi实例
		FIX::SenderCompID senderCompId;
		FIX::OnBehalfOfCompID onBehalfOfCompId;
		std::string ssSenderCompId = oMsg.getHeader().getFieldIfSet(senderCompId) ? senderCompId.getValue() : "";
		std::string ssOnBehalfOfCompID = oMsg.getHeader().getFieldIfSet(onBehalfOfCompId) ? onBehalfOfCompId.getValue() : "";


	}
	else
	{
		//如果account全为数字，则表示客户显式指定了账户，直接通过账户获取对应的Spi实例

	}
}

