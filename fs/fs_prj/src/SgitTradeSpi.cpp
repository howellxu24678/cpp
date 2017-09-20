#include "SgitTradeSpi.h"
#include "SgitContext.h"
#include "Log.h"
#include "quickfix/fix42/ExecutionReport.h"

CSgitTradeSpi::CSgitTradeSpi(CSgitContext *pSgitCtx, CThostFtdcTraderApi *pReqApi, const std::string &ssSgitCfgPath, const std::string &ssTradeId) 
  : m_pSgitCtx(pSgitCtx)
  , m_pTradeApi(pReqApi)
  , m_ssSgitCfgPath(ssSgitCfgPath)
  , m_ssTradeID(ssTradeId)
  , m_enSymbolType(Convert::Original)
{

}

CSgitTradeSpi::~CSgitTradeSpi()
{
  //释放Api内存
  if( m_pTradeApi )
  {
    m_pTradeApi->RegisterSpi(nullptr);
    m_pTradeApi->Release();
    m_pTradeApi = nullptr;
  }
}

void CSgitTradeSpi::OnFrontConnected()
{
	LOG(INFO_LOG_LEVEL, __FUNCTION__);

	CThostFtdcReqUserLoginField stuLogin;
	memset(&stuLogin, 0, sizeof(CThostFtdcReqUserLoginField));
	strncpy(stuLogin.UserID, m_ssTradeID.c_str(), sizeof(stuLogin.UserID));
	strncpy(stuLogin.Password, m_ssPassword.c_str(), sizeof(stuLogin.Password));
	m_pTradeApi->ReqUserLogin(&stuLogin, m_acRequestId++);

	LOG(INFO_LOG_LEVEL, "ReqUserLogin userID:%s",stuLogin.UserID);
}

void CSgitTradeSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, 
  CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  if (!pRspUserLogin || !pRspInfo) return;

  LOG(INFO_LOG_LEVEL, "OnRspUserLogin userID:%s,errID:%d,errMsg:%s", 
    pRspUserLogin->UserID, pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  //fs是否不需要这一步
  if (!pRspInfo->ErrorID)
  {
    CThostFtdcSettlementInfoConfirmField stuConfirm;
    memset(&stuConfirm, 0, sizeof(CThostFtdcSettlementInfoConfirmField));
    strncpy(stuConfirm.InvestorID, pRspUserLogin->UserID, sizeof(stuConfirm.InvestorID));
    strncpy(stuConfirm.BrokerID, pRspUserLogin->BrokerID, sizeof(stuConfirm.BrokerID));

    m_pTradeApi->ReqSettlementInfoConfirm(&stuConfirm, m_acRequestId++);
  }
}

int CSgitTradeSpi::ReqOrderInsert(const FIX42::NewOrderSingle& oNewOrderSingleMsg)
{
	//FIX::SenderCompID senderCompId;
	//FIX::OnBehalfOfCompID  onBehalfOfCompId;
	FIX::Account account;
	FIX::ClOrdID clOrdID;
	//FIX::SecurityExchange securityExchange;
	FIX::Symbol symbol;
	FIX::OrderQty orderQty;
	//FIX::HandlInst handInst;
	FIX::OrdType ordType;
	FIX::Price price;
	FIX::Side side;
	FIX::OpenClose openClose;
	FIX::TransactTime transTime;
	FIX::TimeInForce timeInForce;
	//if ( ordType != FIX::OrdType_LIMIT )
	//  throw FIX::IncorrectTagValue( ordType.getField() );
	oNewOrderSingleMsg.get(account);
	oNewOrderSingleMsg.get(clOrdID);
	//oNewOrderSingleMsg.get(securityExchange);
	oNewOrderSingleMsg.get(symbol);
	oNewOrderSingleMsg.get(orderQty);
	//oNewOrderSingleMsg.get(handInst);
	oNewOrderSingleMsg.get(ordType);
	oNewOrderSingleMsg.get(price);
	oNewOrderSingleMsg.get(side);
	oNewOrderSingleMsg.get(openClose);
	oNewOrderSingleMsg.get(transTime);
	//oNewOrderSingleMsg.get(timeInForce);

	CThostFtdcInputOrderField stuInputOrder;
	memset(&stuInputOrder, 0, sizeof(CThostFtdcInputOrderField));
  
	strncpy(stuInputOrder.UserID, m_ssTradeID.c_str(), sizeof(stuInputOrder.UserID));
  strncpy(stuInputOrder.InvestorID, m_pSgitCtx->GetRealAccont(oNewOrderSingleMsg).c_str(), sizeof(stuInputOrder.InvestorID));
  strncpy(stuInputOrder.OrderRef, clOrdID.getValue().c_str(), sizeof(stuInputOrder.OrderRef));
  strncpy(
    stuInputOrder.InstrumentID, 
    m_enSymbolType == Convert::Original ? 
    symbol.getValue().c_str() : m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
    sizeof(stuInputOrder.InstrumentID));
  
  stuInputOrder.VolumeTotalOriginal = (int)orderQty.getValue();
	stuInputOrder.OrderPriceType = m_pSgitCtx->CvtDict(ordType.getField(), ordType.getValue(), Convert::Sgit);
  stuInputOrder.LimitPrice = price.getValue();
	stuInputOrder.Direction = m_pSgitCtx->CvtDict(side.getField(), side.getValue(), Convert::Sgit);
  stuInputOrder.CombOffsetFlag[0] = m_pSgitCtx->CvtDict(openClose.getField(), openClose.getValue(), Convert::Sgit);

	stuInputOrder.TimeCondition = THOST_FTDC_TC_GFD;
	stuInputOrder.MinVolume = 1;
	stuInputOrder.VolumeCondition = THOST_FTDC_VC_AV;
	stuInputOrder.ContingentCondition = THOST_FTDC_CC_Immediately;
	stuInputOrder.StopPrice = 0.0;
	stuInputOrder.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	stuInputOrder.IsAutoSuspend = false;
	stuInputOrder.UserForceClose = false;
	stuInputOrder.IsSwapOrder = false;
	stuInputOrder.CombHedgeFlag[0] = '0'; 
	
	stuInputOrder.RequestID = m_acRequestId++;
	return m_pTradeApi->ReqOrderInsert(&stuInputOrder, stuInputOrder.RequestID);
}

void CSgitTradeSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	LOG(INFO_LOG_LEVEL, "%s, ErrorID:%d, ErrorMsg:%s,OrderRef:%s, OrderSysID:%s, ExchangeID:%s", 
		__FUNCTION__, pRspInfo->ErrorID, pRspInfo->ErrorMsg, 
		pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID);


  FIX42::ExecutionReport executionReport = FIX42::ExecutionReport
    ( FIX::OrderID( pInputOrder->OrderSysID),
    FIX::ExecID(""),
    FIX::ExecTransType(FIX::ExecTransType_NEW),
    FIX::ExecType(FIX::ExecType_FILL),
    FIX::OrdStatus(FIX::OrdStatus_FILLED),
    FIX::Symbol(m_enSymbolType == Convert::Original ? 
    pInputOrder->InstrumentID : m_pSgitCtx->CvtSymbol(pInputOrder->InstrumentID, m_enSymbolType).c_str()),
    FIX::Side(pInputOrder->Direction),
    FIX::LeavesQty(pInputOrder->VolumeTotalOriginal),
    FIX::CumQty(0),
    FIX::AvgPx(0));

  executionReport.set(FIX::ClOrdID(pInputOrder->OrderRef));
  executionReport.set(FIX::OrderQty(pInputOrder->VolumeTotalOriginal));
  executionReport.set(FIX::LastShares(0));
  executionReport.set(FIX::LastPx(0));

  m_pSgitCtx->Send(pInputOrder->InvestorID, executionReport);

  //if( message.isSet(account) )
  //  executionReport.setField( message.get(account) );

  //try
  //{
  //  FIX::Session::sendToTarget( executionReport, sessionID );
  //}
  //catch ( FIX::SessionNotFound& ) {}
}

void CSgitTradeSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	LOG(INFO_LOG_LEVEL, __FUNCTION__);
}

void CSgitTradeSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	LOG(INFO_LOG_LEVEL, "%s", __FUNCTION__);
}

void CSgitTradeSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	LOG(INFO_LOG_LEVEL, __FUNCTION__);
}

void CSgitTradeSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
	LOG(INFO_LOG_LEVEL, __FUNCTION__);
}

void CSgitTradeSpi::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
	LOG(INFO_LOG_LEVEL, __FUNCTION__);
}

void CSgitTradeSpi::Init()
{
  AutoPtr<IniFileConfiguration> apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);
  m_ssPassword = apSgitConf->getString(m_ssTradeID + ".PassWord");
  m_enSymbolType = (Convert::EnSymbolType)apSgitConf->getInt(m_ssTradeID + ".SymbolType");
}
