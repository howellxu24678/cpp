#include "SgitMdSpi.h"
#include "SgitContext.h"
#include "Log.h"
#include "quickfix/fix42/MarketDataRequestReject.h"
#include "quickfix/fix42/MarketDataSnapshotFullRefresh.h"
#include "quickfix/fix42/MarketDataIncrementalRefresh.h"
#include "Toolkit.h"


CSgitMdSpi::CSgitMdSpi(CSgitContext *pSgitCtx, CThostFtdcMdApi *pMdReqApi, const std::string &ssTradeId, const std::string &ssPassword) 
	: m_pSgitCtx(pSgitCtx)
	, m_pMdReqApi(pMdReqApi)
  , m_acRequestId(0)
{
  memset(&m_stuLogin, 0, sizeof(CThostFtdcReqUserLoginField));
  strncpy(m_stuLogin.UserID, ssTradeId.c_str(), sizeof(m_stuLogin.UserID));
  strncpy(m_stuLogin.Password, ssPassword.c_str(), sizeof(m_stuLogin.Password));

  m_mapCode2SubSession.clear();
  m_setSubAllCodeSession.clear();
  m_mapSnapshot.clear();
}

CSgitMdSpi::~CSgitMdSpi()
{

}

void CSgitMdSpi::MarketDataRequest(const FIX42::MarketDataRequest& oMarketDataRequest)
{
  FIX::MDReqID mdReqID;
  FIX::SubscriptionRequestType subscriptionRequestType;
  //代码个数
  FIX::NoRelatedSym noRelatedSym;
  //代码组
  FIX42::MarketDataRequest::NoRelatedSym symGroup;
  FIX::Symbol symbol;

  oMarketDataRequest.get(mdReqID);
  oMarketDataRequest.get(subscriptionRequestType);
  oMarketDataRequest.get(noRelatedSym);

  int iSymCount = noRelatedSym.getValue();
  std::set<std::string> symbolSet;
  for (int i = 0; i < iSymCount; i++)
  {
    oMarketDataRequest.getGroup(i + 1, symGroup);
    symGroup.get(symbol);
    LOG(DEBUG_LOG_LEVEL, "symbol:%s", symbol.getValue().c_str());
    symbolSet.insert(symbol.getValue());
  }

  CheckValid(symbolSet, mdReqID.getValue(), oMarketDataRequest.getSessionID().toString(), CToolkit::GetSessionKey(oMarketDataRequest));
	
  switch(subscriptionRequestType.getValue())
  {
  case FIX::SubscriptionRequestType_SNAPSHOT:
    SendSnapShot(oMarketDataRequest, symbolSet);
    break;
  case FIX::SubscriptionRequestType_SNAPSHOT_PLUS_UPDATES:
    SendSnapShot(oMarketDataRequest, symbolSet);
    AddSub(symbolSet, oMarketDataRequest.getSessionID().toString());
    break;
  case FIX::SubscriptionRequestType_DISABLE_PREVIOUS_SNAPSHOT_PLUS_UPDATE_REQUEST:
    break;
  default:
    break;
  }
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
  ppInstruments[0] = "all";
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
    ScopedWriteRWLock scopeWriteLock(m_rwLockSnapShot);
    m_mapSnapshot[pDepthMarketData->InstrumentID] = *pDepthMarketData;
  } while (0);

  //推送给感兴趣的订阅方

}

void CSgitMdSpi::SendSnapShot(const FIX42::MarketDataRequest& oMarketDataRequest, const std::set<std::string> &symbolSet)
{
  FIX::MDReqID mdReqID;
  oMarketDataRequest.get(mdReqID);

  CThostFtdcDepthMarketDataField stuMarketData;
  for(std::set<std::string>::const_iterator cit = symbolSet.begin(); cit != symbolSet.end(); cit++)
  {
    if (!GetMarketData(*cit, stuMarketData)) continue;
    
    FIX42::MarketDataSnapshotFullRefresh MdSnapShot = FIX42::MarketDataSnapshotFullRefresh();
    MdSnapShot.setField(mdReqID);

  }



}

void CSgitMdSpi::AddSub(const std::set<std::string> &symbolSet, const std::string &ssSessionID)
{

}

bool CSgitMdSpi::CheckValid(
  const std::set<std::string> &symbolSet, 
  const std::string &ssMDReqID, 
  const std::string &ssSessionID, 
  const std::string &ssSessionKey)
{
 // bool bIsOk = true;
 // std::string ssErrMsg = "";

 // //
 // std::map<std::string, std::set<std::string>>::iterator it = m_mapMDReqId.find(ssSessionKey);
 // if (it != m_mapMDReqId.end())
 // {
 //   if (it->second.count(ssMDReqID) > 0)
 //   {
 //     bIsOk = false;
 //     ssErrMsg = ""
 //   }
 // }
	////主要检查是否带多个symbol，其中有一个为ALL_SYMBOL(既然为ALL_SYMBOL，那么就应该只有一个，否则逻辑上说不通)
	//if (symbolSet.size() == 1) return true;

	//for (std::set<std::string>::const_iterator cit = symbolSet.begin(); cit != symbolSet.end(); cit++)
	//{
	//	if (*cit == ALL_SYMBOL)
	//	{
	//		FIX42::MarketDataRequestReject marketDataRequestReject = FIX42::MarketDataRequestReject(FIX::MDReqID(ssMDReqID));
	//		//marketDataRequestReject.set(FIX::MDReqRejReason())
	//	}
	//}

  return true;
}

void CSgitMdSpi::OnMessage(const FIX::Message& oMsg, const FIX::SessionID& oSessionID)
{
  AddFixInfo(oMsg);

  FIX::MsgType msgType;
  oMsg.getHeader().getField(msgType);

  if (msgType == FIX::MsgType_MarketDataRequest)
  {
    MarketDataRequest((const FIX42::MarketDataRequest&) oMsg);
  }
}

void CSgitMdSpi::AddFixInfo(const FIX::Message& oMsg)
{

}

bool CSgitMdSpi::GetMarketData(const std::string ssSymbol, CThostFtdcDepthMarketDataField &stuMarketData)
{
  ScopedReadRWLock scopeReadLock(m_rwLockSnapShot);

  if (m_mapSnapshot.count(ssSymbol) < 1) return false;
  stuMarketData = m_mapSnapshot[ssSymbol];
  return true;
}

