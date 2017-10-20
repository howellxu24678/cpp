#include "SgitMdSpi.h"
#include "SgitContext.h"
#include "Log.h"


CSgitMdSpi::CSgitMdSpi(CSgitContext *pSgitCtx, CThostFtdcMdApi *pMdReqApi, const std::string &ssTradeId, const std::string &ssPassword) 
	: m_pSgitCtx(pSgitCtx)
	, m_pMdReqApi(pMdReqApi)
  , m_acRequestId(0)
{
  memset(&m_stuLogin, 0, sizeof(CThostFtdcReqUserLoginField));
  strncpy(m_stuLogin.UserID, ssTradeId.c_str(), sizeof(m_stuLogin.UserID));
  strncpy(m_stuLogin.Password, ssPassword.c_str(), sizeof(m_stuLogin.Password));

  m_mapCode2SubSession.clear();
  m_lSubAllCodeSession.clear();
  m_mapSnapshot.clear();
}

CSgitMdSpi::~CSgitMdSpi()
{

}

void CSgitMdSpi::MarketDataRequest(const FIX42::MarketDataRequest& oMarketDataRequest)
{
  FIX::SubscriptionRequestType subscriptionRequestType;
  //代码个数
  FIX::NoRelatedSym noRelatedSym;
  //代码组
  FIX42::MarketDataRequest::NoRelatedSym symGroup;
  FIX::Symbol symbol;

  oMarketDataRequest.get(subscriptionRequestType);
  oMarketDataRequest.get(noRelatedSym);

  int iSymCount = noRelatedSym.getValue(), iSymLen = 0;
  for (int i = 0; i < iSymCount; i++)
  {
    oMarketDataRequest.getGroup(i + 1, symGroup);
    symGroup.get(symbol);
    LOG(DEBUG_LOG_LEVEL, "symbol:%s", symbol.getValue().c_str());
  }

  //char **ppInstrumentID = new char* [iSymCount];
  //for (int i = 0; i < iSymCount; i++)
  //{
  //  oMarketDataRequest.getGroup(i + 1, symGroup);
  //  symGroup.get(symbol);
  //  LOG(DEBUG_LOG_LEVEL, "symbol:%s", symbol.getValue().c_str());

  //  iSymLen = symbol.getValue().size();
  //  ppInstrumentID[i] = new char[iSymLen + 1];
  //  memset(ppInstrumentID[i], 0, iSymLen + 1);
  //  strncpy(ppInstrumentID[i], symbol.getValue().c_str(), iSymLen);
  //}
  //m_pMdReqApi->SubscribeMarketData(ppInstrumentID, iSymCount);

  //for (int i = 0; i < iSymCount; i++)
  //{
  //  delete[] ppInstrumentID[i];
  //  ppInstrumentID[i] = NULL;
  //}
  //delete[] ppInstrumentID;
  //ppInstrumentID = NULL;
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

  do 
  {
    ScopedWriteRWLock scopeWriteLock(m_rwLockSnapShot);

  } while (0);
  

  LOG(INFO_LOG_LEVEL, "InstrumentID:%s,Price:%lf", pDepthMarketData->InstrumentID, pDepthMarketData->LastPrice);
}

