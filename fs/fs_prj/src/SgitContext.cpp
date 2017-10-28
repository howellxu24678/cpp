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

		//if(!InitFixUserConf()) return false;
  }
  catch ( std::exception & e)
  {
  	LOG(FATAL_LOG_LEVEL, "%s", e.what());
    return false;
  }

  return true;
}

SharedPtr<CSgitTdSpi> CSgitContext::CreateTdSpi(const std::string &ssFlowPath, const std::string &ssTradeServerAddr, CSgitTdSpi::STUTdParam &stuTdParam, CSgitTdSpi::EnTdSpiRole enTdSpiRole)
{
  CThostFtdcTraderApi *pTdReqApi = CThostFtdcTraderApi::CreateFtdcTraderApi(ssFlowPath.c_str());
  stuTdParam.m_pTdReqApi = pTdReqApi;
  SharedPtr<CSgitTdSpi> spTdSpi = NULL;
  if (enTdSpiRole == CSgitTdSpi::HubTran)
  {
    spTdSpi = new CSgitTdSpiHubTran(stuTdParam);
  }
  else
  {
    spTdSpi = new CSgitTdSpiDirect(stuTdParam);
  }
  spTdSpi->Init();

  pTdReqApi->IsReviveNtyCapital(false);
  pTdReqApi->RegisterSpi(spTdSpi);
  pTdReqApi->SubscribePublicTopic(THOST_TERT_QUICK);
  pTdReqApi->SubscribePrivateTopic(THOST_TERT_QUICK);

  pTdReqApi->RegisterFront(const_cast<char*>(ssTradeServerAddr.c_str()));
  pTdReqApi->Init();

  LOG(INFO_LOG_LEVEL, "Create trade api instance for TradeID:%s, RegisterFront tradeServerAddr:%s", 
    stuTdParam.m_ssUserId.c_str(), ssTradeServerAddr.c_str());

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

bool CSgitContext::LinkSessionID2TdSpi(const std::string &ssSessionID, SharedPtr<CSgitTdSpi> spTdSpi)
{
  ScopedWriteRWLock scopeWriteLock(m_rwSessionID2TdSpi);

	std::pair<std::map<std::string, SharedPtr<CSgitTdSpi>>::iterator, bool> ret = 
		m_mapSessionID2TdSpi.insert(std::pair<std::string, SharedPtr<CSgitTdSpi>>(ssSessionID, spTdSpi));

	if (ret.second == false)
	{
		LOG(ERROR_LOG_LEVEL, "SessionID:%s is already in map", ssSessionID.c_str());
		return false;
	}

	return true;
}

SharedPtr<CSgitTdSpi> CSgitContext::GetTdSpi(const FIX::Message& oMsg)
{
  ScopedReadRWLock scopeReadLock(m_rwSessionID2TdSpi);
	std::map<std::string, SharedPtr<CSgitTdSpi>>::const_iterator cit = m_mapSessionID2TdSpi.find(oMsg.getSessionID().toString());
	if (cit != m_mapSessionID2TdSpi.end())
	{
		return cit->second;
	}

	LOG(ERROR_LOG_LEVEL, "Can not find TdSpi by SessionID:%s", oMsg.getSessionID().toString().c_str());
	return NULL;
}

SharedPtr<CSgitMdSpi> CSgitContext::GetMdSpi(const FIX::Message& oMsg)
{
  return m_spMdSpi;
}

SharedPtr<CSgitTdSpi> CSgitContext::GetTdSpi(const std::string &ssKey)
{
  std::map<std::string, SharedPtr<CSgitTdSpi>>::const_iterator cit = m_mapSessionID2TdSpi.find(ssKey);
  if (cit != m_mapSessionID2TdSpi.end())
  {
    return cit->second;
  }

  LOG(ERROR_LOG_LEVEL, "Can not find TdSpi by key:%s", ssKey.c_str());
  return NULL;
}

//std::string CSgitContext::GetRealAccont(const FIX::Message& oRecvMsg)
//{
//	FIX::Account account;
//	oRecvMsg.getField(account);
//
//	if(!CToolkit::IsAliasAcct(account.getValue())) return account.getValue();
//
//	std::string ssAcctAliasKey = CToolkit::GenAcctAliasKey(oRecvMsg, account.getValue());
//	std::map<std::string, std::string>::const_iterator cit = m_mapAlias2Acct.find(ssAcctAliasKey);
//	if (cit != m_mapAlias2Acct.end())
//	{
//		return cit->second;
//	}
//
//	LOG(ERROR_LOG_LEVEL, "Can not find Real Account by key:%s", ssAcctAliasKey.c_str());
//	return "";
//}

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
  StringTokenizer stQuoteUserIdPassword(ssQuoteAccount, ":", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  CreateMdSpi(ssFlowPath, ssMdServerAddr, stQuoteUserIdPassword[0], stQuoteUserIdPassword[1]);

  std::string ssTradeAccountListKey = "global.TradeAccountList", ssFixSessionProp = "", ssSessionID = "";

  if (!m_apSgitConf->hasProperty(ssTradeAccountListKey)) return true;

	StringTokenizer stTradeAccountList(m_apSgitConf->getString(ssTradeAccountListKey), ";", 
		StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
	for (StringTokenizer::Iterator it = stTradeAccountList.begin(); it != stTradeAccountList.end(); it++)
	{
		StringTokenizer stTdUserIdPassword(*it, ":", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

		ssFixSessionProp = "global." + stTdUserIdPassword[0];
		if (!m_apSgitConf->hasProperty(ssFixSessionProp))
		{
			LOG(ERROR_LOG_LEVEL, "Can not find the fix session of %s, property:%s", stTdUserIdPassword[0].c_str(), ssFixSessionProp.c_str());
			return false;
		}

    ssSessionID = m_apSgitConf->getString(ssFixSessionProp);

    CSgitTdSpi::STUTdParam stuTdParam;
    stuTdParam.m_pSgitCtx = this;
    stuTdParam.m_ssUserId = stTdUserIdPassword[0];
    stuTdParam.m_ssPassword = stTdUserIdPassword[1];
    stuTdParam.m_ssSessionID = ssSessionID;
    stuTdParam.m_ssSgitCfgPath = m_ssSgitCfgPath;

		if(!LinkSessionID2TdSpi(ssSessionID, 
			CreateTdSpi(ssFlowPath, ssTdServerAddr, stuTdParam, CSgitTdSpi::HubTran))) return false;
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
  //AddFixInfo(GetRealAccont(oMsg), stuFixInfo);
}

void CSgitContext::AddFixInfo(const std::string &ssKey, const STUFixInfo &stuFixInfo)
{
  if (m_mapAcct2FixInfo.count(ssKey) > 0) return;
  m_mapAcct2FixInfo[ssKey] = stuFixInfo;
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
		SharedPtr<CSgitTdSpi> spTdSpi = GetTdSpi(oMsg);
		if (!spTdSpi)
		{
			LOG(ERROR_LOG_LEVEL, "Can not find Tdspi for SessionID:%s", oMsg.getSessionID().toString().c_str());
			return;
		}
		spTdSpi->OnMessage(oMsg);
  }
  else if(CToolkit::IsMdRequest(msgType))
  {
    SharedPtr<CSgitMdSpi> spMdSpi = GetMdSpi(oMsg);
		if(!spMdSpi)
		{
			LOG(ERROR_LOG_LEVEL, "Can not find Mdspi for SessionID:%s", oMsg.getSessionID().toString().c_str());
			return;
		}
		spMdSpi->OnMessage(oMsg);
  }
}

//bool CSgitContext::InitFixUserConf()
//{
//	AbstractConfiguration::Keys kProp;
//	m_apSgitConf->keys(kProp);
//
//	for (AbstractConfiguration::Keys::iterator itProp = kProp.begin(); itProp != kProp.end(); itProp++)
//	{
//    if (strncmp(itProp->c_str(), G_FIX42.c_str(), G_FIX42.size()) == 0)
//    {
//      if(m_apSgitConf->hasProperty(*itProp + ".SymbolType"))
//      {
//        ScopedWriteRWLock scopeWriteLock(m_rwFixUser2CvtType);
//        m_mapFixUser2CvtType[*itProp] = (Convert::EnCvtType) m_apSgitConf->getInt(*itProp + ".SymbolType");
//      }
//      LOG(INFO_LOG_LEVEL, "itProp:%s", itProp->c_str());
//    }
//	}
//
//	return true;
//}


