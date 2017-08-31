#include "SgitApiManager.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "Log.h"
#include "quickfix/SessionID.h"

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
  AutoPtr<IniFileConfiguration> apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);

  std::string ssFlowPath = apSgitConf->hasProperty("global.flowPath") ? 
    apSgitConf->getString("global.flowPath").c_str() : "";
  std::string ssTradeServer = apSgitConf->getString("global.tradeServer");

  std::string ssTradeIdListKey = "global.tradeIDList";
  if (!apSgitConf->hasProperty(ssTradeIdListKey))
  {
    LOG(ERROR_LOG_LEVEL, "Can not find property:%s in %s", ssTradeIdListKey.c_str(), m_ssSgitCfgPath.c_str());
  }

  StringTokenizer st(apSgitConf->getString(ssTradeIdListKey), ",", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  for(StringTokenizer::Iterator it = st.begin(); it != st.end(); it++)
  {
    LinkAcct2Api(CreateApiSpi(ssFlowPath, ssTradeServer, *it), *it);
  }
}

CThostFtdcTraderApi* CSgitApiManager::CreateApiSpi(const std::string &ssFlowPath, const std::string &ssTradeServer, const std::string ssTradeId)
{
  CThostFtdcTraderApi *pTradeApi = CThostFtdcTraderApi::CreateFtdcTraderApi(ssFlowPath.c_str());
  SharedPtr<CSgitTradeSpi> spTradeSpi = new CSgitTradeSpi(pTradeApi, m_ssSgitCfgPath, ssTradeId);

  pTradeApi->IsReviveNtyCapital(false);
  pTradeApi->RegisterSpi(spTradeSpi);
  pTradeApi->SubscribePublicTopic(THOST_TERT_QUICK);
  pTradeApi->SubscribePrivateTopic(THOST_TERT_QUICK);

  pTradeApi->RegisterFront(const_cast<char*>(ssTradeServer.c_str()));
  pTradeApi->Init();

  LOG(INFO_LOG_LEVEL, "Create api instance for TradeID:%s, RegisterFront tradeServer:%s", 
    ssTradeId.c_str(), ssTradeServer.c_str());

  return pTradeApi;
}

void CSgitApiManager::LinkAcct2Api(CThostFtdcTraderApi* pTradeApi, const std::string ssTradeId)
{
  //CThostFtdcTraderApi *pTradeApi = CreateApiSpi(ssFlowPath, ssTradeServer, *it);
  //m_mapAlias2Acct[apSgitConf->getString(*it + ".TargetCompID") + 
  //  (apSgitConf->hasProperty(*it + ".OnBehalfOfCompID") ? 
  //  apSgitConf->getString(*it + ".OnBehalfOfCompID") : "") ] = apSgitConf->getString(*it + ".FUT_ACCT");
}

CThostFtdcTraderApi* CSgitApiManager::GetApi(const FIX::Message& oMsg)
{

}

