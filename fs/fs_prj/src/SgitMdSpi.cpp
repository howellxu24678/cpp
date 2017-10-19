#include "SgitMdSpi.h"
#include "SgitContext.h"
#include "Log.h"

CSgitMdSpi::CSgitMdSpi(CSgitContext *pSgitCtx, CThostFtdcMdApi *pMdReqApi, const std::string &ssTradeId, const std::string &ssPassword) 
	: m_pSgitCtx(pSgitCtx)
	, m_pMdReqApi(pMdReqApi)
	, m_ssTradeID(ssTradeId)
  , m_ssPassword(ssPassword)
  , m_acRequestId(0)
{

}

CSgitMdSpi::~CSgitMdSpi()
{

}

void CSgitMdSpi::MarketDataRequest(const FIX42::MarketDataRequest& oMarketDataRequest)
{
  FIX::SubscriptionRequestType subscriptionRequestType;
  FIX::NoRelatedSym noRelatedSym;
  FIX42::MarketDataRequest::NoRelatedSym symGroup;
  FIX::Symbol symbol;

  oMarketDataRequest.get(subscriptionRequestType);
  oMarketDataRequest.get(noRelatedSym);

  int iSymCount = noRelatedSym.getValue(), iSymLen = 0;
  char **ppInstrumentID = new char* [iSymCount];
  for (int i = 0; i < iSymCount; i++)
  {
    oMarketDataRequest.getGroup(i + 1, symGroup);
    symGroup.get(symbol);
    LOG(DEBUG_LOG_LEVEL, "symbol:%s", symbol.getValue().c_str());

    iSymLen = symbol.getValue().size();
    ppInstrumentID[i] = new char[iSymLen + 1];
    memset(ppInstrumentID[i], 0, iSymLen + 1);
    strncpy(ppInstrumentID[i], symbol.getValue().c_str(), iSymLen);
  }
  m_pMdReqApi->SubscribeMarketData(ppInstrumentID, iSymCount);

  for (int i = 0; i < iSymCount; i++)
  {
    delete[] ppInstrumentID[i];
    ppInstrumentID[i] = NULL;
  }
  delete[] ppInstrumentID;
  ppInstrumentID = NULL;
}

void CSgitMdSpi::OnFrontConnected()
{
	CThostFtdcReqUserLoginField stuLogin;
	memset(&stuLogin, 0, sizeof(CThostFtdcReqUserLoginField));
	strncpy(stuLogin.UserID, m_ssTradeID.c_str(), sizeof(stuLogin.UserID));
	strncpy(stuLogin.Password, m_ssPassword.c_str(), sizeof(stuLogin.Password));
	m_pMdReqApi->ReqUserLogin(&stuLogin, m_acRequestId++);

	LOG(INFO_LOG_LEVEL, "ReqUserLogin userID:%s", stuLogin.UserID);
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
  LOG(INFO_LOG_LEVEL, "InstrumentID:%s,Price:%lf", pDepthMarketData->InstrumentID, pDepthMarketData->LastPrice);
}

void CSgitMdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  if (!pRspUserLogin || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "UserID:%s,ErrorID:%d,ErrorMsg:%s", pRspUserLogin->UserID, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
}

