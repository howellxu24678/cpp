#include "SgitContext.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Util/IniFileConfiguration.h"

#include "Poco/File.h"
#include "Log.h"
#include "quickfix/SessionID.h"
#include "Toolkit.h"
#include "quickfix/Session.h"



CSgitContext::CSgitContext(const std::string &ssSgitCfgPath, const std::string &ssCvtCfgPath)
  : m_ssSgitCfgPath(ssSgitCfgPath)
  , m_oConvert(ssCvtCfgPath)
  , m_ssCvtCfgPath(ssCvtCfgPath)
{

}

CSgitContext::~CSgitContext()
{

}

bool CSgitContext::Init()
{
  try
  {
    if(!InitConvert()) return false;

		LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol("IF1812", Convert::Bloomberg).c_str());
		LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol(m_oConvert.CvtSymbol("IF1812", Convert::Bloomberg), Convert::Original).c_str());
		LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol("IF112", Convert::Bloomberg).c_str());
		LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol("FG801", Convert::Bloomberg).c_str());
		LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol(m_oConvert.CvtSymbol("FG801", Convert::Bloomberg), Convert::Original).c_str());
		LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol("au1801", Convert::Bloomberg).c_str());
		LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol(m_oConvert.CvtSymbol("au1801", Convert::Bloomberg), Convert::Original).c_str());

    if(!InitSgitApi()) return false;
  }
  catch ( std::exception & e)
  {
  	LOG(FATAL_LOG_LEVEL, "%s", e.what());
    return false;
  }

  return true;
}

SharedPtr<CSgitTdSpi> CSgitContext::CreateTdSpi(const std::string &ssFlowPath, const std::string &ssTradeServerAddr, const std::string &ssTradeId)
{
  CThostFtdcTraderApi *pTradeApi = CThostFtdcTraderApi::CreateFtdcTraderApi(ssFlowPath.c_str());
  SharedPtr<CSgitTdSpi> spTradeSpi = new CSgitTdSpi(this, pTradeApi, m_ssSgitCfgPath, ssTradeId);
  spTradeSpi->Init();

  pTradeApi->IsReviveNtyCapital(false);
  pTradeApi->RegisterSpi(spTradeSpi);
  pTradeApi->SubscribePublicTopic(THOST_TERT_QUICK);
  pTradeApi->SubscribePrivateTopic(THOST_TERT_QUICK);

  pTradeApi->RegisterFront(const_cast<char*>(ssTradeServerAddr.c_str()));
  pTradeApi->Init();

  LOG(INFO_LOG_LEVEL, "Create trade api instance for TradeID:%s, RegisterFront tradeServerAddr:%s", 
    ssTradeId.c_str(), ssTradeServerAddr.c_str());

  return spTradeSpi;
}

SharedPtr<CSgitMdSpi> CSgitContext::CreateMdSpi(const std::string &ssFlowPath, const std::string &ssMdServerAddr)
{
  CThostFtdcMdApi* pMdApi = CThostFtdcMdApi::CreateFtdcMdApi(ssFlowPath.c_str());
  m_spMdSpi = new CSgitMdSpi(pMdApi);
  pMdApi->RegisterSpi(m_spMdSpi);
  pMdApi->RegisterFront(const_cast<char*>(ssMdServerAddr.c_str()));
  pMdApi->Init();

  LOG(INFO_LOG_LEVEL, "Create market api instance");

  return m_spMdSpi;
}

void CSgitContext::LinkAcct2TdSpi(SharedPtr<CSgitTdSpi> spTradeSpi, const std::string &ssTradeId)
{
  StringTokenizer stAccounts(m_apSgitConf->getString(ssTradeId + ".Accounts"), ",", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

  for(StringTokenizer::Iterator it = stAccounts.begin(); it != stAccounts.end(); it++)
  {
    m_mapAcct2TdSpi[*it] = spTradeSpi;
  }

  std::string ssAccountAliasKey = ssTradeId + ".AccountAlias";
  if (!m_apSgitConf->hasProperty(ssAccountAliasKey)) return;

  //SessionID + OnBehalfOfCompID(����,��ͨ��������hub��������Ҫ���ֵ)
  std::string ssBeginString = m_apSgitConf->getString(ssTradeId + ".BeginString");
  std::string ssSenderCompID = m_apSgitConf->getString(ssTradeId + ".SenderCompID");
  std::string ssTargetCompID = m_apSgitConf->getString(ssTradeId + ".TargetCompID");
  FIX::SessionID oSessionID = FIX::SessionID(ssBeginString, ssSenderCompID, ssTargetCompID);
  std::string ssOnBehalfOfCompID = "";
  CToolkit::getStrinIfSet(m_apSgitConf, ssTradeId + ".OnBehalfOfCompID", ssOnBehalfOfCompID);

  StringTokenizer stAccountAlias(m_apSgitConf->getString(ssAccountAliasKey), ",", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

  for(StringTokenizer::Iterator it = stAccountAlias.begin(); it != stAccountAlias.end(); it++)
  {
    std::string ssAcctAliasKey = CToolkit::GenAcctAliasKey(oSessionID, ssOnBehalfOfCompID, *it);
    std::string ssAcctValue = m_apSgitConf->getString(ssTradeId + "."+ *it);

    LOG(DEBUG_LOG_LEVEL, "AccountAlias:%s, Account:%s", ssAcctAliasKey.c_str(), ssAcctValue.c_str());

    m_mapAlias2Acct[ssAcctAliasKey] = ssAcctValue;
    m_mapAcct2TdSpi[ssAcctAliasKey] = spTradeSpi;
  }
}

SharedPtr<CSgitTdSpi> CSgitContext::GetTdSpi(const FIX::Message& oMsg)
{
  FIX::Account account;
  oMsg.getField(account);

  //���accountȫΪ���֣����ʾ�ͻ���ʽָ�����˻���ֱ��ͨ���˻���ȡ��Ӧ��Spiʵ��
  if (!CToolkit::isAliasAcct(account.getValue())) return GetTdSpi(account.getValue());

  //���accountΪ�˻���������������ĸ����Ҫͨ�� SessionID + OnBehalfOfCompID + ���� ��ȡ��Ӧ��Spiʵ��
	return GetTdSpi(CToolkit::GenAcctAliasKey(account.getValue(), oMsg));
}

SharedPtr<CSgitTdSpi> CSgitContext::GetTdSpi(const std::string &ssKey)
{
  std::map<std::string, SharedPtr<CSgitTdSpi>>::const_iterator cit = m_mapAcct2TdSpi.find(ssKey);
  if (cit != m_mapAcct2TdSpi.end())
  {
    return cit->second;
  }

  LOG(ERROR_LOG_LEVEL, "Can not find Spi by key:%s", ssKey.c_str());
  return nullptr;
}

std::string CSgitContext::GetRealAccont(const FIX::Message& oRecvMsg)
{
	FIX::Account account;
	oRecvMsg.getField(account);

	if(!CToolkit::isAliasAcct(account.getValue())) return account.getValue();

	std::string ssAcctAliasKey = CToolkit::GenAcctAliasKey(account.getValue(), oRecvMsg);
	std::map<std::string, std::string>::const_iterator cit = m_mapAlias2Acct.find(ssAcctAliasKey);
	if (cit != m_mapAlias2Acct.end())
	{
		return cit->second;
	}

	LOG(ERROR_LOG_LEVEL, "Can not find Real Account by key:%s", ssAcctAliasKey.c_str());
	return "";
}

bool CSgitContext::InitSgitApi()
{
  m_apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);

  std::string ssFlowPath = "";
  CToolkit::getStrinIfSet(m_apSgitConf, "global.FlowPath", ssFlowPath); 

  std::string ssTdServerAddr = m_apSgitConf->getString("global.TradeServerAddr");
  std::string ssMdServerAddr = m_apSgitConf->getString("global.QuoteServerAddr");

  LOG(INFO_LOG_LEVEL, "TdApi version:%s, MdApi version:%s", 
    CThostFtdcTraderApi::GetApiVersion(), CThostFtdcMdApi::GetApiVersion());
  
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
    LinkAcct2TdSpi(CreateTdSpi(ssFlowPath, ssTdServerAddr, *it), *it);
  }

  CreateMdSpi(ssFlowPath, ssMdServerAddr);

  return true;
}

bool CSgitContext::InitConvert()
{
  return m_oConvert.Init();
}

char CSgitContext::CvtDict(const int iField, const char cValue, const Convert::EnDictType enDstDictType)
{
  return m_oConvert.CvtDict(iField, cValue, enDstDictType);
}

std::string CSgitContext::CvtSymbol(const std::string &ssSymbol, const Convert::EnCvtType enDstType)
{
  return m_oConvert.CvtSymbol(ssSymbol, enDstType);
}

void CSgitContext::Send(const std::string &ssAcct, FIX::Message &oMsg)
{
  /*
  1. �ҵ�ԭʼ������˻�����������������
  2. ԭʼSessionID
  3. header�е���Ϣ
  */
  STUFixInfo stuFixInfo;
  if(!GetFixInfo(ssAcct, stuFixInfo))
  {
    LOG(ERROR_LOG_LEVEL, "Failed to get fixinto by account:%s", ssAcct.c_str());
    return;
  }

  SetFixInfo(stuFixInfo, oMsg);
  try
  {
    FIX::Session::sendToTarget( oMsg, stuFixInfo.m_oSessionID );
  }
  catch ( FIX::SessionNotFound& e) 
  {
    LOG(ERROR_LOG_LEVEL, "%s", e.what());
  }
}

void CSgitContext::AddFixInfo(const FIX::Message& oMsg, const FIX::SessionID& sessionID)
{
  std::string ssRealAccount = GetRealAccont(oMsg); 
  if(m_mapAcct2FixInfo.count(ssRealAccount) > 0) return;

  FIX::Account account;
  oMsg.getField(account);

  STUFixInfo stuFixInfo;
  stuFixInfo.m_ssAcctRecv = account.getValue();
  stuFixInfo.m_oSessionID = sessionID;
  stuFixInfo.m_oHeader = oMsg.getHeader();

  m_mapAcct2FixInfo[ssRealAccount] = stuFixInfo;
}

bool CSgitContext::GetFixInfo(const std::string &ssAcct, STUFixInfo &stuFixInfo)
{
  std::map<std::string, STUFixInfo>::const_iterator cit = m_mapAcct2FixInfo.find(ssAcct);
  if(cit != m_mapAcct2FixInfo.end())
  {
    stuFixInfo = cit->second;
    return true;
  }

  return false;
}

void CSgitContext::SetFixInfo(const STUFixInfo &stuFixInfo, FIX::Message &oMsg)
{
  oMsg.setField(FIX::Account(stuFixInfo.m_ssAcctRecv));

  
  FIX::OnBehalfOfCompID onBehalfOfCompID;
  if (stuFixInfo.m_oHeader.isSetField(onBehalfOfCompID.getField()))
  {
    FIX::DeliverToCompID deliverToCompID(stuFixInfo.m_oHeader.getField(onBehalfOfCompID.getField()));
    oMsg.getHeader().setField(deliverToCompID);
  }

  FIX::SenderSubID senderSubID;
  if (stuFixInfo.m_oHeader.isSetField(senderSubID.getField()))
  {
    FIX::TargetSubID targetSubID(stuFixInfo.m_oHeader.getField(senderSubID.getField()));
    oMsg.getHeader().setField(targetSubID);
  }

  FIX::OnBehalfOfSubID onBehalfOfSubID;
  if (stuFixInfo.m_oHeader.isSetField(onBehalfOfSubID.getField()))
  {
    FIX::DeliverToSubID deliverToSubID(stuFixInfo.m_oHeader.getField(onBehalfOfSubID.getField()));
    oMsg.getHeader().setField(deliverToSubID);
  }
}

std::string CSgitContext::CvtExchange(const std::string &ssExchange, const Convert::EnCvtType enDstType)
{
	return m_oConvert.CvtExchange(ssExchange, enDstType);
}

SharedPtr<CSgitMdSpi> CSgitContext::GetMdSpi(const FIX::Message& oMsg)
{
  return m_spMdSpi;
}



