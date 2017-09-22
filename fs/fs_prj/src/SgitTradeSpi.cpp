#include "SgitTradeSpi.h"
#include "SgitContext.h"
#include "Log.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "Poco/Format.h"
#include "Poco/UUIDGenerator.h"
#include "Toolkit.h"

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

  LOG(INFO_LOG_LEVEL, "userID:%s,errID:%d,errMsg:%s", 
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

int CSgitTradeSpi::ReqOrderInsert(const FIX42::NewOrderSingle& oNewOrderSingle)
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
	//FIX::TransactTime transTime;
	//FIX::TimeInForce timeInForce;
	//if ( ordType != FIX::OrdType_LIMIT )
	//  throw FIX::IncorrectTagValue( ordType.getField() );
	oNewOrderSingle.get(account);
	oNewOrderSingle.get(clOrdID);
	//oNewOrderSingleMsg.get(securityExchange);
	oNewOrderSingle.get(symbol);
	oNewOrderSingle.get(orderQty);
	//oNewOrderSingleMsg.get(handInst);
	oNewOrderSingle.get(ordType);
	oNewOrderSingle.get(price);
	oNewOrderSingle.get(side);
	oNewOrderSingle.get(openClose);
	//oNewOrderSingle.get(transTime);
	//oNewOrderSingleMsg.get(timeInForce);

	CThostFtdcInputOrderField stuInputOrder;
	memset(&stuInputOrder, 0, sizeof(CThostFtdcInputOrderField));
  
	strncpy(stuInputOrder.UserID, m_ssTradeID.c_str(), sizeof(stuInputOrder.UserID));
  strncpy(stuInputOrder.InvestorID, m_pSgitCtx->GetRealAccont(oNewOrderSingle).c_str(), sizeof(stuInputOrder.InvestorID));
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

int CSgitTradeSpi::ReqOrderAction(const FIX42::OrderCancelRequest& oOrderCancel)
{
  //本地号
  FIX::OrderID orderID;
  //被撤单本地号
  FIX::OrigClOrdID origClOrdID;
  //被撤单系统报单号
  FIX::ClOrdID clOrdID;
  FIX::Side side;
  FIX::Symbol symbol;
  FIX::Account account;

  oOrderCancel.get(orderID);
  oOrderCancel.get(origClOrdID);
  oOrderCancel.get(clOrdID);
  oOrderCancel.get(side);
  oOrderCancel.get(symbol);
  oOrderCancel.get(account);

  CThostFtdcInputOrderActionField stuInputOrderAction;
  memset(&stuInputOrderAction, 0, sizeof(CThostFtdcInputOrderActionField));

  //不一定送过来的可以转为数字
  Poco::strToInt(orderID.getValue(), stuInputOrderAction.OrderActionRef, 10);
  strncpy(stuInputOrderAction.OrderRef, origClOrdID.getValue().c_str(), sizeof(stuInputOrderAction.OrderRef));
  strncpy(stuInputOrderAction.OrderSysID, clOrdID.getValue().c_str(), sizeof(stuInputOrderAction.OrderSysID));

  stuInputOrderAction.RequestID = m_acRequestId++;
  return m_pTradeApi->ReqOrderAction(&stuInputOrderAction, stuInputOrderAction.RequestID);
}


void CSgitTradeSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  std::string ssUUid = CToolkit::GetUuid();
	LOG(INFO_LOG_LEVEL, "ErrorID:%d, ErrorMsg:%s,OrderRef:%s, OrderSysID:%s, ExchangeID:%s,uuid:%s", 
		pRspInfo->ErrorID, pRspInfo->ErrorMsg, 
		pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID, ssUUid.c_str());

  bool isSuccess = pRspInfo->ErrorID == 0;

  FIX42::ExecutionReport executionReport = FIX42::ExecutionReport();
  executionReport.set(FIX::OrderID( strlen(pInputOrder->OrderSysID) < 1 ? " " : pInputOrder->OrderSysID));
  executionReport.set(FIX::ExecID(ssUUid));
  executionReport.set(FIX::Symbol(m_enSymbolType == Convert::Original ? 
    pInputOrder->InstrumentID : m_pSgitCtx->CvtSymbol(pInputOrder->InstrumentID, m_enSymbolType).c_str()));
  executionReport.set(FIX::Side(m_pSgitCtx->CvtDict(FIX::FIELD::Side, pInputOrder->Direction, Convert::Fix)));
  executionReport.set(FIX::LeavesQty(pInputOrder->VolumeTotalOriginal));
  executionReport.set(FIX::CumQty(0));
  executionReport.set(FIX::AvgPx(0));
  executionReport.set(FIX::ClOrdID(pInputOrder->OrderRef));
  executionReport.set(FIX::OrderQty(pInputOrder->VolumeTotalOriginal));
  executionReport.set(FIX::LastShares(0));
  executionReport.set(FIX::LastPx(0));

  executionReport.set(FIX::ExecTransType(FIX::ExecTransType_NEW));
  executionReport.set(FIX::OrdStatus(isSuccess ? FIX::OrdStatus_NEW : FIX::OrdStatus_REJECTED));
  executionReport.set(FIX::ExecType(isSuccess ? FIX::ExecType_NEW : FIX::ExecType_REJECTED));

  if(!isSuccess)
  {
    executionReport.set(FIX::OrdRejReason(FIX::OrdRejReason_DUPLICATE_ORDER));
    executionReport.set(FIX::Text(format("errID:%d,errMsg:%s", pRspInfo->ErrorID, (std::string)(pRspInfo->ErrorMsg))));
  }

  m_pSgitCtx->Send(pInputOrder->InvestorID, executionReport);
}

void CSgitTradeSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	LOG(INFO_LOG_LEVEL, "ErrorID:%d,ErrorMsg:%s,OrderActionRef:%s,OrderRef:%s,OrderSysID:%s,ActionFlag:%c,VolumeChange:%d", 
    pRspInfo->ErrorID, pRspInfo->ErrorMsg, pInputOrderAction->OrderActionRef, pInputOrderAction->OrderRef, 
    pInputOrderAction->OrderSysID, pInputOrderAction->ActionFlag, pInputOrderAction->VolumeChange);
}

void CSgitTradeSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
  std::string ssUUid = CToolkit::GetUuid();
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderLocalID:%s,OrderSysID:%s,OrderStatus:%c,VolumeTraded:%d,uuid:%s,LimitPrice:%f,StopPrice:%f",
    pOrder->OrderRef, pOrder->OrderLocalID, pOrder->OrderSysID, pOrder->OrderStatus, pOrder->VolumeTraded, 
    ssUUid.c_str(), pOrder->LimitPrice, pOrder->StopPrice);

  FIX42::ExecutionReport executionReport = FIX42::ExecutionReport();
  executionReport.set(FIX::OrderID(strlen(pOrder->OrderSysID) < 1 ? " " : pOrder->OrderSysID));
  executionReport.set(FIX::ExecID(ssUUid.c_str()));
  executionReport.set(FIX::Symbol(m_enSymbolType == Convert::Original ? 
    pOrder->InstrumentID : m_pSgitCtx->CvtSymbol(pOrder->InstrumentID, m_enSymbolType).c_str()));
  executionReport.set(FIX::Side(m_pSgitCtx->CvtDict(FIX::FIELD::Side, pOrder->Direction, Convert::Fix)));
  executionReport.set(FIX::LeavesQty(pOrder->VolumeTotalOriginal));
  executionReport.set(FIX::CumQty(pOrder->VolumeTotalOriginal - pOrder->VolumeTotal));
  executionReport.set(FIX::AvgPx(0));
  executionReport.set(FIX::ClOrdID(pOrder->OrderRef));
  if (strlen(pOrder->RelativeOrderSysID) > 1)
    executionReport.set(FIX::OrigClOrdID(pOrder->RelativeOrderSysID));
  executionReport.set(FIX::OrderQty(pOrder->VolumeTotalOriginal));
  executionReport.set(FIX::LastShares(pOrder->VolumeTraded));
  executionReport.set(FIX::LastPx(pOrder->LimitPrice));

  executionReport.set(FIX::ExecTransType(FIX::ExecTransType_NEW));
  char chOrderStatus = m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, pOrder->OrderStatus, Convert::Fix);
  executionReport.set(FIX::OrdStatus(chOrderStatus));
  executionReport.set(FIX::ExecType(chOrderStatus));


  m_pSgitCtx->Send(pOrder->InvestorID, executionReport);
}

void CSgitTradeSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,TradeID:%s,OrderSysID:%s,Price:%f,Volume:%d", 
    pTrade->OrderRef, pTrade->TradeID, pTrade->OrderSysID, pTrade->Price, pTrade->Volume);

  //FIX42::ExecutionReport executionReport = FIX42::ExecutionReport();
  //executionReport.set(FIX::OrderID(strlen(pTrade->OrderSysID) < 1 ? " " : pTrade->OrderSysID));
  //executionReport.set(FIX::ExecID(strlen(pTrade->TradeID) < 1 ? " " : pTrade->TradeID));
  //executionReport.set(FIX::Symbol(m_enSymbolType == Convert::Original ? 
  //  pTrade->InstrumentID : m_pSgitCtx->CvtSymbol(pTrade->InstrumentID, m_enSymbolType).c_str()));
  //executionReport.set(FIX::Side(m_pSgitCtx->CvtDict(FIX::FIELD::Side, pTrade->Direction, Convert::Fix)));
  ////executionReport.set(FIX::LeavesQty(pTrade->VolumeTotalOriginal));
  ////executionReport.set(FIX::CumQty(0));
  ////executionReport.set(FIX::AvgPx(0));
  //executionReport.set(FIX::ClOrdID(pTrade->OrderRef));
  ////executionReport.set(FIX::OrderQty(pInputOrder->VolumeTotalOriginal));
  //executionReport.set(FIX::LastShares(pTrade->Volume));
  //executionReport.set(FIX::LastPx(pTrade->Price));

  //executionReport.set(FIX::ExecTransType(FIX::ExecTransType_NEW));
  //executionReport.set(FIX::OrdStatus(isSuccess ? FIX::OrdStatus_NEW : FIX::OrdStatus_REJECTED));
  //executionReport.set(FIX::ExecType(isSuccess ? FIX::ExecType_NEW : FIX::ExecType_REJECTED));

  //if(!isSuccess)
  //{
  //  executionReport.set(FIX::OrdRejReason(FIX::OrdRejReason_DUPLICATE_ORDER));
  //  executionReport.set(FIX::Text(format("errID:%d,errMsg:%s", pRspInfo->ErrorID, (std::string)(pRspInfo->ErrorMsg))));
  //}

  //m_pSgitCtx->Send(pTrade->InvestorID, executionReport);
}

void CSgitTradeSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
  LOG(INFO_LOG_LEVEL, "ErrorID:%d, ErrorMsg:%s,OrderRef:%s, OrderSysID:%s, ExchangeID:%s", 
    pRspInfo->ErrorID, pRspInfo->ErrorMsg, 
    pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID);
}

void CSgitTradeSpi::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
  LOG(INFO_LOG_LEVEL, "ErrorID:%d,ErrorMsg:%s,OrderActionRef:%s,OrderRef:%s,OrderSysID:%s,ActionFlag:%c,VolumeChange:%d", 
    pRspInfo->ErrorID, pRspInfo->ErrorMsg, pOrderAction->OrderActionRef, pOrderAction->OrderRef, 
    pOrderAction->OrderSysID, pOrderAction->ActionFlag, pOrderAction->VolumeChange);
}

void CSgitTradeSpi::Init()
{
  AutoPtr<IniFileConfiguration> apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);
  m_ssPassword = apSgitConf->getString(m_ssTradeID + ".PassWord");
  m_enSymbolType = (Convert::EnSymbolType)apSgitConf->getInt(m_ssTradeID + ".SymbolType");
}
