#include "SgitMdSpi.h"
#include "SgitContext.h"
#include "Log.h"
#include "quickfix/fix42/MarketDataRequestReject.h"
#include "Toolkit.h"


CSgitMdSpi::CSgitMdSpi(CSgitContext *pSgitCtx, CThostFtdcMdApi *pMdReqApi, const std::string &ssTradeId, const std::string &ssPassword) 
	: m_pSgitCtx(pSgitCtx)
	, m_pMdReqApi(pMdReqApi)
  , m_acRequestId(0)
{
  memset(&m_stuLogin, 0, sizeof(CThostFtdcReqUserLoginField));
  strncpy(m_stuLogin.UserID, ssTradeId.c_str(), sizeof(m_stuLogin.UserID));
  strncpy(m_stuLogin.Password, ssPassword.c_str(), sizeof(m_stuLogin.Password));

  m_mapCode2ScrbParmSet.clear();
  //m_setSubAllCodeSession.clear();
  m_mapSnapshot.clear();
}

CSgitMdSpi::~CSgitMdSpi()
{

}

bool CSgitMdSpi::MarketDataRequest(const FIX42::MarketDataRequest& oMarketDataRequest, std::string& ssErrMsg)
{
  FIX::MDReqID mdReqID;
  FIX::SubscriptionRequestType subscriptionRequestType;
  FIX::MarketDepth marketDepth;

  //价格类型个数
  FIX::NoMDEntryTypes noMdEntryTypes;
  //价格类型组
  FIX42::MarketDataRequest::NoMDEntryTypes entryGroup;
  FIX::MDEntryType mdEntryType;

  //代码个数
  FIX::NoRelatedSym noRelatedSym;
  //代码组
  FIX42::MarketDataRequest::NoRelatedSym symGroup;
  FIX::Symbol symbol;

  oMarketDataRequest.get(mdReqID);


  oMarketDataRequest.get(subscriptionRequestType);
  oMarketDataRequest.get(marketDepth);
  oMarketDataRequest.get(noMdEntryTypes);
  oMarketDataRequest.get(noRelatedSym);

  STUScrbParm stuScrbParm;
  stuScrbParm.m_ssSessionKey = CToolkit::GetSessionKey(oMarketDataRequest);
  stuScrbParm.m_iDepth = marketDepth.getValue();

  //价格类型
  int iEntryTypeCount = noMdEntryTypes.getValue();
  for (int i = 0; i < iEntryTypeCount; i++)
  {
    oMarketDataRequest.getGroup(i + 1, entryGroup);
    entryGroup.get(mdEntryType);
    LOG(DEBUG_LOG_LEVEL, "entryType:%c", mdEntryType.getValue());
    stuScrbParm.m_setEntryTypes.insert(mdEntryType.getValue());
  }

  //代码
  int iSymCount = noRelatedSym.getValue();
  std::set<std::string> symbolSet;
  for (int i = 0; i < iSymCount; i++)
  {
    oMarketDataRequest.getGroup(i + 1, symGroup);
    symGroup.get(symbol);
    LOG(DEBUG_LOG_LEVEL, "symbol:%s", symbol.getValue().c_str());
    symbolSet.insert(symbol.getValue());
  }

  char chRejReason;
  if(!CheckValid(symbolSet, mdReqID.getValue(), stuScrbParm, chRejReason, ssErrMsg))
  {

  }

	
  switch(subscriptionRequestType.getValue())
  {
  case FIX::SubscriptionRequestType_SNAPSHOT:
    return SendMarketDataSet(oMarketDataRequest, symbolSet, stuScrbParm, ssErrMsg);
    break;
  case FIX::SubscriptionRequestType_SNAPSHOT_PLUS_UPDATES:
    if(!SendMarketDataSet(oMarketDataRequest, symbolSet, stuScrbParm, ssErrMsg)) return false;

    AddSub(symbolSet, stuScrbParm);
    break;
  case FIX::SubscriptionRequestType_DISABLE_PREVIOUS_SNAPSHOT_PLUS_UPDATE_REQUEST:
    DelSub(symbolSet, stuScrbParm);
    break;
  default:
    Poco::format(ssErrMsg, "unsupported subscriptionRequestType:%c", subscriptionRequestType.getValue());
    return false;
  }

  return true;
}

void CSgitMdSpi::OnFrontConnected()
{
	m_pMdReqApi->ReqUserLogin(&m_stuLogin, m_acRequestId++);

	LOG(INFO_LOG_LEVEL, "ReqUserLogin userID:%s", m_stuLogin.UserID);
}

void CSgitMdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  if (!pRspUserLogin || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "UserID:%s,ErrorID:%d,ErrorMsg:%s,RequestID:%d", pRspUserLogin->UserID, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
  if (pRspInfo->ErrorID != 0) return;

  //登录成功后订阅全市场行情
  char* ppInstruments[1];
  ppInstruments[0] = (char *)"all";
  m_pMdReqApi->SubscribeMarketData(ppInstruments, 1);
}

void CSgitMdSpi::OnFrontDisconnected(int nReason)
{
  LOG(WARN_LOG_LEVEL, "Reason:%d", nReason);
}

void CSgitMdSpi::OnHeartBeatWarning(int nTimeLapse)
{
  LOG(WARN_LOG_LEVEL, "TimeLapse:%d", nTimeLapse);
}

void CSgitMdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  if(!pRspInfo) return;
  LOG(ERROR_LOG_LEVEL, "ErrorID:%d,ErrorMsg:%s,RequestID:%d", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
}

void CSgitMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  if (!pSpecificInstrument || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "InstrumentID:%s,ErrorID:%d,ErrorMsg:%s,RequestID:%d", 
    pSpecificInstrument->InstrumentID, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
}

void CSgitMdSpi::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  if(!pSpecificInstrument || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "InstrumentID:%s,ErrorID:%d,ErrorMsg:%s,RequestID:%d", 
    pSpecificInstrument->InstrumentID, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
}

void CSgitMdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
  if(!pDepthMarketData) return;
  if (feq(pDepthMarketData->LastPrice, DBL_MAX)) return;

  //LOG(DEBUG_LOG_LEVEL, "InstrumentID:%s,Price:%lf", pDepthMarketData->InstrumentID, pDepthMarketData->LastPrice);
  do 
  {
    ScopedWriteRWLock scopeLock(m_rwLockSnapShot);
    m_mapSnapshot[pDepthMarketData->InstrumentID] = *pDepthMarketData;
  } while (0);

  //推送给感兴趣的订阅方
  PubMarketData(*pDepthMarketData);
}

bool CSgitMdSpi::SendMarketDataSet(const FIX42::MarketDataRequest& oMarketDataRequest, const std::set<std::string> &symbolSet, const STUScrbParm &stuScrbParm, std::string &ssErrMsg)
{
  FIX::MDReqID mdReqID;
  oMarketDataRequest.get(mdReqID);

  CThostFtdcDepthMarketDataField stuMarketData;
  for(std::set<std::string>::const_iterator citSymbol = symbolSet.begin(); citSymbol != symbolSet.end(); citSymbol++)
  {
    if (!GetMarketData(*citSymbol, stuMarketData))
    {
      Poco::format(ssErrMsg, "Can not find symbol:%s in current market data cache", *citSymbol);
      LOG(WARN_LOG_LEVEL, ssErrMsg.c_str());
      return false;
    }

    FIX42::MarketDataSnapshotFullRefresh oMdSnapShot = CreateSnapShot(stuMarketData, stuScrbParm, mdReqID.getValue());
    if(!CToolkit::Send(oMarketDataRequest, oMdSnapShot))
    {
      ssErrMsg = "Failed to send message to target";
      return false;
    }
  }

  return true;
}

void CSgitMdSpi::AddPrice(FIX42::MarketDataSnapshotFullRefresh &oMdSnapShot, char chEntryType, double dPrice, int iVolume /*= 0*/, int iPos /*= 0*/)
{
  if (feq(dPrice, DBL_MAX)) return;

  FIX42::MarketDataSnapshotFullRefresh::NoMDEntries noMdEntriesGroup = FIX42::MarketDataSnapshotFullRefresh::NoMDEntries();
  noMdEntriesGroup.setField(FIX::MDEntryType(chEntryType));
  noMdEntriesGroup.setField(FIX::MDEntryPx(dPrice));
  if(iVolume > 0) noMdEntriesGroup.setField(FIX::MDEntrySize(iVolume));
  if(iPos > 0) noMdEntriesGroup.setField(FIX::MDEntryPositionNo(iPos));
  oMdSnapShot.addGroup(noMdEntriesGroup);
}

void CSgitMdSpi::AddSub(const std::set<std::string> &symbolSet, const STUScrbParm &stuScrbParm)
{
  ScopedWriteRWLock scopeLock(m_rwLockCode2ScrbParmSet);
  std::string ssOriginalSymbol = "";
  for (std::set<std::string>::const_iterator citSymbol = symbolSet.begin(); citSymbol != symbolSet.end(); citSymbol++)
  {
    ssOriginalSymbol = m_pSgitCtx->CvtSymbol(*citSymbol, Convert::Original);
    if (m_mapCode2ScrbParmSet.count(ssOriginalSymbol) < 1)
    {
      std::set<STUScrbParm> stuScrbParmSet;
      stuScrbParmSet.insert(stuScrbParm);
      m_mapCode2ScrbParmSet[ssOriginalSymbol] = stuScrbParmSet;
    }
    else
    {
      m_mapCode2ScrbParmSet[ssOriginalSymbol].insert(stuScrbParm);
    }
  }
}

void CSgitMdSpi::DelSub(const std::set<std::string> &symbolSet, const STUScrbParm &stuScrbParm)
{
  ScopedWriteRWLock scopeLock(m_rwLockCode2ScrbParmSet);
  std::string ssOriginalSymbol = "";
  for (std::set<std::string>::const_iterator citSymbol = symbolSet.begin(); citSymbol != symbolSet.end(); citSymbol++)
  {
    ssOriginalSymbol = m_pSgitCtx->CvtSymbol(*citSymbol, Convert::Original);
    if (m_mapCode2ScrbParmSet.count(ssOriginalSymbol) < 1) continue;

    std::set<STUScrbParm>::iterator itStuScrbParm = m_mapCode2ScrbParmSet[ssOriginalSymbol].find(stuScrbParm);
    if (itStuScrbParm == m_mapCode2ScrbParmSet[ssOriginalSymbol].end()) continue;

    m_mapCode2ScrbParmSet[ssOriginalSymbol].erase(itStuScrbParm);
  }
}

bool CSgitMdSpi::CheckValid(
  const std::set<std::string> &symbolSet, 
  const std::string &ssMDReqID, const STUScrbParm &stuScrbParm, char &chRejReason, const std::string &ssErrMsg)
{
  /* do
  {
  Poco::FastMutex::ScopedLock oScopedLock(m_fastmutexLockMDReqID);

  std::map<std::string, std::set<std::string> >::iterator it = m_mapMDReqID.find(stuScrbParm.m_ssSessionKey);
  if(it == m_mapMDReqID.end())
  {
  std::set<std::string> setMdReqID;
  setMdReqID.insert(ssMDReqID);
  m_mapMDReqID[stuScrbParm.m_ssSessionKey] = setMdReqID;
  }
  else
  {
  if(it->second.count(ssMDReqID) > 0)
  {
  chRejReason = FIX::MDReqRejReason_DUPLICATE_MDREQID;
  Poco::format(ssErrMsg, "MDReqID:%s is duplicate", ssMDReqID);
  return false;
  }

  it->second.insert(ssMDReqID);
  }
  }while(0);*/

  //if (stuScrbParm.m_iDepth > 5)
  //{
  //  chRejReason = FIX::MDReqRejReason_UNSUPPORTED_MARKETDEPTH;
  //  Poco::format(ssErrMsg, "market depth no more than 5, receive:%d", stuScrbParm.m_iDepth);
  //  return false;
  //}

 /* for(std::set<char>::const_iterator citEntryType = stuScrbParm.m_setEntryTypes.begin(); 
    citEntryType != stuScrbParm.m_setEntryTypes.end(); 
    citEntryType++)
  {
    if (!(*citEntryType == FIX::MDEntryType_BID || *citEntryType == FIX::MDEntryType_OFFER || *citEntryType == FIX::MDEntryType_TRADE || 
      *citEntryType == FIX::MDEntryType_OPENING_PRICE || *citEntryType == FIX::MDEntryType_CLOSING_PRICE || 
      *citEntryType == FIX::MDEntryType_SETTLEMENT_PRICE || *citEntryType == FIX::MDEntryType_SESSION_HIGH_BID ||
      *citEntryType == FIX::MDEntryType_SESSION_LOW_OFFER))
    {
      chRejReason = FIX::MDReqRejReason_UNSUPPORTED_MDENTRYTYPE;
      Poco::format(ssErrMsg, "MDEntryType:%c is unsupported", *citEntryType);
      return false;
    }
  }

  for(std::set<std::string>::const_iterator citSymbol = symbolSet.begin(); citSymbol != symbolSet.end(); citSymbol++)
  {
    ScopedReadRWLock scopeLock(m_rwLockSnapShot);

    if (m_mapSnapshot.count(*citSymbol) < 1)
    {
      chRejReason = FIX::MDReqRejReason_UNKNOWN_SYMBOL;
      Poco::format(ssErrMsg, "Can not find symbol:%s in current market data cache", *citSymbol);
      return false;
    }
  }*/

  return true;
}

bool CSgitMdSpi::OnMessage(const FIX::Message& oMsg, const FIX::SessionID& oSessionID, std::string& ssErrMsg)
{
  AddFixInfo(oMsg);

  FIX::MsgType msgType;
  oMsg.getHeader().getField(msgType);

  if (msgType == FIX::MsgType_MarketDataRequest)
  {
    const FIX42::MarketDataRequest& oMarketDataRequest = (const FIX42::MarketDataRequest&) oMsg;
    if(!MarketDataRequest(oMarketDataRequest, ssErrMsg))
    {
      FIX::MDReqID mdReqID;
      oMarketDataRequest.get(mdReqID);

      FIX42::MarketDataRequestReject marketDataRequestReject = FIX42::MarketDataRequestReject(FIX::MDReqID(mdReqID));
      marketDataRequestReject.set(FIX::MDReqRejReason());

      return true;
    }
  }

  ssErrMsg = "unsupported message type";
  return false;
}

void CSgitMdSpi::AddFixInfo(const FIX::Message& oMsg)
{

}

bool CSgitMdSpi::GetMarketData(const std::string ssSymbol, CThostFtdcDepthMarketDataField &stuMarketData)
{
  ScopedReadRWLock scopeLock(m_rwLockSnapShot);

  if (m_mapSnapshot.count(ssSymbol) < 1) return false;
  stuMarketData = m_mapSnapshot[ssSymbol];
  return true;
}

FIX42::MarketDataSnapshotFullRefresh CSgitMdSpi::CreateSnapShot(const CThostFtdcDepthMarketDataField &stuMarketData, const STUScrbParm &stuScrbParm, const std::string &ssMDReqID /*= ""*/)
{
  FIX42::MarketDataSnapshotFullRefresh oMdSnapShot = FIX42::MarketDataSnapshotFullRefresh();
  if(!ssMDReqID.empty()) oMdSnapShot.setField(FIX::MDReqID(ssMDReqID));
  
  Convert::EnCvtType enSymbolType = m_pSgitCtx->GetSymbolType(stuScrbParm.m_ssSessionKey);

  oMdSnapShot.setField(FIX::Symbol(enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
    stuMarketData.InstrumentID : m_pSgitCtx->CvtSymbol(stuMarketData.InstrumentID, enSymbolType)));

  std::string ssExchange = enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
    stuMarketData.ExchangeID : m_pSgitCtx->CvtExchange(stuMarketData.ExchangeID, enSymbolType);
  oMdSnapShot.setField(FIX::SecurityExchange(ssExchange));

  for(std::set<char>::const_iterator cit = stuScrbParm.m_setEntryTypes.begin(); cit != stuScrbParm.m_setEntryTypes.end(); cit++)
  {
    if (*cit == FIX::MDEntryType_BID || *cit == FIX::MDEntryType_OFFER)
    {
      if(stuScrbParm.m_iDepth == 0 || stuScrbParm.m_iDepth >= 1)
        AddPrice(oMdSnapShot, *cit, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidPrice1 : stuMarketData.AskPrice1, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidVolume1 : stuMarketData.AskVolume1, 1);
      
      if(stuScrbParm.m_iDepth == 0 || stuScrbParm.m_iDepth >= 2)
        AddPrice(oMdSnapShot, *cit, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidPrice2 : stuMarketData.AskPrice2, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidVolume2 : stuMarketData.AskVolume2, 2);

      if(stuScrbParm.m_iDepth == 0 || stuScrbParm.m_iDepth >= 3)
        AddPrice(oMdSnapShot, *cit, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidPrice3 : stuMarketData.AskPrice3, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidVolume3 : stuMarketData.AskVolume3, 3);

      if(stuScrbParm.m_iDepth == 0 || stuScrbParm.m_iDepth >= 4)
        AddPrice(oMdSnapShot, *cit, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidPrice4 : stuMarketData.AskPrice4, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidVolume4 : stuMarketData.AskVolume4, 4);

      if(stuScrbParm.m_iDepth == 0 || stuScrbParm.m_iDepth >= 5)
        AddPrice(oMdSnapShot, *cit, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidPrice5 : stuMarketData.AskPrice5, 
          *cit == FIX::MDEntryType_BID ? stuMarketData.BidVolume5 : stuMarketData.AskVolume5, 5);
    }
    else if(*cit == FIX::MDEntryType_TRADE)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_TRADE, stuMarketData.LastPrice, stuMarketData.Volume);
    }
    else if(*cit == FIX::MDEntryType_OPENING_PRICE)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_OPENING_PRICE, stuMarketData.OpenPrice);
    }
    else if(*cit == FIX::MDEntryType_CLOSING_PRICE)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_CLOSING_PRICE, stuMarketData.PreClosePrice);
    }
    else if(*cit == FIX::MDEntryType_SETTLEMENT_PRICE)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_SETTLEMENT_PRICE, stuMarketData.SettlementPrice);
    }
    else if(*cit == FIX::MDEntryType_TRADING_SESSION_HIGH_PRICE)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_TRADING_SESSION_HIGH_PRICE, stuMarketData.HighestPrice);
    }
    else if(*cit == FIX::MDEntryType_TRADING_SESSION_LOW_PRICE)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_TRADING_SESSION_LOW_PRICE, stuMarketData.LowestPrice);
    }
  }

  return oMdSnapShot;
}

void CSgitMdSpi::PubMarketData(const CThostFtdcDepthMarketDataField &stuDepthMarketData)
{
  do 
  {
    ScopedReadRWLock scopeLock(m_rwLockCode2ScrbParmSet);
    if (m_mapCode2ScrbParmSet.count(stuDepthMarketData.InstrumentID) < 1) return;

    for (std::set<STUScrbParm>::const_iterator citStuScrbParm = m_mapCode2ScrbParmSet[stuDepthMarketData.InstrumentID].begin();
      citStuScrbParm != m_mapCode2ScrbParmSet[stuDepthMarketData.InstrumentID].end();
      citStuScrbParm++)
    {
      FIX42::MarketDataSnapshotFullRefresh oMktDataSnapshot = CreateSnapShot(stuDepthMarketData, *citStuScrbParm);
      Send(citStuScrbParm->m_ssSessionKey, oMktDataSnapshot);
    }

  } while (0);
}

void CSgitMdSpi::Send(const std::string &ssSessionKey, FIX42::MarketDataSnapshotFullRefresh oMdSnapShot)
{
  FIX::SessionID oSessionID;
  std::string ssOnBehalfCompID;
  CToolkit::SessionKey2SessionIDBehalfCompID(ssSessionKey, oSessionID, ssOnBehalfCompID);

  if (m_pSgitCtx->GetLoginStatus(oSessionID.toString()))
  {
    CToolkit::Send(oMdSnapShot, oSessionID, ssOnBehalfCompID);
  }
}



