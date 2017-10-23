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
  CThostFtdcTraderApi *pTdReqApi = CThostFtdcTraderApi::CreateFtdcTraderApi(ssFlowPath.c_str());
  SharedPtr<CSgitTdSpi> spTdSpi = new CSgitTdSpi(this, pTdReqApi, m_ssSgitCfgPath, ssTradeId);
  spTdSpi->Init();

  pTdReqApi->IsReviveNtyCapital(false);
  pTdReqApi->RegisterSpi(spTdSpi);
  pTdReqApi->SubscribePublicTopic(THOST_TERT_QUICK);
  pTdReqApi->SubscribePrivateTopic(THOST_TERT_QUICK);

  pTdReqApi->RegisterFront(const_cast<char*>(ssTradeServerAddr.c_str()));
  pTdReqApi->Init();

  LOG(INFO_LOG_LEVEL, "Create trade api instance for TradeID:%s, RegisterFront tradeServerAddr:%s", 
    ssTradeId.c_str(), ssTradeServerAddr.c_str());

  return spTdSpi;
}

void CSgitContext::CreateMdSpi(const std::string &ssFlowPath, const std::string &ssMdServerAddr, const std::string &ssTradeId, const std::string &ssPassword)
{
  CThostFtdcMdApi	*pMdReqApi = CThostFtdcMdApi::CreateFtdcMdApi(ssFlowPath.c_str());
  m_spMdSpi = new CSgitMdSpi(this, pMdReqApi, ssTradeId, ssPassword);

  pMdReqApi->RegisterSpi(m_spMdSpi);
  pMdReqApi->RegisterFront(const_cast<char*>(ssMdServerAddr.c_str()));
  pMdReqApi->Init();

  LOG(INFO_LOG_LEVEL, "Create market api instance for TradeID:%s, RegisterFront tradeServerAddr:%s",
		ssTradeId.c_str(), ssMdServerAddr.c_str());
}

void CSgitContext::LinkAcct2TdSpi(SharedPtr<CSgitTdSpi> spTdSpi, const std::string &ssTradeId)
{
  //建立真实资金账号和TdSpi实例的对应关系
  StringTokenizer stAccounts(m_apSgitConf->getString(ssTradeId + ".Accounts"), ",", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

  for(StringTokenizer::Iterator it = stAccounts.begin(); it != stAccounts.end(); it++)
  {
    m_mapAcct2TdSpi[*it] = spTdSpi;
		//m_mapAcct2MdSpi[*it] = spMdSpi;
  }

  //如果配置账户别名，建立账户别名和TdSpi实例的对应关系，账户别名和真实资金账号的对应关系
  std::string ssAccountAliasKey = ssTradeId + ".AccountAlias";
  if (!m_apSgitConf->hasProperty(ssAccountAliasKey)) return;

  //SessionID + OnBehalfOfCompID(如有,彭博通过第三方hub过来的需要这个值)
  std::string ssBeginString = m_apSgitConf->getString(ssTradeId + ".BeginString");
  std::string ssSenderCompID = m_apSgitConf->getString(ssTradeId + ".SenderCompID");
  std::string ssTargetCompID = m_apSgitConf->getString(ssTradeId + ".TargetCompID");

  std::string ssOnBehalfOfCompID = "";
  CToolkit::GetStrinIfSet(m_apSgitConf, ssTradeId + ".OnBehalfOfCompID", ssOnBehalfOfCompID);

  StringTokenizer stAccountAlias(m_apSgitConf->getString(ssAccountAliasKey), ",", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

  for(StringTokenizer::Iterator it = stAccountAlias.begin(); it != stAccountAlias.end(); it++)
  {
    std::string ssAcctAliasKey = CToolkit::GenAcctAliasKey(FIX::SessionID(ssBeginString, ssTargetCompID, ssSenderCompID), ssOnBehalfOfCompID, *it);
    std::string ssAcctValue = m_apSgitConf->getString(ssTradeId + "."+ *it);

    LOG(DEBUG_LOG_LEVEL, "AccountAlias:%s, Account:%s", ssAcctAliasKey.c_str(), ssAcctValue.c_str());

    m_mapAlias2Acct[ssAcctAliasKey] = ssAcctValue;
    m_mapAcct2TdSpi[ssAcctAliasKey] = spTdSpi;
		//m_mapAcct2MdSpi[ssAcctAliasKey] = spMdSpi;
  }
}

SharedPtr<CSgitTdSpi> CSgitContext::GetTdSpi(const FIX::Message& oMsg)
{
  FIX::Account account;
  oMsg.getField(account);

  //如果account全为数字，则表示客户显式指定了账户，直接通过账户获取对应的Spi实例
  if (!CToolkit::IsAliasAcct(account.getValue())) return GetTdSpi(account.getValue());

  //如果account为账户别名，即包含字母，则要通过 SessionID + OnBehalfOfCompID + 别名 获取对应的Spi实例
	return GetTdSpi(CToolkit::GenAcctAliasKey(oMsg, account.getValue()));
}

SharedPtr<CSgitMdSpi> CSgitContext::GetMdSpi(const FIX::Message& oMsg)
{
	//FIX::Account account;
	//oMsg.getField(account);

	////如果account全为数字，则表示客户显式指定了账户，直接通过账户获取对应的Spi实例
	//if (!CToolkit::isAliasAcct(account.getValue())) return GetMdSpi(account.getValue());

	////如果account为账户别名，即包含字母，则要通过 SessionID + OnBehalfOfCompID + 别名 获取对应的Spi实例
	//return GetMdSpi(CToolkit::GenAcctAliasKey(account.getValue(), oMsg));

  return m_spMdSpi;
}

//SharedPtr<CSgitMdSpi> CSgitContext::GetMdSpi(const std::string &ssKey)
//{
//	std::map<std::string, SharedPtr<CSgitMdSpi>>::const_iterator cit = m_mapAcct2MdSpi.find(ssKey);
//	if (cit != m_mapAcct2MdSpi.end())
//	{
//		return cit->second;
//	}
//
//	LOG(ERROR_LOG_LEVEL, "Can not find MdSpi by key:%s", ssKey.c_str());
//	return NULL;
//}

SharedPtr<CSgitTdSpi> CSgitContext::GetTdSpi(const std::string &ssKey)
{
  std::map<std::string, SharedPtr<CSgitTdSpi>>::const_iterator cit = m_mapAcct2TdSpi.find(ssKey);
  if (cit != m_mapAcct2TdSpi.end())
  {
    return cit->second;
  }

  LOG(ERROR_LOG_LEVEL, "Can not find TdSpi by key:%s", ssKey.c_str());
  return NULL;
}

std::string CSgitContext::GetRealAccont(const FIX::Message& oRecvMsg)
{
	FIX::Account account;
	oRecvMsg.getField(account);

	if(!CToolkit::IsAliasAcct(account.getValue())) return account.getValue();

	std::string ssAcctAliasKey = CToolkit::GenAcctAliasKey(oRecvMsg, account.getValue());
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
  LOG(INFO_LOG_LEVEL, "TdApi version:%s, MdApi version:%s", 
    CThostFtdcTraderApi::GetApiVersion(), CThostFtdcMdApi::GetApiVersion());

  m_apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);

  std::string ssFlowPath = "";
  CToolkit::GetStrinIfSet(m_apSgitConf, "global.FlowPath", ssFlowPath); 

  std::string ssTdServerAddr = m_apSgitConf->getString("global.TradeServerAddr");
  std::string ssMdServerAddr = m_apSgitConf->getString("global.QuoteServerAddr");
  std::string ssQuoteAccount = m_apSgitConf->getString("global.QuoteAccount");
  LOG(INFO_LOG_LEVEL, "QuoteAccount:%s", ssQuoteAccount.c_str());
  StringTokenizer stQuoteAcct(ssQuoteAccount, ":", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  CreateMdSpi(ssFlowPath, ssMdServerAddr, stQuoteAcct[0], stQuoteAcct[1]);

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
  1. 找到原始送入的账户（真名？别名？）
  2. 原始SessionID
  3. header中的信息
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

void CSgitContext::AddFixInfo(const FIX::Message& oMsg)
{
  STUFixInfo stuFixInfo;
  stuFixInfo.m_oSessionID = oMsg.getSessionID();
  stuFixInfo.m_oHeader = oMsg.getHeader();
  AddFixInfo(CToolkit::GetSessionKey(oMsg), stuFixInfo);


  FIX::Account account;
  oMsg.getFieldIfSet(account);
  if (account.getValue().empty()) return;

  stuFixInfo.m_ssAcctRecv = account.getValue();
  AddFixInfo(GetRealAccont(oMsg), stuFixInfo);
}

void CSgitContext::AddFixInfo(const std::string &ssKey, const STUFixInfo &stuFixInfo)
{
  if (m_mapSessionAcct2FixInfo.count(ssKey) > 0) return;
  m_mapSessionAcct2FixInfo[ssKey] = stuFixInfo;
}

bool CSgitContext::GetFixInfo(const std::string &ssAcct, STUFixInfo &stuFixInfo)
{
  std::map<std::string, STUFixInfo>::const_iterator cit = m_mapSessionAcct2FixInfo.find(ssAcct);
  if(cit != m_mapSessionAcct2FixInfo.end())
  {
    stuFixInfo = cit->second;
    return true;
  }

  return false;
}

void CSgitContext::SetFixInfo(const STUFixInfo &stuFixInfo, FIX::Message &oMsg)
{
  if (!stuFixInfo.m_ssAcctRecv.empty())
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

void CSgitContext::Deal(const FIX::Message& oMsg)
{
  const FIX::BeginString& beginString = 
    FIELD_GET_REF( oMsg.getHeader(), BeginString);
  if ( beginString != FIX::BeginString_FIX42 ) return;

  FIX::MsgType msgType;
  oMsg.getHeader().getField(msgType);

  if(CToolkit::IsTdRequest(msgType))
  {

  }
  else if(CToolkit::IsMdRequest(msgType))
  {
    SharedPtr<CSgitMdSpi> spMdSpi = GetMdSpi(oMsg);
    if (spMdSpi)
    {
      spMdSpi->MarketDataRequest(oMsg);
    }
  }
}

