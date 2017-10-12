#include "SgitMdSpi.h"
#include "SgitContext.h"
#include "Log.h"

CSgitMdSpi::CSgitMdSpi(CThostFtdcMdApi *pMdApi)
  : m_pMdApi(pMdApi)
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

  for (int i = 0; i < noRelatedSym.getValue(); i++)
  {
    oMarketDataRequest.getGroup(i + 1, symGroup);
    symGroup.get(symbol);
    LOG(INFO_LOG_LEVEL, "symbol:%s", symbol.getValue().c_str());
  }
}

void CSgitMdSpi::OnFrontConnected()
{
  LOG(INFO_LOG_LEVEL, "");
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
  LOG(ERROR_LOG_LEVEL, "ErrorID:%d,ErrorMsg:%s,RequestID:%d", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
}

void CSgitMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  LOG(INFO_LOG_LEVEL, "InstrumentID:%s,ErrorID:%d,ErrorMsg:%s,RequestID:%d", 
    pSpecificInstrument->InstrumentID, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
}

void CSgitMdSpi::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  LOG(INFO_LOG_LEVEL, "InstrumentID:%s,ErrorID:%d,ErrorMsg:%s,RequestID:%d", 
    pSpecificInstrument->InstrumentID, pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
}

void CSgitMdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
  LOG(INFO_LOG_LEVEL, "InstrumentID:%s,Price:%lf", pDepthMarketData->InstrumentID, pDepthMarketData->LastPrice);
}

