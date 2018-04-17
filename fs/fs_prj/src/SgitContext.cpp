#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#else
#include "config.h"
#endif

#include "SgitContext.h"
#include "Log.h"
#include "Toolkit.h"

#include "Poco/StringTokenizer.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/File.h"


#include "quickfix/Session.h"
#include "quickfix/SessionID.h"

#include "Order.h"


CSgitContext::CSgitContext(const std::string &ssSgitCfgPath, const std::string &ssCvtCfgPath)
  : m_ssSgitCfgPath(ssSgitCfgPath)
  , m_oConvert(ssCvtCfgPath)
  , m_ssCvtCfgPath(ssCvtCfgPath)
  , m_bQuoteSupported(false)
  , m_bTradeSupported(false)
  , m_spSQLiteSession(NULL)
  , m_ssTradingDay("")
{
  SQLite::Connector::registerConnector();
}

CSgitContext::~CSgitContext()
{
  SQLite::Connector::unregisterConnector();
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

    if(!InitSgit()) return false;
  }
  catch ( std::exception & e)
  {
  	LOG(FATAL_LOG_LEVEL, "%s", e.what());
    return false;
  }

  return true;
}

SharedPtr<CSgitTdSpi> CSgitContext::CreateTdSpi(STUTdParam &stuTdParam, EnTdSpiRole enTdSpiRole)
{
  CThostFtdcTraderApi *pTdReqApi = CThostFtdcTraderApi::CreateFtdcTraderApi(m_ssFlowPath.c_str());
  stuTdParam.m_pSgitCtx = this;
  stuTdParam.m_pTdReqApi = pTdReqApi;
  stuTdParam.m_ssSgitCfgPath = m_ssSgitCfgPath;
  stuTdParam.m_ssDataPath = m_ssDataPath;
  //对于网关预先登录的账号，这时数据库会话还是空指针（需要账户登录后才可以获得交易日期信息）
  stuTdParam.m_spSQLiteSession = m_spSQLiteSession;

  SharedPtr<CSgitTdSpi> spTdSpi = NULL;
  if (enTdSpiRole == HubTran)
  {
    spTdSpi = new CSgitTdSpiHubTran(stuTdParam);
  }
  else
  {
    spTdSpi = new CSgitTdSpiDirect(stuTdParam);
  }

  if(!spTdSpi->Init()) return NULL;

  pTdReqApi->IsReviveNtyCapital(false);
  pTdReqApi->RegisterSpi(spTdSpi);
  pTdReqApi->SubscribePublicTopic(THOST_TERT_RESUME);
  pTdReqApi->SubscribePrivateTopic(THOST_TERT_RESUME);

  pTdReqApi->RegisterFront(const_cast<char*>(m_ssTdServerAddr.c_str()));
  pTdReqApi->Init();

  return spTdSpi;
}

SharedPtr<CSgitTdSpi> CSgitContext::CreateTdSpi(const std::string &ssSessionID, EnTdSpiRole enTdSpiRole)
{
  STUTdParam stuTdParam;
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

	std::pair<std::map<std::string, SharedPtr<CSgitTdSpi> >::iterator, bool> ret = 
		m_mapSessionID2TdSpi.insert(std::pair<std::string, SharedPtr<CSgitTdSpi> >(ssSessionID, spTdSpi));

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
	std::map<std::string, SharedPtr<CSgitTdSpi> >::const_iterator cit = m_mapSessionID2TdSpi.find(oSessionID.toString());
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

bool CSgitContext::InitSgit()
{
  m_apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);

  LOG(INFO_LOG_LEVEL, "TdApi version:%s, MdApi version:%s", 
    CThostFtdcTraderApi::GetApiVersion(), CThostFtdcMdApi::GetApiVersion());

  CToolkit::GetStrinIfSet(m_apSgitConf, "global.FlowPath", m_ssFlowPath);
	if (!m_ssFlowPath.empty()) FIX::file_mkdir(m_ssFlowPath.c_str());

  if(!CToolkit::GetString(m_apSgitConf, "global.DataPath", m_ssDataPath)) return false;
	FIX::file_mkdir(m_ssDataPath.c_str());

  if (!InitSgitQuote()) return false;
  if (!InitSgitTrade()) return false;


  return true;
}

bool CSgitContext::InitConvert()
{
  return m_oConvert.Init();
}

char CSgitContext::CvtDict(const int iField, const char cFrom, const Convert::EnDictType enDstDictType)
{
  return m_oConvert.CvtDict(iField, cFrom, enDstDictType);
}

std::string CSgitContext::CvtDict(const int iField, const std::string &ssFrom, const Convert::EnDictType enDstDictType)
{
  return m_oConvert.CvtDict(iField, ssFrom, enDstDictType);
}

std::string CSgitContext::CvtSymbol(const std::string &ssSymbol, const Convert::EnCvtType enDstType)
{
  return m_oConvert.CvtSymbol(ssSymbol, enDstType);
}

std::string CSgitContext::CvtExchange(const std::string &ssExchange, const Convert::EnCvtType enDstType)
{
  return m_oConvert.CvtExchange(ssExchange, enDstType);
}

bool CSgitContext::Deal(const FIX::Message& oMsg, const FIX::SessionID& oSessionID, std::string& ssErrMsg)
{
  const FIX::BeginString& beginString = 
    FIELD_GET_REF( oMsg.getHeader(), BeginString);
  if ( beginString != FIX::BeginString_FIX42 )
  {
    ssErrMsg = "only support fix42";
    return false;
  }

  FIX::MsgType msgType;
  oMsg.getHeader().getField(msgType);

  if(CToolkit::IsTdRequest(msgType))
  {
    if(!IsTradeSupported())
    {
      ssErrMsg = "Trade request is not supported on this fix gateway";
      return false;
    }

		SharedPtr<CSgitTdSpi> spTdSpi = GetTdSpi(oSessionID);
		if (!spTdSpi)
		{
      ssErrMsg = "Can not find Tdspi for SessionID:" + oSessionID.toString();
			LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
			return false;
		}
		return spTdSpi->OnMessage(oMsg, oSessionID, ssErrMsg);
  }
  else if(CToolkit::IsMdRequest(msgType))
  {
    if(!IsQuoteSupported())
    {
      ssErrMsg = "Quote request is not supported on this fix gateway";
      return false;
    }

    SharedPtr<CSgitMdSpi> spMdSpi = GetMdSpi(oSessionID);
		if(!spMdSpi)
		{
      ssErrMsg = "Can not find Mdspi for SessionID:" + oSessionID.toString();
			LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
			return false;
		}
		return spMdSpi->OnMessage(oMsg, oSessionID, ssErrMsg);
  }

  ssErrMsg = "unsupported message type";
  return false;
}

void CSgitContext::AddUserInfo(const std::string &ssSessionKey, SharedPtr<STUserInfo> spStuFixInfo)
{
  ScopedWriteRWLock scopeLock(m_rwFixUser2Info);
  m_mapFixUser2Info[ssSessionKey] = spStuFixInfo;
}

Convert::EnCvtType CSgitContext::GetSymbolType(const std::string &ssSessionKey)
{
  ScopedReadRWLock scopeLock(m_rwFixUser2Info);
  std::map<std::string, SharedPtr<STUserInfo> >::const_iterator citFind = m_mapFixUser2Info.find(ssSessionKey);
  if (citFind != m_mapFixUser2Info.end()) return citFind->second->m_enCvtType;

  return Convert::Unknow;
}

void CSgitContext::UpdateSymbolType(const std::string &ssSessionKey, Convert::EnCvtType enSymbolType)
{
  ScopedWriteRWLock scopeLock(m_rwFixUser2Info);
  std::map<std::string, SharedPtr<STUserInfo> >::iterator itFind = m_mapFixUser2Info.find(ssSessionKey);
  if (itFind != m_mapFixUser2Info.end()) itFind->second->m_enCvtType = enSymbolType;
}

SharedPtr<CSgitTdSpi> CSgitContext::GetOrCreateTdSpi(const FIX::SessionID& oSessionID, EnTdSpiRole enTdSpiRole)
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

bool CSgitContext::InitSgitQuote()
{
  if(!NeedRun("quote")) return true;

  m_bQuoteSupported = true;
  std::string ssServerAddr = "", ssAccount = "";
  if(!CToolkit::GetString(m_apSgitConf, "quote.ServerAddr", ssServerAddr)) return false;
  if(!CToolkit::GetString(m_apSgitConf, "quote.Account", ssAccount)) return false;
  LOG(INFO_LOG_LEVEL, "QuoteAccount:%s", ssAccount.c_str());
  StringTokenizer stQuoteUserIdPassword(ssAccount, ":", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  CreateMdSpi(m_ssFlowPath, ssServerAddr, stQuoteUserIdPassword[0], stQuoteUserIdPassword[1]);

  return true;
}

bool CSgitContext::InitSgitTrade()
{
  if(!NeedRun("trade")) return true;

  m_bTradeSupported = true;
  if(!CToolkit::GetString(m_apSgitConf, "trade.ServerAddr", m_ssTdServerAddr)) return false;

  std::string ssTradeAccountListKey = "trade.AccountList", ssFixSession = "", ssSessionID = "";
  //没有配置TradeAccountList，说明不需要预先登录。正常返回
  if (!m_apSgitConf->hasProperty(ssTradeAccountListKey)) return true;

  //读取配置文件中需要预先登录的UserID和密码进行登录
  StringTokenizer stTradeAccountList(m_apSgitConf->getString(ssTradeAccountListKey), ";", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  for (StringTokenizer::Iterator it = stTradeAccountList.begin(); it != stTradeAccountList.end(); it++)
  {
    StringTokenizer stTdUserIdPassword(*it, ":", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

    if(!CToolkit::GetString(m_apSgitConf, "trade." + stTdUserIdPassword[0], ssFixSession)) return false;

    SharedPtr<CSgitTdSpi> spTdSpi = CreateTdSpi(ssFixSession, HubTran);
    if (!spTdSpi) return false;


    std::string ssErrMsg = "";
    if(!spTdSpi->ReqUserLogin(stTdUserIdPassword[0], stTdUserIdPassword[1], ssErrMsg))
    {
      LOG(ERROR_LOG_LEVEL, "Failed to ReqUserLogin for UserID:%s,errMsg:%s", stTdUserIdPassword[0].c_str(), ssErrMsg.c_str());
      return false;
    }

    LOG(INFO_LOG_LEVEL, "Create trade api instance for TradeID:%s, RegisterFront tradeServerAddr:%s", 
      stTdUserIdPassword[0].c_str(), m_ssTdServerAddr.c_str());

    if(m_ssTradingDay.empty())
    {
      m_ssTradingDay = spTdSpi->GetTradingDay();
      LOG(INFO_LOG_LEVEL, "TradingDay is %s", m_ssTradingDay.c_str());
    }
  }

  if(!InitSQLConnect()) return false;

  //在这里批量重置一下数据库会话。对于这种需要网关预先登录的账号需要这样操作，后续就不需要了。因为上面的数据会话已经正确创建了
  for(std::map<std::string, SharedPtr<CSgitTdSpi> >::iterator it = m_mapSessionID2TdSpi.begin(); 
    it != m_mapSessionID2TdSpi.end(); it++)
  {
    if (it->second) it->second->SetSQLiteSession(m_spSQLiteSession);
  }

  return true;
}

bool CSgitContext::NeedRun(const std::string ssSectionName)
{
  std::string ssEnableProp = ssSectionName + ".enable";

  if(!m_apSgitConf->hasProperty(ssEnableProp))
  {
    LOG(INFO_LOG_LEVEL, "Can not find property:[%s] in config file, so [%s] will not run", ssEnableProp.c_str(), ssSectionName.c_str());
    return false;
  }

  if(!m_apSgitConf->getBool(ssEnableProp))
  {
    LOG(INFO_LOG_LEVEL, "The property:[%s] in config file is not enabled, so [%s] will not run", ssEnableProp.c_str(), ssSectionName.c_str());
    return false;
  }

  return true;
}

bool CSgitContext::IsQuoteSupported()
{
  return m_bQuoteSupported;
}

bool CSgitContext::IsTradeSupported()
{
  return m_bTradeSupported;
}

bool CSgitContext::InitSQLConnect()
{
  if(!CToolkit::GetString(m_apSgitConf, "global.DataPath", m_ssDataPath)) return false;
  FIX::file_mkdir(m_ssDataPath.c_str());

  try
  {
    std::string ssDbPath = Poco::format("%sfs%s.db", m_ssDataPath, m_ssTradingDay);
    std::string ssCreateNewDBEveryTradingDayProp = "trade.CreateNewDBEveryTradingDay";

    //默认按交易日创建sqlite数据库文件，除非显式设为0，才只生成一个fs.db文件
    if (m_apSgitConf->hasProperty(ssCreateNewDBEveryTradingDayProp) && !m_apSgitConf->getBool(ssCreateNewDBEveryTradingDayProp))
    {
      ssDbPath = m_ssDataPath + "fs.db";
    }
    m_spSQLiteSession = new Session(SQLite::Connector::KEY, ssDbPath);

    if(!m_spSQLiteSession->isConnected())
    {
      LOG(ERROR_LOG_LEVEL, "Failed to connect to sqlite:%s", ssDbPath.c_str());
      return false;
    }

    LOG(INFO_LOG_LEVEL, "SQLite DbPath:%s", ssDbPath);

    *m_spSQLiteSession << 
      "CREATE TABLE IF NOT EXISTS [Order] ( \
      userID        TEXT, \
      clOrdID       TEXT, \
      orderRef      TEXT, \
      acctRecv      TEXT, \
      acctReal      TEXT, \
      recvSymbol    TEXT, \
      orderQty      INTEGER, \
      ordType       INTEGER, \
      side          INTEGER, \
      openClose     INTEGER, \
      price         REAL, \
      orderStatus   CHAR, \
      orderSysID    TEXT, \
      leavesQty     INTEGER, \
      cumQty        INTEGER, \
      cancelClOrdID TEXT, \
      tradingDay    TEXT, \
      time          TEXT, \
      PRIMARY KEY ( \
      userID, \
      clOrdID \
      ) \
      ON CONFLICT ROLLBACK, \
      UNIQUE ( \
      userID, \
      orderRef \
      ) \
      ON CONFLICT ROLLBACK \
      );", now;

    *m_spSQLiteSession << 
      "CREATE TABLE IF NOT EXISTS [Trade] ( \
      tradeID    TEXT, \
      tradingDay TEXT, \
      matchTime  TEXT, \
      orderSysID TEXT, \
      matchPrice REAL, \
      matchQty   INTEGER, \
      userID     TEXT, \
      orderRef   TEXT, \
      acctReal   TEXT, \
      PRIMARY KEY ( \
      tradeID, \
      tradingDay \
      ) \
      ON CONFLICT ROLLBACK \
      );", now;
  }

  catch (Poco::Exception &e)
  {
    LOG(ERROR_LOG_LEVEL, e.displayText().c_str());
    return false;
  }
  
  return true;
}

