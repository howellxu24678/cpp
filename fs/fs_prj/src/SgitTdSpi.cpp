#include "SgitTdSpi.h"
#include "SgitContext.h"
#include "Log.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix42/OrderCancelReject.h"
#include "Poco/Format.h"
#include "Poco/UUIDGenerator.h"
#include "Toolkit.h"

//CSgitTdSpi::CSgitTdSpi(CSgitContext *pSgitCtx, CThostFtdcTraderApi *pReqApi,  const std::string &ssUserId, const std::string &ssPassword) 
//  : m_stuTdParam.m_pSgitCtx(pSgitCtx)
//  , m_stuTdParam.m_pTdReqApi(pReqApi)
//  , m_ssUserId(ssUserId)
//  , m_ssPassword(ssPassword)
//  , m_enSymbolType(Convert::Unknow)
//	, m_acRequestId(0)
//	, m_acOrderRef(0)
//	, m_chOrderRef2ClOrderID(12*60*60*1000)//超时设为12小时
//	, m_chClOrderID2OrderRef(12*60*60*1000)//超时设为12小时
//  , m_chOrderRef2Order(12*60*60*1000)//超时设为12小时
//{
//
//}

CSgitTdSpi::CSgitTdSpi(const STUTdParam &stuTdParam)
  : m_stuTdParam(stuTdParam)
  , m_enSymbolType(Convert::Unknow)
	, m_acRequestId(0)
	, m_acOrderRef(0)
	, m_chOrderRef2ClOrderID(12*60*60*1000)//超时设为12小时
	, m_chClOrderID2OrderRef(12*60*60*1000)//超时设为12小时
  , m_chOrderRef2Order(12*60*60*1000)//超时设为12小时
{

}

CSgitTdSpi::~CSgitTdSpi()
{
  //释放Api内存
  if( m_stuTdParam.m_pTdReqApi )
  {
    m_stuTdParam.m_pTdReqApi->RegisterSpi(nullptr);
    m_stuTdParam.m_pTdReqApi->Release();
    m_stuTdParam.m_pTdReqApi = nullptr;
  }
}

void CSgitTdSpi::OnFrontConnected()
{
	CThostFtdcReqUserLoginField stuLogin;
	memset(&stuLogin, 0, sizeof(CThostFtdcReqUserLoginField));
	strncpy(stuLogin.UserID, m_stuTdParam.m_ssUserId.c_str(), sizeof(stuLogin.UserID));
	strncpy(stuLogin.Password, m_stuTdParam.m_ssPassword.c_str(), sizeof(stuLogin.Password));
	m_stuTdParam.m_pTdReqApi->ReqUserLogin(&stuLogin, m_acRequestId++);

	LOG(INFO_LOG_LEVEL, "ReqUserLogin userID:%s",stuLogin.UserID);
}

void CSgitTdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, 
  CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  if (!pRspUserLogin || !pRspInfo) return;

  LOG(INFO_LOG_LEVEL, "userID:%s,MaxOrderRef:%s,errID:%d,errMsg:%s", 
    pRspUserLogin->UserID, pRspUserLogin->MaxOrderRef, pRspInfo->ErrorID, pRspInfo->ErrorMsg);

	m_acOrderRef = atoi(pRspUserLogin->MaxOrderRef);

  //fs是否不需要这一步
  if (!pRspInfo->ErrorID)
  {
    CThostFtdcSettlementInfoConfirmField stuConfirm;
    memset(&stuConfirm, 0, sizeof(CThostFtdcSettlementInfoConfirmField));
    strncpy(stuConfirm.InvestorID, pRspUserLogin->UserID, sizeof(stuConfirm.InvestorID));
    strncpy(stuConfirm.BrokerID, pRspUserLogin->BrokerID, sizeof(stuConfirm.BrokerID));

    m_stuTdParam.m_pTdReqApi->ReqSettlementInfoConfirm(&stuConfirm, m_acRequestId++);
  }
}

void CSgitTdSpi::ReqOrderInsert(const FIX42::NewOrderSingle& oNewOrderSingle)
{
	CThostFtdcInputOrderField stuInputOrder;
	STUOrder stuOrder;
  std::string ssErrMsg = "";
	if(!Cvt(oNewOrderSingle, stuInputOrder, stuOrder, ssErrMsg))
  {
    SendExecutionReport(stuOrder, -1, ssErrMsg);
    return;
  }

	m_chOrderRef2Order.add(stuOrder.m_ssOrderRef, stuOrder);

	int iRet = m_stuTdParam.m_pTdReqApi->ReqOrderInsert(&stuInputOrder, stuInputOrder.RequestID);
	if (iRet != 0)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to call api:ReqOrderInsert,iRet:%d", iRet);
    SendExecutionReport(stuOrder, iRet, "Failed to call api:ReqOrderInsert");
    return;
	}
}

void CSgitTdSpi::ReqOrderAction(const FIX42::OrderCancelRequest& oOrderCancel)
{
  CThostFtdcInputOrderActionField stuInputOrderAction;
  std::string ssErrMsg = "";
	if(!Cvt(oOrderCancel, stuInputOrderAction, ssErrMsg))
  {
    SendOrderCancelReject(oOrderCancel, ssErrMsg);
    return;
  }

  int iRet = m_stuTdParam.m_pTdReqApi->ReqOrderAction(&stuInputOrderAction, stuInputOrderAction.RequestID);
	if (iRet != 0)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to call api:ReqOrderAction,iRet:%d", iRet);
		SendOrderCancelReject(stuInputOrderAction.OrderRef, iRet, "Failed to call api:ReqOrderAction");
    return;
	}
}


void CSgitTdSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pInputOrder || !pRspInfo) return;
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,ExchangeID:%s,ErrorID:%d, ErrorMsg:%s", 
		pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID,pRspInfo->ErrorID, pRspInfo->ErrorMsg);
  
  Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(pInputOrder->OrderRef);
  if (spStuOrder.isNull())
  {
    LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", pInputOrder->OrderRef);
    return;
  }

  spStuOrder->Update(*pInputOrder);

	SendExecutionReport(*spStuOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
}

void CSgitTdSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pInputOrderAction || !pRspInfo) return;
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,VolumeChange:%d,ErrorID:%d,ErrorMsg:%s", 
    pInputOrderAction->OrderRef, pInputOrderAction->OrderSysID, pInputOrderAction->VolumeChange, 
		pRspInfo->ErrorID, pRspInfo->ErrorMsg);

	Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(pInputOrderAction->OrderRef);
	if (spStuOrder.isNull())
	{
		LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", pInputOrderAction->OrderRef);
		return;
	}

	if(pRspInfo->ErrorID != 0)
	{
		SendOrderCancelReject(*spStuOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
	}
	else
	{
		SendExecutionReport(*spStuOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg, true);
	}
}

//撤单的情况在此回复执行回报，其余情况只用于更新订单的最新状态参数
void CSgitTdSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	if (!pOrder) return;
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,OrderStatus:%c,VolumeTraded:%d,VolumeLeave:%d",
     pOrder->OrderRef, pOrder->OrderSysID, pOrder->OrderStatus, pOrder->VolumeTraded, pOrder->VolumeTotal);

  Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(pOrder->OrderRef);
  if (spStuOrder.isNull())
  {
    LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", pOrder->OrderRef);
    return;
  }

  spStuOrder->Update(*pOrder);

  //订单被拒绝
  if (pOrder->OrderSubmitStatus == THOST_FTDC_OSS_InsertRejected 
    || pOrder->OrderSubmitStatus == THOST_FTDC_OSS_CancelRejected)
  {
    SendExecutionReport(*spStuOrder, -1, "Reject by Exchange");
  }
  //撤单回报
  else if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled)
  {
    //撤单的回报中应把订单剩余数量置0
    spStuOrder->m_iLeavesQty = 0;
    SendExecutionReport(*spStuOrder);
  }
}


//收到成交，回复执行回报
void CSgitTdSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	if (!pTrade) return;
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,TradeID:%s,Price:%f,Volume:%d", 
    pTrade->OrderRef, pTrade->OrderSysID, pTrade->TradeID, pTrade->Price, pTrade->Volume);

  Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(pTrade->OrderRef);
  if (spStuOrder.isNull())
  {
    LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", pTrade->OrderRef);
    return;
  }

  spStuOrder->Update(*pTrade);
  SendExecutionReport(pTrade->OrderRef);
}

void CSgitTdSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
	if (!pInputOrder || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,ExchangeID:%s,ErrorID:%d,ErrorMsg:%s", 
    pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID,pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  SendExecutionReport(pInputOrder->OrderRef, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
}

void CSgitTdSpi::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
	if (!pOrderAction || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "OrderActionRef:%s,OrderRef:%s,OrderSysID:%s,ActionFlag:%c,VolumeChange:%d,ErrorID:%d,ErrorMsg:%s", 
    pOrderAction->OrderActionRef, pOrderAction->OrderRef, pOrderAction->OrderSysID, pOrderAction->ActionFlag, 
    pOrderAction->VolumeChange, pRspInfo->ErrorID, pRspInfo->ErrorMsg);

	SendOrderCancelReject(pOrderAction->OrderRef, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
}

//void CSgitTdSpi::Init()
//{
//  AutoPtr<IniFileConfiguration> apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);
//  m_ssPassword = apSgitConf->getString(m_ssUserId + ".PassWord");
//  m_enSymbolType = (Convert::EnCvtType)apSgitConf->getInt(m_ssUserId + ".SymbolType");
//}

void CSgitTdSpi::SendExecutionReport(const STUOrder& oStuOrder, int iErrCode /*= 0*/, const std::string& ssErrMsg /*= ""*/, bool bIsPendingCancel /*= false*/)
{
  std::string& ssUUid = CToolkit::GetUuid();
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,ClOrdID:%s,OrderID:%s,uuid:%s", 
    oStuOrder.m_ssOrderRef.c_str(), oStuOrder.m_ssClOrdID.c_str(), oStuOrder.m_ssOrderID.c_str(), ssUUid.c_str());

  FIX42::ExecutionReport executionReport = FIX42::ExecutionReport();
  executionReport.set(FIX::ClOrdID(oStuOrder.m_ssClOrdID));
  executionReport.set(FIX::OrderID(oStuOrder.m_ssOrderID.length() < 1 ? " " : oStuOrder.m_ssOrderID));
  executionReport.set(FIX::ExecID(ssUUid));
  executionReport.set(FIX::Symbol(oStuOrder.m_ssSymbol));
  executionReport.set(FIX::Price(oStuOrder.m_dPrice));
  executionReport.set(FIX::OrderQty(oStuOrder.m_iOrderQty));
  executionReport.set(FIX::Side(oStuOrder.m_cSide));
  executionReport.set(FIX::LeavesQty(oStuOrder.m_iLeavesQty));
  executionReport.set(FIX::CumQty(oStuOrder.m_iCumQty));
  executionReport.set(FIX::AvgPx(oStuOrder.AvgPx()));
  executionReport.set(FIX::ExecTransType(FIX::ExecTransType_NEW));

  if (bIsPendingCancel || oStuOrder.m_vTradeRec.size() < 1)
  {
    executionReport.set(FIX::LastPx(0));
    executionReport.set(FIX::LastShares(0));
  }
  else
  {
    executionReport.set(FIX::LastPx(oStuOrder.m_vTradeRec.back().m_dMatchPrice));
    executionReport.set(FIX::LastShares(oStuOrder.m_vTradeRec.back().m_iMatchVolume));
  }

	//撤单应答
	if(bIsPendingCancel)
	{
		executionReport.set(FIX::OrdStatus(FIX::ExecType_PENDING_CANCEL));
		executionReport.set(FIX::ExecType(FIX::ExecType_PENDING_CANCEL));
	}
	//有错误发生
  else if (iErrCode != 0)
  {
		executionReport.set(FIX::ExecType(FIX::ExecType_REJECTED));
		executionReport.set(FIX::OrdStatus(FIX::OrdStatus_REJECTED));

		executionReport.set(FIX::OrdRejReason(FIX::OrdRejReason_BROKER_OPTION));
		executionReport.set(FIX::Text(format("errID:%d,errMsg:%s", iErrCode, ssErrMsg)));
  }
	//正常情况
  else
  {
		char chOrderStatus = m_stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, oStuOrder.m_cOrderStatus, Convert::Fix);
		executionReport.set(FIX::OrdStatus(chOrderStatus));
		executionReport.set(FIX::ExecType(chOrderStatus));
  }

  m_stuTdParam.m_pSgitCtx->Send(oStuOrder.m_ssAccout, executionReport);
}

void CSgitTdSpi::SendExecutionReport(const std::string& ssOrderRef, int iErrCode /*= 0*/, const std::string& ssErrMsg /*= ""*/)
{
  Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(ssOrderRef);
  if (spStuOrder.isNull())
  {
    LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", ssOrderRef.c_str());
    return;
  }

  return SendExecutionReport(*spStuOrder, iErrCode, ssErrMsg);
}

void CSgitTdSpi::SendOrderCancelReject(const std::string& ssOrderRef, int iErrCode, const std::string& ssErrMsg)
{
	Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(ssOrderRef);
	if (spStuOrder.isNull())
	{
		LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", ssOrderRef.c_str());
		return;
	}

	return SendOrderCancelReject(*spStuOrder, iErrCode, ssErrMsg);
}

void CSgitTdSpi::SendOrderCancelReject(const STUOrder& oStuOrder, int iErrCode, const std::string& ssErrMsg)
{
	FIX42::OrderCancelReject orderCancelReject = FIX42::OrderCancelReject(
		FIX::OrderID(oStuOrder.m_ssOrderID),
		FIX::ClOrdID(oStuOrder.m_ssCancelClOrdID),
		FIX::OrigClOrdID(oStuOrder.m_ssClOrdID),
		FIX::OrdStatus(m_stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, oStuOrder.m_cOrderStatus, Convert::Fix)),
		FIX::CxlRejResponseTo(FIX::CxlRejResponseTo_ORDER_CANCEL_REQUEST));

	orderCancelReject.set(FIX::Text(ssErrMsg));

	m_stuTdParam.m_pSgitCtx->Send(oStuOrder.m_ssAccout, orderCancelReject);
}

void CSgitTdSpi::SendOrderCancelReject(const FIX42::OrderCancelRequest& oOrderCancel, const std::string& ssErrMsg)
{
  FIX::OrderID orderID;
  FIX::ClOrdID clOrderID;
  FIX::OrigClOrdID origClOrdID;

  oOrderCancel.getFieldIfSet(orderID);
  oOrderCancel.get(clOrderID);
  oOrderCancel.get(origClOrdID);

  FIX42::OrderCancelReject orderCancelReject = FIX42::OrderCancelReject(
    FIX::OrderID(orderID.getValue().empty() ? "" : orderID.getValue()),
    FIX::ClOrdID(clOrderID),
    FIX::OrigClOrdID(origClOrdID),
    FIX::OrdStatus(FIX::OrdStatus_SUSPENDED),
    FIX::CxlRejResponseTo(FIX::CxlRejResponseTo_ORDER_CANCEL_REQUEST));

  orderCancelReject.set(FIX::Text(ssErrMsg));

  m_stuTdParam.m_pSgitCtx->Send(m_stuTdParam.m_pSgitCtx->GetRealAccont(oOrderCancel), orderCancelReject);
}

//bool CSgitTradeSpi::GetClOrdID(const std::string& ssOrderRef, std::string& ssClOrdID)
//{
//	return Get(m_chOrderRef2ClOrderID, ssOrderRef, ssClOrdID);
//}

bool CSgitTdSpi::GetOrderRef(const std::string& ssClOrdID, std::string& ssOrderRef)
{
	return Get(m_chClOrderID2OrderRef, ssClOrdID, ssOrderRef);
}

bool CSgitTdSpi::Get( ExpireCache<std::string, std::string>& oExpCache, const std::string& ssKey, std::string& ssValue)
{
	if(!oExpCache.has(ssKey))
	{
		LOG(ERROR_LOG_LEVEL, "Can not find key:%s in ExpireCache", ssKey.c_str());
		return false;
	}

	ssValue = *oExpCache.get(ssKey);
	return true;
}

bool CSgitTdSpi::AddOrderRefClOrdID(const std::string& ssOrderRef, const std::string& ssClOrdID, std::string& ssErrMsg)
{
	if (m_chOrderRef2ClOrderID.has(ssOrderRef))
	{
    ssErrMsg = "OrderRef:" + ssOrderRef + " is duplicate";
		LOG(WARN_LOG_LEVEL, ssErrMsg.c_str());
    return false;
	}
  m_chOrderRef2ClOrderID.add(ssOrderRef, ssClOrdID);
	

	if (m_chClOrderID2OrderRef.has(ssClOrdID))
	{
    ssErrMsg = "ClOrdID:" + ssClOrdID + " is duplicate";
    LOG(WARN_LOG_LEVEL, ssErrMsg.c_str());
		return false;
	}
	m_chClOrderID2OrderRef.add(ssClOrdID, ssOrderRef);

  return true;
}

bool CSgitTdSpi::Cvt(const FIX42::NewOrderSingle& oNewOrderSingle, CThostFtdcInputOrderField& stuInputOrder, STUOrder& stuOrder, std::string& ssErrMsg)
{
	//FIX::Account account;
	FIX::ClOrdID clOrdID;
	FIX::Symbol symbol;
	FIX::OrderQty orderQty;
	FIX::OrdType ordType;
	FIX::Price price;
	FIX::Side side;
	FIX::OpenClose openClose;

	//oNewOrderSingle.get(account);
	oNewOrderSingle.get(clOrdID);
	oNewOrderSingle.get(symbol);
	oNewOrderSingle.get(orderQty);
	oNewOrderSingle.get(ordType);
	oNewOrderSingle.get(price);
	oNewOrderSingle.get(side);
	oNewOrderSingle.get(openClose);

  std::string ssRealAccount = m_stuTdParam.m_pSgitCtx->GetRealAccont(oNewOrderSingle);
  //STUOrder
  stuOrder.m_ssAccout = ssRealAccount;
  stuOrder.m_ssClOrdID = clOrdID.getValue();
  stuOrder.m_cOrderStatus = THOST_FTDC_OST_NoTradeQueueing;
  stuOrder.m_ssSymbol = symbol.getValue();
  stuOrder.m_cSide = side.getValue();
  stuOrder.m_iOrderQty = (int)orderQty.getValue();
  stuOrder.m_dPrice = price.getValue();
  stuOrder.m_iLeavesQty = (int)orderQty.getValue();
  stuOrder.m_iCumQty = 0;

	memset(&stuInputOrder, 0, sizeof(CThostFtdcInputOrderField));
	//由于不能确保送入的ClOrdID(11)严格递增，在这里递增生成一个报单引用并做关联
	std::string ssOrderRef = format(ssOrderRefFormat, ++m_acOrderRef);
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,ClOrdID:%s", ssOrderRef.c_str(), clOrdID.getValue().c_str());
	if(!AddOrderRefClOrdID(ssOrderRef, clOrdID.getValue(), ssErrMsg)) return false;
  stuOrder.m_ssOrderRef = ssOrderRef;
  
	strncpy(stuInputOrder.UserID, m_stuTdParam.m_ssUserId.c_str(), sizeof(stuInputOrder.UserID));
	strncpy(stuInputOrder.InvestorID, ssRealAccount.c_str(), sizeof(stuInputOrder.InvestorID));
	strncpy(stuInputOrder.OrderRef, ssOrderRef.c_str(), sizeof(stuInputOrder.OrderRef));
	strncpy(
		stuInputOrder.InstrumentID, 
		m_enSymbolType == Convert::Original ? 
		symbol.getValue().c_str() : m_stuTdParam.m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
		sizeof(stuInputOrder.InstrumentID));

	stuInputOrder.VolumeTotalOriginal = (int)orderQty.getValue();
	stuInputOrder.OrderPriceType = m_stuTdParam.m_pSgitCtx->CvtDict(ordType.getField(), ordType.getValue(), Convert::Sgit);
	stuInputOrder.LimitPrice = price.getValue();
	stuInputOrder.Direction = m_stuTdParam.m_pSgitCtx->CvtDict(side.getField(), side.getValue(), Convert::Sgit);
	stuInputOrder.CombOffsetFlag[0] = m_stuTdParam.m_pSgitCtx->CvtDict(openClose.getField(), openClose.getValue(), Convert::Sgit);

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

  return true;
}

bool CSgitTdSpi::Cvt(const FIX42::OrderCancelRequest& oOrderCancel, CThostFtdcInputOrderActionField& stuInputOrderAction, std::string& ssErrMsg)
{
  //两种撤单组合：1.OrderSysID+ExchangeID 2.OrderRef+UserID+InstrumentID

  //本地号
  FIX::ClOrdID clOrdID;
  //被撤单本地号
  FIX::OrigClOrdID origClOrdID;
  //被撤单系统报单号
  FIX::OrderID orderID;
  FIX::SecurityExchange securityExchange;
  FIX::Symbol symbol;

  oOrderCancel.get(clOrdID);
  oOrderCancel.get(origClOrdID);
  oOrderCancel.getIfSet(orderID);
  oOrderCancel.getIfSet(securityExchange);
  oOrderCancel.get(symbol);

  std::string ssOrderRef = "";
  if(!GetOrderRef(origClOrdID.getValue(), ssOrderRef))
  {
    ssErrMsg = "Can not GetOrderRef by origClOrdID:" + origClOrdID.getValue();
    LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return false;
  }

	//将撤单请求ID赋值到原有委托结构体中
  Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(ssOrderRef);
  if (spStuOrder.isNull())
  {
    ssErrMsg = "Can not find orderRef:" + ssOrderRef + " in cache";
    LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return false;
  }
  spStuOrder->m_ssCancelClOrdID = origClOrdID.getValue();

  memset(&stuInputOrderAction, 0, sizeof(CThostFtdcInputOrderActionField));
  stuInputOrderAction.OrderActionRef = ++m_acOrderRef;
  std::string ssOrderActionRef = format(ssOrderRefFormat, stuInputOrderAction.OrderActionRef);
  LOG(INFO_LOG_LEVEL, "OrderActionRef:%s,ClOrdID:%s", ssOrderActionRef.c_str(), clOrdID.getValue().c_str());
  if(!AddOrderRefClOrdID(ssOrderActionRef, clOrdID.getValue(), ssErrMsg)) return false;

  //1.OrderSysID+ExchangeID
  if (!orderID.getValue().empty() && !securityExchange.getValue().empty())
  {
    strncpy(stuInputOrderAction.OrderSysID, orderID.getValue().c_str(), sizeof(stuInputOrderAction.OrderSysID));
    strncpy(
      stuInputOrderAction.ExchangeID, 
      m_enSymbolType == Convert::Original ? 
      securityExchange.getValue().c_str() : m_stuTdParam.m_pSgitCtx->CvtExchange(securityExchange.getValue(), Convert::Original).c_str(),
      sizeof(stuInputOrderAction.ExchangeID));
  }
  //2.OrderRef+UserID+InstrumentID
  else
  {
    strncpy(stuInputOrderAction.OrderRef, ssOrderRef.c_str(), sizeof(stuInputOrderAction.OrderRef));
    strncpy(stuInputOrderAction.UserID, m_stuTdParam.m_ssUserId.c_str(), sizeof(stuInputOrderAction.UserID));
    strncpy(
      stuInputOrderAction.InstrumentID, 
      m_enSymbolType == Convert::Original ? 
      symbol.getValue().c_str() : m_stuTdParam.m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
      sizeof(stuInputOrderAction.InstrumentID));
  }

  stuInputOrderAction.RequestID = m_acRequestId++;

  return true;
}

void CSgitTdSpi::ReqQryOrder(const FIX42::OrderStatusRequest& oOrderStatusRequest)
{
  FIX::Symbol symbol;
  FIX::ClOrdID clOrdID;
  FIX::OrderID orderID;
  
  oOrderStatusRequest.get(symbol);
  oOrderStatusRequest.get(clOrdID);
  oOrderStatusRequest.get(orderID);

  CThostFtdcQryOrderField stuQryOrder;
  memset(&stuQryOrder, 0, sizeof(CThostFtdcQryOrderField));

  strncpy(stuQryOrder.InvestorID, m_stuTdParam.m_pSgitCtx->GetRealAccont(oOrderStatusRequest).c_str(), sizeof(stuQryOrder.InvestorID));
  strncpy(
    stuQryOrder.InstrumentID, 
    m_enSymbolType == Convert::Original ? 
    symbol.getValue().c_str() : m_stuTdParam.m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
    sizeof(stuQryOrder.InstrumentID));
  strncpy(stuQryOrder.OrderSysID, orderID.getValue().c_str(), sizeof(stuQryOrder.OrderSysID));

  int iRet = m_stuTdParam.m_pTdReqApi->ReqQryOrder(&stuQryOrder, m_acRequestId++);
	if (iRet != 0)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to call api:ReqOrderInsert,iRet:%d", iRet);
	}
}

void CSgitTdSpi::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pOrder || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,OrderStatus:%c,VolumeTraded:%d,VolumeLeave:%d",
    pOrder->OrderRef, pOrder->OrderSysID, pOrder->OrderStatus, pOrder->VolumeTraded, pOrder->VolumeTotal);

  Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(pOrder->OrderRef);
  if (spStuOrder.isNull())
  {
    LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", pOrder->OrderRef);
    return;
  }

  spStuOrder->Update(*pOrder);

  int iErrCode = pRspInfo->ErrorID;
  std::string ssErrMsg = pRspInfo->ErrorMsg;

  //订单被拒绝
  if (iErrCode == 0 && (pOrder->OrderSubmitStatus == THOST_FTDC_OSS_InsertRejected 
    || pOrder->OrderSubmitStatus == THOST_FTDC_OSS_CancelRejected))
  {
    iErrCode = -1;
    ssErrMsg = "Reject by Exchange";
  }
  //撤单回报
  else if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled)
  {
    //撤单的回报中应把订单剩余数量置0
    spStuOrder->m_iLeavesQty = 0;
  }

  SendExecutionReport(*spStuOrder, iErrCode, ssErrMsg);
}

void CSgitTdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  LOG(ERROR_LOG_LEVEL, "ErrorID:%d,ErrorMsg:%s,RequestID:%d", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
}

void CSgitTdSpi::OnMessage(const FIX::Message& oMsg)
{
  FIX::MsgType msgType;
  oMsg.getHeader().getField(msgType);

  if (msgType == FIX::MsgType_NewOrderSingle)
  {
    ReqOrderInsert((const FIX42::NewOrderSingle&) oMsg);
  }
  else if(msgType == FIX::MsgType_OrderCancelRequest)
  {
    ReqOrderAction((const FIX42::NewOrderSingle&) oMsg);
  }
  else if(msgType == FIX::MsgType_OrderStatusRequest)
  {
    ReqQryOrder((const FIX42::OrderStatusRequest&) oMsg);
  }
}

double CSgitTdSpi::STUOrder::AvgPx() const
{
  double dTurnover = 0.0;
  int iTotalVolume = 0;
  for (std::vector<STUTradeRec>::const_iterator cit = m_vTradeRec.begin(); cit != m_vTradeRec.end(); cit++)
  {
    dTurnover += cit->m_dMatchPrice * cit->m_iMatchVolume;
    iTotalVolume += cit->m_iMatchVolume;
  }

  if (iTotalVolume == 0) return 0.0;

  return dTurnover / iTotalVolume;
}

void CSgitTdSpi::STUOrder::Update(const CThostFtdcInputOrderField& oInputOrder)
{
  if(m_ssOrderID.empty() && strlen(oInputOrder.OrderSysID) > 0)
  {
    m_ssOrderID = oInputOrder.OrderSysID;
  }
}

void CSgitTdSpi::STUOrder::Update(const CThostFtdcOrderField& oOrder)
{
  if (m_ssOrderID.empty() && strlen(oOrder.OrderSysID) > 0)
  {
    m_ssOrderID = oOrder.OrderSysID;
  }

  m_cOrderStatus = oOrder.OrderStatus;
  //m_iLeavesQty = oOrder.VolumeTotal;
  //m_iCumQty = m_iOrderQty - m_iLeavesQty;
}

void CSgitTdSpi::STUOrder::Update(const CThostFtdcTradeField& oTrade)
{
  m_vTradeRec.push_back(STUTradeRec(oTrade.Price, oTrade.Volume));
  m_iCumQty += oTrade.Volume;
  m_iLeavesQty = m_iOrderQty - m_iCumQty;
}

CSgitTdSpi::STUOrder::STUOrder()
	: m_ssAccout("")
  , m_ssOrderRef("")
  , m_ssOrderID("")
	, m_ssClOrdID("")
	, m_ssCancelClOrdID("")
	, m_cOrderStatus(THOST_FTDC_OST_NoTradeQueueing)
	, m_ssSymbol("")
	, m_cSide('*')
  , m_iOrderQty(0)
  , m_dPrice(0.0)
	, m_iLeavesQty(0)
	, m_iCumQty(0)
{
	m_vTradeRec.clear();
}

CSgitTdSpi::STUTradeRec::STUTradeRec()
  : m_dMatchPrice(0.0)
  , m_iMatchVolume(0)
{}

CSgitTdSpi::STUTradeRec::STUTradeRec(double dPrice, int iVolume)
  : m_dMatchPrice(dPrice)
  , m_iMatchVolume(iVolume)
{}

CSgitTdSpiHubTran::CSgitTdSpiHubTran(const STUTdParam &stuTdParam)
  : CSgitTdSpi(stuTdParam)
{

}

CSgitTdSpiHubTran::~CSgitTdSpiHubTran()
{

}

bool CSgitTdSpiHubTran::Init()
{
  AutoPtr<IniFileConfiguration> apSgitConf =  new IniFileConfiguration(m_stuTdParam.m_ssSgitCfgPath);

  std::string ssSessionIDTmp = Poco::replace(m_stuTdParam.m_ssSessionID, ".", "#");
  LOG(INFO_LOG_LEVEL, "SessionID:%s, ssSessionIDTmp:%s", m_stuTdParam.m_ssSessionID.c_str(), ssSessionIDTmp.c_str());
  
  AbstractConfiguration::Keys kProp;
  apSgitConf->keys(kProp);

  for (AbstractConfiguration::Keys::iterator itProp = kProp.begin(); itProp != kProp.end(); itProp++)
  {
    if (strncmp(itProp->c_str(), G_FIX42.c_str(), G_FIX42.size()) == 0)
    {
      if(m_apSgitConf->hasProperty(*itProp + ".SymbolType"))
      {
        ScopedWriteRWLock scopeWriteLock(m_rwFixUser2CvtType);
        m_mapFixUser2CvtType[*itProp] = (Convert::EnCvtType) m_apSgitConf->getInt(*itProp + ".SymbolType");
      }
      LOG(INFO_LOG_LEVEL, "itProp:%s", itProp->c_str());
    }
  }

  return true;
}



CSgitTdSpiDirect::CSgitTdSpiDirect(const STUTdParam &stuTdParam)
  : CSgitTdSpi(stuTdParam)
{

}

CSgitTdSpiDirect::~CSgitTdSpiDirect()
{

}

bool CSgitTdSpiDirect::Init()
{
  return true;
}


