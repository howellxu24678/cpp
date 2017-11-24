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

		//LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol("IF1812", Convert::Bloomberg).c_str());
		//LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol(m_oConvert.CvtSymbol("IF1812", Convert::Bloomberg), Convert::Original).c_str());
		//LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol("IF112", Convert::Bloomberg).c_str());
		//LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol("FG801", Convert::Bloomberg).c_str());
		//LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol(m_oConvert.CvtSymbol("FG801", Convert::Bloomberg), Convert::Original).c_str());
		//LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol("au1801", Convert::Bloomberg).c_str());
		//LOG(DEBUG_LOG_LEVEL, "%s", m_oConvert.CvtSymbol(m_oConvert.CvtSymbol("au1801", Convert::Bloomberg), Convert::Original).c_str());

    if(!InitSgitApi()) return false;
  }
  catch ( std::exception & e)
  {
  	LOG(FATAL_LOG_LEVEL, "%s", e.what());
    return false;
  }

  return true;
}

SharedPtr<CSgitTdSpi> CSgitContext::CreateTdSpi(CSgitTdSpi::STUTdParam &stuTdParam, CSgitTdSpi::EnTdSpiRole enTdSpiRole)
{
  CThostFtdcTraderApi *pTdReqApi = CThostFtdcTraderApi::CreateFtdcTraderApi(m_ssFlowPath.c_str());
  stuTdParam.m_pSgitCtx = this;
  stuTdParam.m_pTdReqApi = pTdReqApi;
  stuTdParam.m_ssSgitCfgPath = m_ssSgitCfgPath;
  stuTdParam.m_ssDataPath = m_ssDataPath;

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
  pTdReqApi->SubscribePublicTopic(THOST_TERT_RESUME);
  pTdReqApi->SubscribePrivateTopic(THOST_TERT_RESUME);

  pTdReqApi->RegisterFront(const_cast<char*>(m_ssTdServerAddr.c_str()));
  pTdReqApi->Init();

  return spTdSpi;
}

SharedPtr<CSgitTdSpi> CSgitContext::CreateTdSpi(const std::string &ssSessionID, CSgitTdSpi::EnTdSpiRole enTdSpiRole)
{
  CSgitTdSpi::STUTdParam stuTdParam;
  stuTdParam.m_ssSessionID = ssSessionID;

  SharedPtr<CSgitTdSpi> spTdSpi = CreateTdSpi(stuTdParam, enTdSpiRole);
  if (!spTdSpi)
  {
    LOG(ERROR_LOG_LEVEL, "Failed to CreateTdSpi for sessionID:%s", stuTdParam.m_ssSessionID.c_str());
    return NULL;
  }

  if(!LinkSessionID2TdSpi(stuTdParam.m_ssSessionID, spTdSpi)) return NULL;

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
  ScopedWriteRWLock scopeLock(m_rwSessionID2TdSpi);

	std::pair<std::map<std::string, SharedPtr<CSgitTdSpi>>::iterator, bool> ret = 
		m_mapSessionID2TdSpi.insert(std::pair<std::string, SharedPtr<CSgitTdSpi>>(ssSessionID, spTdSpi));

	if (ret.second == false)
	{
		LOG(ERROR_LOG_LEVEL, "SessionID:%s is already in map", ssSessionID.c_str());
		return false;
	}

	return true;
}

SharedPtr<CSgitTdSpi> CSgitContext::GetTdSpi(const FIX::SessionID& oSessionID)
{
  ScopedReadRWLock scopeLock(m_rwSessionID2TdSpi);
	std::map<std::string, SharedPtr<CSgitTdSpi>>::const_iterator cit = m_mapSessionID2TdSpi.find(oSessionID.toString());
	if (cit != m_mapSessionID2TdSpi.end())
	{
		return cit->second;
	}

	LOG(ERROR_LOG_LEVEL, "Can not find TdSpi by SessionID:%s", oSessionID.toString().c_str());
	return NULL;
}

SharedPtr<CSgitMdSpi> CSgitContext::GetMdSpi(const FIX::SessionID& oSessionID)
{
  return m_spMdSpi;
}

bool CSgitContext::InitSgitApi()
{
  LOG(INFO_LOG_LEVEL, "TdApi version:%s, MdApi version:%s", 
    CThostFtdcTraderApi::GetApiVersion(), CThostFtdcMdApi::GetApiVersion());

  m_apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);

  CToolkit::GetStrinIfSet(m_apSgitConf, "global.FlowPath", m_ssFlowPath);
	if (!m_ssFlowPath.empty()) FIX::file_mkdir(m_ssFlowPath.c_str());

	m_ssDataPath = m_apSgitConf->getString("global.DataPath");
	FIX::file_mkdir(m_ssDataPath.c_str());

  m_ssTdServerAddr = m_apSgitConf->getString("global.TradeServerAddr");
  std::string ssMdServerAddr = m_apSgitConf->getString("global.QuoteServerAddr");
  std::string ssQuoteAccount = m_apSgitConf->getString("global.QuoteAccount");
  LOG(INFO_LOG_LEVEL, "QuoteAccount:%s", ssQuoteAccount.c_str());
  StringTokenizer stQuoteUserIdPassword(ssQuoteAccount, ":", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  CreateMdSpi(m_ssFlowPath, ssMdServerAddr, stQuoteUserIdPassword[0], stQuoteUserIdPassword[1]);

  std::string ssTradeAccountListKey = "global.TradeAccountList", ssFixSessionProp = "", ssSessionID = "";

  if (!m_apSgitConf->hasProperty(ssTradeAccountListKey)) return true;

  //读取配置文件中需要预先登录的UserID和密码进行登录
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

    SharedPtr<CSgitTdSpi> spTdSpi = CreateTdSpi(m_apSgitConf->getString(ssFixSessionProp), CSgitTdSpi::HubTran);
    if (!spTdSpi)
    {
      LOG(ERROR_LOG_LEVEL, "Failed to CreateTdSpi for SessionID:%s", m_apSgitConf->getString(ssFixSessionProp).c_str());
      return false;
    }

    std::string ssErrMsg = "";
    if(!spTdSpi->ReqUserLogin(stTdUserIdPassword[0], stTdUserIdPassword[1], ssErrMsg))
    {
      LOG(ERROR_LOG_LEVEL, "Failed to ReqUserLogin for UserID:%s,errMsg:%s", stTdUserIdPassword[0].c_str(), ssErrMsg.c_str());
      return false;
    }
    
    LOG(INFO_LOG_LEVEL, "Create trade api instance for TradeID:%s, RegisterFront tradeServerAddr:%s", 
      stTdUserIdPassword[0].c_str(), m_ssTdServerAddr.c_str());
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

std::string CSgitContext::CvtExchange(const std::string &ssExchange, const Convert::EnCvtType enDstType)
{
  return m_oConvert.CvtExchange(ssExchange, enDstType);
}

void CSgitContext::Deal(const FIX::Message& oMsg, const FIX::SessionID& oSessionID)
{
  const FIX::BeginString& beginString = 
    FIELD_GET_REF( oMsg.getHeader(), BeginString);
  if ( beginString != FIX::BeginString_FIX42 ) return;

  FIX::MsgType msgType;
  oMsg.getHeader().getField(msgType);

  if(CToolkit::IsTdRequest(msgType))
  {
		SharedPtr<CSgitTdSpi> spTdSpi = GetTdSpi(oSessionID);
		if (!spTdSpi)
		{
			LOG(ERROR_LOG_LEVEL, "Can not find Tdspi for SessionID:%s", oSessionID.toString().c_str());
			return;
		}
		spTdSpi->OnMessage(oMsg, oSessionID);
  }
  else if(CToolkit::IsMdRequest(msgType))
  {
    SharedPtr<CSgitMdSpi> spMdSpi = GetMdSpi(oSessionID);
		if(!spMdSpi)
		{
			LOG(ERROR_LOG_LEVEL, "Can not find Mdspi for SessionID:%s", oSessionID.toString().c_str());
			return;
		}
		spMdSpi->OnMessage(oMsg, oSessionID);
  }
}

void CSgitContext::AddUserInfo(const std::string &ssSessionKey, SharedPtr<STUserInfo> spStuFixInfo)
{
  ScopedWriteRWLock scopeLock(m_rwFixUser2Info);
  m_mapFixUser2Info[ssSessionKey] = spStuFixInfo;
}

Convert::EnCvtType CSgitContext::GetSymbolType(const std::string &ssSessionKey)
{
  ScopedReadRWLock scopeLock(m_rwFixUser2Info);
  std::map<std::string, SharedPtr<STUserInfo>>::const_iterator citFind = m_mapFixUser2Info.find(ssSessionKey);
  if (citFind != m_mapFixUser2Info.end()) return citFind->second->m_enCvtType;

  return Convert::Unknow;
}

void CSgitContext::UpdateSymbolType(const std::string &ssSessionKey, Convert::EnCvtType enSymbolType)
{
  ScopedWriteRWLock scopeLock(m_rwFixUser2Info);
  std::map<std::string, SharedPtr<STUserInfo>>::iterator itFind = m_mapFixUser2Info.find(ssSessionKey);
  if (itFind != m_mapFixUser2Info.end()) itFind->second->m_enCvtType = enSymbolType;
}

SharedPtr<CSgitTdSpi> CSgitContext::GetOrCreateTdSpi(const FIX::SessionID& oSessionID, CSgitTdSpi::EnTdSpiRole enTdSpiRole)
{
  SharedPtr<CSgitTdSpi> spTdSpi = GetTdSpi(oSessionID);
  if (spTdSpi) return spTdSpi;

  LOG(INFO_LOG_LEVEL, "Prepare to create TdSpi for:%s", oSessionID.toString().c_str());
  return CreateTdSpi(oSessionID.toString(), enTdSpiRole);
}

void CSgitContext::UpsertLoginStatus(const std::string ssSessionID, bool bStatus)
{
  ScopedWriteRWLock scopeLock(m_rwFisSessionID2LoginStatus);
  m_mapFisSessionID2LoginStatus[ssSessionID] = bStatus;
}

bool CSgitContext::GetLoginStatus(const std::string ssSessionID)
{
  ScopedReadRWLock scopeLock(m_rwFisSessionID2LoginStatus);
  std::map<std::string, bool>::const_iterator itFind = m_mapFisSessionID2LoginStatus.find(ssSessionID);
  if (itFind != m_mapFisSessionID2LoginStatus.end()) return itFind->second;

  return false;
}

