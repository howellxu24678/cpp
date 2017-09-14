#include "SgitApiManager.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Util/IniFileConfiguration.h"

#include "Poco/File.h"
#include "Log.h"
#include "quickfix/SessionID.h"
#include "Toolkit.h"



CSgitApiManager::CSgitApiManager(const std::string &ssSgitCfgPath)
  : m_ssSgitCfgPath(ssSgitCfgPath)
{

}

CSgitApiManager::~CSgitApiManager()
{

}

bool CSgitApiManager::Init()
{
  try
  {
    if(!InitDict()) return false;

    if(!InitSgit()) return false;
  }
  catch ( std::exception & e)
  {
  	LOG(FATAL_LOG_LEVEL, "%s", e.what());
    return false;
  }

  return true;
}

SharedPtr<CSgitTradeSpi> CSgitApiManager::CreateSpi(const std::string &ssFlowPath, const std::string &ssTradeServerAddr, const std::string &ssTradeId)
{
  CThostFtdcTraderApi *pTradeApi = CThostFtdcTraderApi::CreateFtdcTraderApi(ssFlowPath.c_str());
  SharedPtr<CSgitTradeSpi> spTradeSpi = new CSgitTradeSpi(this, pTradeApi, m_ssSgitCfgPath, ssTradeId);

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

void CSgitApiManager::LinkAcct2Spi(SharedPtr<CSgitTradeSpi> spTradeSpi, const std::string &ssTradeId)
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
  std::string ssTargetCompID = m_apSgitConf->getString(ssTradeId + ".TargetCompID");
  std::string ssOnBehalfOfCompID = "";
  CToolkit::getStrinIfSet(m_apSgitConf, ssTradeId + ".OnBehalfOfCompID", ssOnBehalfOfCompID);

  StringTokenizer stAccountAlias(m_apSgitConf->getString(ssAccountAliasKey), ",", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

  for(StringTokenizer::Iterator it = stAccountAlias.begin(); it != stAccountAlias.end(); it++)
  {
    std::string ssAcctAliasKey = CToolkit::GenAcctAliasKey(ssTargetCompID, ssOnBehalfOfCompID, *it);
    std::string ssAcctValue = m_apSgitConf->getString(ssTradeId + "."+ *it);

    LOG(DEBUG_LOG_LEVEL, "AccountAlias:%s, Account:%s", ssAcctAliasKey.c_str(), ssAcctValue.c_str());

    m_mapAlias2Acct[ssAcctAliasKey] = ssAcctValue;
    m_mapAcct2Spi[ssAcctAliasKey] = spTradeSpi;
  }
}

SharedPtr<CSgitTradeSpi> CSgitApiManager::GetSpi(const FIX::Message& oMsg)
{
  FIX::Account account;
  oMsg.getField(account);

  //如果account全为数字，则表示客户显式指定了账户，直接通过账户获取对应的Spi实例
  if (!CToolkit::isAliasAcct(account.getValue())) return GetSpi(account.getValue());

  //如果account为账户别名，即包含字母，则要通过 SenderCompID + OnBehalfOfCompID + 别名 获取对应的Spi实例
	return GetSpi(CToolkit::GetAcctAliasKey(account.getValue(), oMsg));
}

SharedPtr<CSgitTradeSpi> CSgitApiManager::GetSpi(const std::string &ssKey)
{
  std::map<std::string, SharedPtr<CSgitTradeSpi>>::const_iterator cit = m_mapAcct2Spi.find(ssKey);
  if (cit != m_mapAcct2Spi.end())
  {
    return cit->second;
  }

  LOG(ERROR_LOG_LEVEL, "Can not find Spi by key:%s", ssKey.c_str());
  return nullptr;
}

std::string CSgitApiManager::GetRealAccont(const FIX::Message& oMsg)
{
	FIX::Account account;
	oMsg.getField(account);

	if(!CToolkit::isAliasAcct(account.getValue())) return account.getValue();

	std::string ssAcctAliasKey = CToolkit::GetAcctAliasKey(account.getValue(), oMsg);
	std::map<std::string, std::string>::const_iterator cit = m_mapAlias2Acct.find(ssAcctAliasKey);
	if (cit != m_mapAlias2Acct.end())
	{
		return cit->second;
	}

	LOG(ERROR_LOG_LEVEL, "Can not find Real Account by key:%s", ssAcctAliasKey.c_str());
	return "";
}

bool CSgitApiManager::InitSgit()
{
  m_apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);

  std::string ssFlowPath = "";
  CToolkit::getStrinIfSet(m_apSgitConf, "global.FlowPath", ssFlowPath); 

  std::string ssTradeServerAddr = m_apSgitConf->getString("global.TradeServerAddr");

  std::string ssTradeIdListKey = "global.TradeIDList";
  if (!m_apSgitConf->hasProperty(ssTradeIdListKey))
  {
    LOG(ERROR_LOG_LEVEL, "Can not find property:%s in %s", ssTradeIdListKey.c_str(), m_ssSgitCfgPath.c_str());
    return false;
  }

  StringTokenizer st(m_apSgitConf->getString(ssTradeIdListKey), ",", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  for(StringTokenizer::Iterator it = st.begin(); it != st.end(); it++)
  {
    LinkAcct2Spi(CreateSpi(ssFlowPath, ssTradeServerAddr, *it), *it);
  }

  return true;
}

bool CSgitApiManager::InitDict()
{
  //AutoPtr<JSONConfiguration> apJsonConf = new JSONConfiguration(m_ssDictCfgPath);
  //PrintJsonValue("symbols", apJsonConf);

	AutoPtr<XMLConfiguration> apXmlConf = new XMLConfiguration(m_ssDictCfgPath);
	AbstractConfiguration::Keys ks;
	apXmlConf->keys("symbols", ks);
	LOG(INFO_LOG_LEVEL, "ks size:%d", ks.size());
	for (AbstractConfiguration::Keys::iterator it = ks.begin(); it != ks.end(); it++)
	{
		LOG(INFO_LOG_LEVEL, "xml it:%s", it->c_str());
	}
	//LOG(INFO_LOG_LEVEL, "xml1:%s", apXmlConf->getString("symbols.symbol").c_str());
	//LOG(INFO_LOG_LEVEL, "xml2:%s", apXmlConf->getString("prop1").c_str());

  return true;
}

void CSgitApiManager::PrintJsonValue(const std::string &ssKey, AutoPtr<JSONConfiguration> apJson)
{
  AbstractConfiguration::Keys ks;
  apJson->keys(ssKey, ks);

  if (ks.size() < 2)
  {
    LOG(INFO_LOG_LEVEL, "ssKey:%s, value:%s", ssKey.c_str(), apJson->getString(ssKey).c_str());
    return;
  }

  for (AbstractConfiguration::Keys::iterator it = ks.begin(); it != ks.end(); it++)
  {
    PrintJsonValue(ssKey + "." + *it, apJson);
  }
}

