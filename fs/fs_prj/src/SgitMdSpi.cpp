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
  //�������
  FIX::NoRelatedSym noRelatedSym;
  //������
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
    SendMarketData(oMarketDataRequest, symbolSet);
    break;
  case FIX::SubscriptionRequestType_SNAPSHOT_PLUS_UPDATES:
    SendMarketData(oMarketDataRequest, symbolSet);
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

  //��¼�ɹ�����ȫ�г�����
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

  //���͸�����Ȥ�Ķ��ķ�


}

void CSgitMdSpi::SendMarketData(const FIX42::MarketDataRequest& oMarketDataRequest, const std::set<std::string> &symbolSet)
{
  FIX::MDReqID mdReqID;
  oMarketDataRequest.get(mdReqID);

  Convert::EnCvtType enSymbolType = m_pSgitCtx->GetSymbolType(CToolkit::GetSessionKey(oMarketDataRequest));
  CThostFtdcDepthMarketDataField stuMarketData;
  std::string ssExchange = "";
  for(std::set<std::string>::const_iterator citSymbol = symbolSet.begin(); citSymbol != symbolSet.end(); citSymbol++)
  {
    if (!GetMarketData(*citSymbol, stuMarketData)) continue;
    
    FIX42::MarketDataSnapshotFullRefresh oMdSnapShot = FIX42::MarketDataSnapshotFullRefresh();
    oMdSnapShot.setField(mdReqID);
    oMdSnapShot.setField(FIX::Symbol(enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
      *citSymbol : m_pSgitCtx->CvtSymbol(*citSymbol, enSymbolType)));
    
    ssExchange = enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
      stuMarketData.ExchangeID : m_pSgitCtx->CvtExchange(stuMarketData.ExchangeID, enSymbolType);
    //oMdSnapShot.setField(FIX::SecurityType(ssExchange));
    oMdSnapShot.setField(FIX::SecurityExchange(ssExchange));

    AddPrice(oMdSnapShot, FIX::MDEntryType_TRADE, stuMarketData.LastPrice, stuMarketData.Volume);
    AddPrice(oMdSnapShot, FIX::MDEntryType_OPENING_PRICE, stuMarketData.OpenPrice);
    AddPrice(oMdSnapShot, FIX::MDEntryType_CLOSING_PRICE, stuMarketData.ClosePrice);
    AddPrice(oMdSnapShot, FIX::MDEntryType_TRADING_SESSION_HIGH_PRICE, stuMarketData.HighestPrice);
    AddPrice(oMdSnapShot, FIX::MDEntryType_TRADING_SESSION_LOW_PRICE, stuMarketData.LowestPrice);

    if (stuMarketData.BidVolume1 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_BID, stuMarketData.BidPrice1, stuMarketData.BidVolume1, 1);
    }
    if (stuMarketData.AskVolume1 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_OFFER, stuMarketData.AskPrice1, stuMarketData.AskVolume1, 1);
    }
    if (stuMarketData.BidVolume2 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_BID, stuMarketData.BidPrice2, stuMarketData.BidVolume2, 2);
    }
    if (stuMarketData.AskVolume2 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_OFFER, stuMarketData.AskPrice2, stuMarketData.AskVolume2, 2);
    }
    if (stuMarketData.BidVolume3 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_BID, stuMarketData.BidPrice3, stuMarketData.BidVolume3, 3);
    }
    if (stuMarketData.AskVolume3 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_OFFER, stuMarketData.AskPrice3, stuMarketData.AskVolume3, 3);
    }
    if (stuMarketData.BidVolume4 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_BID, stuMarketData.BidPrice4, stuMarketData.BidVolume4, 4);
    }
    if (stuMarketData.AskVolume4 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_OFFER, stuMarketData.AskPrice4, stuMarketData.AskVolume4, 4);
    }
    if (stuMarketData.BidVolume5 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_BID, stuMarketData.BidPrice5, stuMarketData.BidVolume5, 5);
    }
    if (stuMarketData.AskVolume5 > 0)
    {
      AddPrice(oMdSnapShot, FIX::MDEntryType_OFFER, stuMarketData.AskPrice5, stuMarketData.AskVolume5, 5);
    }

    CToolkit::Send(oMarketDataRequest, oMdSnapShot);
  }
}

void CSgitMdSpi::AddPrice(FIX42::MarketDataSnapshotFullRefresh &oMdSnapShot, char chEntryType, double dPrice, int iVolume /*= 0*/, int iPos /*= 0*/)
{
  FIX42::MarketDataSnapshotFullRefresh::NoMDEntries noMdEntriesGroup = FIX42::MarketDataSnapshotFullRefresh::NoMDEntries();
  noMdEntriesGroup.setField(FIX::MDEntryType(chEntryType));
  noMdEntriesGroup.setField(FIX::MDEntryPx(dPrice));
  if(iVolume > 0) noMdEntriesGroup.setField(FIX::MDEntrySize(iVolume));
  if(iPos > 0) noMdEntriesGroup.setField(FIX::MDEntryPositionNo(iPos));
  oMdSnapShot.addGroup(noMdEntriesGroup);
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
	////��Ҫ����Ƿ�����symbol��������һ��ΪALL_SYMBOL(��ȻΪALL_SYMBOL����ô��Ӧ��ֻ��һ���������߼���˵��ͨ)
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

