#include "SgitTdSpi.h"
#include "SgitContext.h"
#include "Log.h"
#include "Toolkit.h"

#include "Poco/Format.h"
#include "Poco/UUIDGenerator.h"
#include "Poco/StringTokenizer.h"

#include "quickfix/Session.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix42/OrderCancelReject.h"

CSgitTdSpi::CSgitTdSpi(const STUTdParam &stuTdParam)
  : m_stuTdParam(stuTdParam)
	, m_acRequestId(0)
	, m_acOrderRef(0)
{
	
}

CSgitTdSpi::~CSgitTdSpi()
{
	m_fOrderRef2ClOrdID.close();
  //释放Api内存
  if( m_stuTdParam.m_pTdReqApi )
  {
    m_stuTdParam.m_pTdReqApi->RegisterSpi(nullptr);
    m_stuTdParam.m_pTdReqApi->Release();
    m_stuTdParam.m_pTdReqApi = NULL;
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

	m_acOrderRef = MAX(m_acOrderRef, atoi(pRspUserLogin->MaxOrderRef));

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

	m_mapOrderRef2Order[stuOrder.m_ssOrderRef] = stuOrder;

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
  
	STUOrder stuOrder;
	if(!GetStuOrder(pInputOrder->OrderRef, stuOrder)) return;

  stuOrder.Update(*pInputOrder);

	SendExecutionReport(stuOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
}

void CSgitTdSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pInputOrderAction || !pRspInfo) return;
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,VolumeChange:%d,ErrorID:%d,ErrorMsg:%s", 
    pInputOrderAction->OrderRef, pInputOrderAction->OrderSysID, pInputOrderAction->VolumeChange, 
		pRspInfo->ErrorID, pRspInfo->ErrorMsg);

	STUOrder stuOrder;
	if(!GetStuOrder(pInputOrderAction->OrderRef, stuOrder)) return;

	if(pRspInfo->ErrorID != 0)
	{
		SendOrderCancelReject(stuOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
	}
	else
	{
		SendExecutionReport(stuOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg, true);
	}
}

//撤单的情况在此回复执行回报，其余情况只用于更新订单的最新状态参数
void CSgitTdSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	if (!pOrder) return;
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,OrderStatus:%c,VolumeTraded:%d,VolumeLeave:%d",
     pOrder->OrderRef, pOrder->OrderSysID, pOrder->OrderStatus, pOrder->VolumeTraded, pOrder->VolumeTotal);

	STUOrder stuOrder;
  UpsertOrder(*pOrder, stuOrder);;

  //订单被拒绝
  if (pOrder->OrderSubmitStatus == THOST_FTDC_OSS_InsertRejected 
    || pOrder->OrderSubmitStatus == THOST_FTDC_OSS_CancelRejected)
  {
    SendExecutionReport(stuOrder, -1, "Reject by Exchange");
  }
  //撤单回报
  else if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled)
  {
    //撤单的回报中应把订单剩余数量置0
    stuOrder.m_iLeavesQty = 0;
    SendExecutionReport(stuOrder);
  }
}


//收到成交，回复执行回报
void CSgitTdSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	if (!pTrade) return;
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,TradeID:%s,Price:%f,Volume:%d", 
    pTrade->OrderRef, pTrade->OrderSysID, pTrade->TradeID, pTrade->Price, pTrade->Volume);

	STUOrder stuOrder;
	if(!GetStuOrder(pTrade->OrderRef, stuOrder)) return;

  stuOrder.Update(*pTrade);
  SendExecutionReport(stuOrder);
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

void CSgitTdSpi::SendExecutionReport(const STUOrder& stuOrder, int iErrCode /*= 0*/, const std::string& ssErrMsg /*= ""*/, bool bIsPendingCancel /*= false*/)
{
  std::string& ssUUid = CToolkit::GetUuid();
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,ClOrdID:%s,OrderID:%s,uuid:%s", 
    stuOrder.m_ssOrderRef.c_str(), stuOrder.m_ssClOrdID.c_str(), stuOrder.m_ssOrderID.c_str(), ssUUid.c_str());

  FIX42::ExecutionReport executionReport = FIX42::ExecutionReport();
  executionReport.set(FIX::ClOrdID(stuOrder.m_ssClOrdID));
  executionReport.set(FIX::OrderID(stuOrder.m_ssOrderID.empty() ? " " : stuOrder.m_ssOrderID));
  executionReport.set(FIX::ExecID(ssUUid));
  executionReport.set(FIX::Symbol(stuOrder.m_ssSymbol));
  executionReport.set(FIX::Price(stuOrder.m_dPrice));
  executionReport.set(FIX::OrderQty(stuOrder.m_iOrderQty));
  executionReport.set(FIX::Side(stuOrder.m_cSide));
  executionReport.set(FIX::LeavesQty(stuOrder.m_iLeavesQty));
  executionReport.set(FIX::CumQty(stuOrder.m_iCumQty));
  executionReport.set(FIX::AvgPx(stuOrder.AvgPx()));
  executionReport.set(FIX::ExecTransType(FIX::ExecTransType_NEW));

  if (bIsPendingCancel || stuOrder.m_mapTradeRec.size() < 1)
  {
    executionReport.set(FIX::LastPx(0));
    executionReport.set(FIX::LastShares(0));
  }
  else
  {
    executionReport.set(FIX::LastPx(stuOrder.m_mapTradeRec.rend()->second.m_dMatchPrice));
    executionReport.set(FIX::LastShares(stuOrder.m_mapTradeRec.rend()->second.m_iMatchVolume));
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
		executionReport.set(FIX::OrdStatus(stuOrder.m_cOrderStatus));
		executionReport.set(FIX::ExecType(stuOrder.m_cOrderStatus));
  }

  //回执行回报时，优先送入收到的账户，如果没有收到，找到配置中配置的账户别名，如果也没有找到，只能使用真实账户
	std::string ssAccount = stuOrder.m_ssRealAccount;
	if (!stuOrder.m_ssRecvAccount.empty())
	{
		ssAccount = stuOrder.m_ssRecvAccount;
	}
	else if (m_mapReal2AliasAcct.count(stuOrder.m_ssRealAccount) > 0)
	{
		ssAccount = m_mapReal2AliasAcct[stuOrder.m_ssRealAccount];
	}
	executionReport.set(FIX::Account(ssAccount));

	SendByRealAcct(stuOrder.m_ssRealAccount, executionReport);
}

void CSgitTdSpi::SendExecutionReport(const std::string& ssOrderRef, int iErrCode /*= 0*/, const std::string& ssErrMsg /*= ""*/)
{
	STUOrder stuOrder;
	if(!GetStuOrder(ssOrderRef, stuOrder)) return;

  return SendExecutionReport(stuOrder, iErrCode, ssErrMsg);
}

void CSgitTdSpi::SendExecutionReport(const FIX42::OrderStatusRequest& oOrderStatusRequest, int iErrCode, const std::string& ssErrMsg)
{
  FIX::ClOrdID clOrdID;
  FIX::OrderID orderID;
  //FIX::ExecID aExecID;
  //FIX::ExecTransType aExecTransType;
  //FIX::ExecType aExecType;
  //FIX::OrdStatus aOrdStatus;
  FIX::Symbol symbol;
  FIX::Side side;
  //FIX::LeavesQty aLeavesQty;
  //FIX::CumQty aCumQty;
  //FIX::AvgPx aAvgPx;
  FIX::Account account;

  oOrderStatusRequest.get(clOrdID);
  oOrderStatusRequest.getIfSet(orderID);
  oOrderStatusRequest.get(symbol);
  oOrderStatusRequest.get(side);
  oOrderStatusRequest.getIfSet(account);

  FIX42::ExecutionReport executionReport = FIX42::ExecutionReport(
    FIX::OrderID(orderID.getValue().empty() ? " " : orderID.getValue()),
    FIX::ExecID(CToolkit::GetUuid()),
    FIX::ExecTransType(FIX::ExecTransType_STATUS),
    FIX::ExecType(FIX::ExecType_NEW),
    FIX::OrdStatus(FIX::OrdStatus_NEW),
    FIX::Symbol(symbol),
    FIX::Side(side),
    FIX::LeavesQty(0),
    FIX::CumQty(0),
    FIX::AvgPx(0.0));

  executionReport.set(FIX::OrdRejReason(FIX::OrdRejReason_BROKER_OPTION));
  executionReport.set(FIX::Text(format("errID:%d,errMsg:%s", iErrCode, ssErrMsg)));

  CToolkit::Send(oOrderStatusRequest, executionReport);
}

void CSgitTdSpi::SendOrderCancelReject(const std::string& ssOrderRef, int iErrCode, const std::string& ssErrMsg)
{
	STUOrder stuOrder;
	if(!GetStuOrder(ssOrderRef, stuOrder)) return;

	return SendOrderCancelReject(stuOrder, iErrCode, ssErrMsg);
}

void CSgitTdSpi::SendOrderCancelReject(const STUOrder& stuOrder, int iErrCode, const std::string& ssErrMsg)
{
	FIX42::OrderCancelReject orderCancelReject = FIX42::OrderCancelReject(
		FIX::OrderID(stuOrder.m_ssOrderID),
		FIX::ClOrdID(stuOrder.m_ssCancelClOrdID),
		FIX::OrigClOrdID(stuOrder.m_ssClOrdID),
		FIX::OrdStatus(m_stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, stuOrder.m_cOrderStatus, Convert::Fix)),
		FIX::CxlRejResponseTo(FIX::CxlRejResponseTo_ORDER_CANCEL_REQUEST));

	orderCancelReject.set(FIX::Text(ssErrMsg));

	SendByRealAcct(stuOrder.m_ssRealAccount, orderCancelReject);
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

  CToolkit::Send(oOrderCancel, orderCancelReject);
}

//bool CSgitTradeSpi::GetClOrdID(const std::string& ssOrderRef, std::string& ssClOrdID)
//{
//	return Get(m_chOrderRef2ClOrderID, ssOrderRef, ssClOrdID);
//}

bool CSgitTdSpi::GetOrderRef(const std::string& ssClOrdID, std::string& ssOrderRef)
{
	return Get(m_mapClOrdID2OrderRef, ssClOrdID, ssOrderRef);
}

bool CSgitTdSpi::Get( std::map<std::string, std::string> &oMap, const std::string& ssKey, std::string& ssValue)
{
	std::map<std::string, std::string>::const_iterator cit = oMap.find(ssKey);
	if (cit == oMap.end())
	{
		LOG(ERROR_LOG_LEVEL, "Can not find key:%s in map", ssKey.c_str());
		return false;
	}

	ssValue = cit->second;
	return true;
}

bool CSgitTdSpi::AddOrderRefClOrdID(const std::string& ssOrderRef, const std::string& ssClOrdID, std::string& ssErrMsg)
{
	if(m_mapOrderRef2ClOrdID.count(ssOrderRef) > 0)
	{
    ssErrMsg = "OrderRef:" + ssOrderRef + " is duplicate";
		LOG(WARN_LOG_LEVEL, ssErrMsg.c_str());
    return false;
	}
  m_mapOrderRef2ClOrdID[ssOrderRef] = ssClOrdID;
	

	if(m_mapClOrdID2OrderRef.count(ssClOrdID) > 0)
	{
    ssErrMsg = "ClOrdID:" + ssClOrdID + " is duplicate";
    LOG(WARN_LOG_LEVEL, ssErrMsg.c_str());
		return false;
	}
	m_mapClOrdID2OrderRef[ssClOrdID] = ssOrderRef;

  return true;
}

bool CSgitTdSpi::Cvt(const FIX42::NewOrderSingle& oNewOrderSingle, CThostFtdcInputOrderField& stuInputOrder, STUOrder& stuOrder, std::string& ssErrMsg)
{
	FIX::Account account;
	FIX::ClOrdID clOrdID;
	FIX::Symbol symbol;
	FIX::OrderQty orderQty;
	FIX::OrdType ordType;
	FIX::Price price;
	FIX::Side side;
	FIX::OpenClose openClose;
	

	oNewOrderSingle.get(account);
	oNewOrderSingle.get(clOrdID);
	oNewOrderSingle.get(symbol);
	oNewOrderSingle.get(orderQty);
	oNewOrderSingle.get(ordType);
	oNewOrderSingle.get(price);
	oNewOrderSingle.get(side);
	oNewOrderSingle.get(openClose);

  //STUOrder
	stuOrder.m_ssRecvAccount = account.getValue();
	stuOrder.m_ssRealAccount = GetRealAccont(oNewOrderSingle);
  stuOrder.m_ssClOrdID = clOrdID.getValue();
  stuOrder.m_cOrderStatus = FIX::OrdStatus_PENDING_NEW;
  stuOrder.m_ssSymbol = symbol.getValue();
  stuOrder.m_cSide = side.getValue();
  stuOrder.m_iOrderQty = (int)orderQty.getValue();
  stuOrder.m_dPrice = price.getValue();
  stuOrder.m_iLeavesQty = (int)orderQty.getValue();
  stuOrder.m_iCumQty = 0;
	
	//由于不能确保送入的ClOrdID(11)严格递增，在这里递增生成一个报单引用并做关联
	std::string ssOrderRef = format(ssOrderRefFormat, ++m_acOrderRef);
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,ClOrdID:%s", ssOrderRef.c_str(), clOrdID.getValue().c_str());
	if(!AddOrderRefClOrdID(ssOrderRef, clOrdID.getValue(), ssErrMsg)) return false;
	WriteDatFile(ssOrderRef, clOrdID.getValue());

  stuOrder.m_ssOrderRef = ssOrderRef;
  
	Convert::EnCvtType enSymbolType = Convert::Unknow;
	if(!CheckIdSource(oNewOrderSingle, enSymbolType, ssErrMsg)) return false;

	memset(&stuInputOrder, 0, sizeof(CThostFtdcInputOrderField));
	strncpy(stuInputOrder.UserID, m_stuTdParam.m_ssUserId.c_str(), sizeof(stuInputOrder.UserID));
	strncpy(stuInputOrder.InvestorID, stuOrder.m_ssRealAccount.c_str(), sizeof(stuInputOrder.InvestorID));
	strncpy(stuInputOrder.OrderRef, ssOrderRef.c_str(), sizeof(stuInputOrder.OrderRef));
	strncpy(
		stuInputOrder.InstrumentID, 
		enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
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
	STUOrder stuOrder;
	if(!GetStuOrder(ssOrderRef, stuOrder)) return false;
  stuOrder.m_ssCancelClOrdID = origClOrdID.getValue();

  memset(&stuInputOrderAction, 0, sizeof(CThostFtdcInputOrderActionField));
  stuInputOrderAction.OrderActionRef = ++m_acOrderRef;
  std::string ssOrderActionRef = format(ssOrderRefFormat, stuInputOrderAction.OrderActionRef);
  LOG(INFO_LOG_LEVEL, "OrderActionRef:%s,ClOrdID:%s", ssOrderActionRef.c_str(), clOrdID.getValue().c_str());
  if(!AddOrderRefClOrdID(ssOrderActionRef, clOrdID.getValue(), ssErrMsg)) return false;


	Convert::EnCvtType enSymbolType = Convert::Unknow;
	if (!CheckIdSource(oOrderCancel, enSymbolType, ssErrMsg)) return false;

  //1.OrderSysID+ExchangeID
  if (!orderID.getValue().empty() && !securityExchange.getValue().empty())
  {
    strncpy(stuInputOrderAction.OrderSysID, orderID.getValue().c_str(), sizeof(stuInputOrderAction.OrderSysID));
		strncpy(
			stuInputOrderAction.ExchangeID,
			enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ?
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
			enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ?
			symbol.getValue().c_str() : m_stuTdParam.m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
			sizeof(stuInputOrderAction.InstrumentID));
  }

  stuInputOrderAction.RequestID = m_acRequestId++;

  return true;
}

void CSgitTdSpi::Cvt(const CThostFtdcOrderField &stuFtdcOrder, STUOrder &stuOrder)
{
	stuOrder.m_ssRealAccount = stuFtdcOrder.InvestorID;
	stuOrder.m_ssOrderRef = stuFtdcOrder.OrderRef;
	stuOrder.m_ssOrderID = stuFtdcOrder.OrderSysID;
	stuOrder.m_cOrderStatus = m_stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, stuFtdcOrder.OrderStatus, Convert::Fix);
	Convert::EnCvtType enSymbolType = GetSymbolType(stuOrder.m_ssRealAccount);
	stuOrder.m_ssSymbol = enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
		stuFtdcOrder.InstrumentID : m_stuTdParam.m_pSgitCtx->CvtSymbol(stuFtdcOrder.InstrumentID, enSymbolType);
	stuOrder.m_cSide = m_stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::Side, stuFtdcOrder.Direction, Convert::Fix);
	stuOrder.m_iOrderQty = stuFtdcOrder.VolumeTotalOriginal;
	stuOrder.m_dPrice = stuFtdcOrder.LimitPrice;
}

void CSgitTdSpi::ReqQryOrder(const FIX42::OrderStatusRequest& oOrderStatusRequest)
{
  FIX::ClOrdID clOrdID;
  FIX::Symbol symbol;
  FIX::OrderID orderID;
  FIX::Account account;
  
  oOrderStatusRequest.get(clOrdID);
  oOrderStatusRequest.get(symbol);
  oOrderStatusRequest.getIfSet(orderID);
  oOrderStatusRequest.getIfSet(account);

  CThostFtdcQryOrderField stuQryOrder;
  memset(&stuQryOrder, 0, sizeof(CThostFtdcQryOrderField));
 
  std::string ssOrderID = "", ssErrMsg = "";
  if (!orderID.getValue().empty())
  {
    ssOrderID = orderID.getValue();
  }
  else
  {
    std::string ssOrderRef = "";
    if(!GetOrderRef(clOrdID.getValue(), ssOrderRef))
    {
      ssErrMsg = "Can not GetOrderRef by clOrdID:" + clOrdID.getValue();
      LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
      return SendExecutionReport(oOrderStatusRequest, -1, ssErrMsg);
    }

    STUOrder stuOrder;
    if(!GetStuOrder(ssOrderRef, stuOrder))
    {
      ssErrMsg = "Can not GetStuOrder by clOrdID:" + clOrdID.getValue();
      LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
      return SendExecutionReport(oOrderStatusRequest, -1, ssErrMsg);
    }

    ssOrderID = stuOrder.m_ssOrderID;
  }
  if (!ssOrderID.empty())
  {
    strncpy(stuQryOrder.OrderSysID, ssOrderID.c_str(), sizeof(stuQryOrder.OrderSysID));
  }


  Convert::EnCvtType enSymbolType = m_stuTdParam.m_pSgitCtx->GetSymbolType(CToolkit::GetSessionKey(oOrderStatusRequest));
  strncpy(
    stuQryOrder.InstrumentID, 
    enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ?
    symbol.getValue().c_str() : m_stuTdParam.m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
    sizeof(stuQryOrder.InstrumentID));

  if (!account.getValue().empty())
  {
    strncpy(stuQryOrder.InvestorID, GetRealAccont(oOrderStatusRequest).c_str(), sizeof(stuQryOrder.InvestorID));
  }

  int iRet = m_stuTdParam.m_pTdReqApi->ReqQryOrder(&stuQryOrder, m_acRequestId++);
	if (iRet != 0)
	{
    ssErrMsg =  "Failed to call api:ReqQryOrder,iRet:" + iRet;
		LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return SendExecutionReport(oOrderStatusRequest, -1, ssErrMsg);
	}
}

void CSgitTdSpi::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pOrder || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,OrderStatus:%c,VolumeTraded:%d,VolumeLeave:%d",
    pOrder->OrderRef, pOrder->OrderSysID, pOrder->OrderStatus, pOrder->VolumeTraded, pOrder->VolumeTotal);

	STUOrder stuOrder;
	if(!GetStuOrder(pOrder->OrderRef, stuOrder)) return;

  stuOrder.Update(*pOrder, m_stuTdParam);

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
    stuOrder.m_iLeavesQty = 0;
  }

  SendExecutionReport(stuOrder, iErrCode, ssErrMsg);
}

void CSgitTdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  LOG(ERROR_LOG_LEVEL, "ErrorID:%d,ErrorMsg:%s,RequestID:%d", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
}

void CSgitTdSpi::OnMessage(const FIX::Message& oMsg, const FIX::SessionID& oSessionID)
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

bool CSgitTdSpi::Init()
{
	if (!LoadDatFile()) return false;

  if(!LoadConfig()) return false;

  return true;
}

bool CSgitTdSpi::LoadConfig()
{
  AutoPtr<IniFileConfiguration> apSgitConf =  new IniFileConfiguration(m_stuTdParam.m_ssSgitCfgPath);

  std::string ssSessionProp = CToolkit::SessionID2Prop(m_stuTdParam.m_ssSessionID);
  LOG(INFO_LOG_LEVEL, "SessionID:%s, ssSessionIDTmp:%s", m_stuTdParam.m_ssSessionID.c_str(), ssSessionProp.c_str());

  AbstractConfiguration::Keys kProp;
  apSgitConf->keys(kProp);

  for (AbstractConfiguration::Keys::iterator itProp = kProp.begin(); itProp != kProp.end(); itProp++)
  {
    if (strncmp(itProp->c_str(), ssSessionProp.c_str(), ssSessionProp.size()) == 0)
    {
      if (apSgitConf->hasProperty(*itProp + ".AccountAlias"))
        if(!LoadAcctAlias(apSgitConf, *itProp)) return false;

      if(!LoadConfig(apSgitConf, *itProp)) return false;
    }
  }

  return true;
}

bool CSgitTdSpi::LoadAcctAlias(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp)
{
  StringTokenizer stAccountAliasList(apSgitConf->getString(ssSessionProp + ".AccountAlias"), ";", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  std::string ssKey = "", ssRealAcct = "";
  for (StringTokenizer::Iterator it = stAccountAliasList.begin(); it != stAccountAliasList.end(); it++)
  {
    ssKey = CToolkit::SessionProp2ID(ssSessionProp) + "|" + *it;
    ssRealAcct = apSgitConf->getString(ssSessionProp + "." + *it);
    std::pair<std::map<std::string, std::string>::iterator, bool> ret;
		ret = m_mapAlias2RealAcct.insert(std::pair<std::string, std::string>(ssKey, ssRealAcct));
    if(!ret.second)
    {
      LOG(ERROR_LOG_LEVEL, "Alias account key:%s is dulplicated", ssKey.c_str());
      return false;
    }

		ret  = m_mapReal2AliasAcct.insert(std::pair<std::string, std::string>(ssRealAcct, *it));
		if (!ret.second)
		{
			LOG(ERROR_LOG_LEVEL, "Real account key:%s is dulplicated", ssRealAcct.c_str());
			return false;
		}
  }

  return true;
}

std::string CSgitTdSpi::GetRealAccont(const FIX::Message& oRecvMsg)
{
	FIX::Account account;
	oRecvMsg.getField(account);

	if (!CToolkit::IsAliasAcct(account.getValue())) return account.getValue();

	std::string ssKey = CToolkit::GetSessionKey(oRecvMsg) + "|" + account.getValue();
	std::map<std::string, std::string>::const_iterator cit = 
		m_mapAlias2RealAcct.find(ssKey);
	if(cit != m_mapAlias2RealAcct.end()) 
		return cit->second;
	else
	{
		LOG(ERROR_LOG_LEVEL, "Can not find real account by key:%s", ssKey.c_str());
		return "unknow";
	}
}

bool CSgitTdSpi::GetStuOrder(const std::string &ssOrderRef, STUOrder &stuOrder)
{
	std::map<std::string, STUOrder>::iterator it = m_mapOrderRef2Order.find(ssOrderRef);
	if (it == m_mapOrderRef2Order.end())
	{
		LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_mapOrderRef2Order", ssOrderRef.c_str());
		return false;
	}
	stuOrder = it->second;

	return true;
}

std::string CSgitTdSpi::GetOrderRefDatFileName()
{
	return m_stuTdParam.m_ssDataPath + Poco::replace(Poco::replace(Poco::replace(m_stuTdParam.m_ssSessionID, ">", "_"), ".", "_"), ":", "_") + "_ref.dat";
}

bool CSgitTdSpi::LoadDatFile()
{
	std::string ssFileName = GetOrderRefDatFileName();
	m_fOrderRef2ClOrdID.open(ssFileName, std::fstream::in | std::fstream::out | std::fstream::app);
	if (m_fOrderRef2ClOrdID.bad())
	{
		LOG(ERROR_LOG_LEVEL, "Failed to open file:%s", ssFileName.c_str());
		return false;
	}

	//加载OrderRef和ClOrdID
	string ssOrderRef = "", ssClOrdID = "", ssErrMsg = "";
	while(m_fOrderRef2ClOrdID >> ssOrderRef >> ssClOrdID)
	{
		LOG(INFO_LOG_LEVEL, "ssOrderRef:%s, ssClOrdID:%s", ssOrderRef.c_str(), ssClOrdID.c_str());
		if(!AddOrderRefClOrdID(ssOrderRef, ssClOrdID, ssErrMsg)) return false;
	}

	m_fOrderRef2ClOrdID.clear();

  if (m_mapOrderRef2ClOrdID.size() > 0)
  {
    std::map<std::string, std::string>::reverse_iterator rit = m_mapOrderRef2ClOrdID.rbegin();
    m_acOrderRef = MAX(atoi(rit->first.c_str()), m_acOrderRef);
  }

	return true;
}

void CSgitTdSpi::WriteDatFile(const std::string &ssOrderRef, const std::string &ssClOrdID)
{
	Poco::FastMutex::ScopedLock oScopedLock(m_fastMutexOrderRef2ClOrdID);

	m_fOrderRef2ClOrdID << ssOrderRef << " " << ssClOrdID << endl;
}

void CSgitTdSpi::UpsertOrder(const CThostFtdcOrderField &stuFtdcOrder, STUOrder &stuOrder)
{
  std::map<std::string, STUOrder>::iterator itFind = m_mapOrderRef2Order.find(stuFtdcOrder.OrderRef);
  if (itFind != m_mapOrderRef2Order.end())
  {
    itFind->second.Update(stuFtdcOrder, m_stuTdParam);
		stuOrder = itFind->second;
  }
  else
  {
		Cvt(stuFtdcOrder, stuOrder);
    m_mapOrderRef2Order[stuFtdcOrder.OrderRef] = stuOrder;
  }
}


bool CSgitTdSpi::CheckIdSource(const FIX::Message& oRecvMsg, Convert::EnCvtType &enSymbolType, std::string& ssErrMsg)
{
	FIX::IDSource idSource;
	oRecvMsg.getFieldIfSet(idSource);

	if(!idSource.getValue().empty())
	{
		enSymbolType = (Convert::EnCvtType)atoi(idSource.getValue().c_str());
		if (!CToolkit::CheckIfValid(enSymbolType, ssErrMsg)) return false;

    m_stuTdParam.m_pSgitCtx->UpdateSymbolType(CToolkit::GetSessionKey(oRecvMsg), enSymbolType);
	}
	else
	{
		enSymbolType = m_stuTdParam.m_pSgitCtx->GetSymbolType(CToolkit::GetSessionKey(oRecvMsg));
	}

	return true;
}

void CSgitTdSpi::SendByRealAcct(const std::string &ssRealAcct, FIX::Message& oMsg)
{
  Poco::SharedPtr<STUserInfo> spUserInfo = GetUserInfo(ssRealAcct);
  if (!spUserInfo)
  {
    LOG(ERROR_LOG_LEVEL, "Failed to GetUserInfo by real account:%s", ssRealAcct.c_str());
    return;
  }

  CToolkit::SetUserInfo(*spUserInfo, oMsg);

  try
  {
    FIX::Session::sendToTarget(oMsg, spUserInfo->m_oSessionID);
  }
  catch ( FIX::SessionNotFound& e) 
  {
    LOG(ERROR_LOG_LEVEL, "msg:%s, err:%s", oMsg.toString(), e.what());
  }
}

double CSgitTdSpi::STUOrder::AvgPx() const
{
  double dTurnover = 0.0;
  int iTotalVolume = 0;
	for (std::map<std::string, STUTradeRec>::const_iterator cit = m_mapTradeRec.begin(); cit != m_mapTradeRec.end(); cit++)
	{
		dTurnover += cit->second.m_dMatchPrice * cit->second.m_iMatchVolume;
		iTotalVolume += cit->second.m_iMatchVolume; 
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

void CSgitTdSpi::STUOrder::Update(const CThostFtdcOrderField& oOrder, const STUTdParam &stuTdParam)
{
  if (m_ssOrderID.empty() && strlen(oOrder.OrderSysID) > 0)
  {
    m_ssOrderID = oOrder.OrderSysID;
  }

	m_cOrderStatus = stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, oOrder.OrderStatus, Convert::Fix);
  //m_iLeavesQty = oOrder.VolumeTotal;
  //m_iCumQty = m_iOrderQty - m_iLeavesQty;
}

void CSgitTdSpi::STUOrder::Update(const CThostFtdcTradeField& oTrade)
{
  m_mapTradeRec[oTrade.TradeID] = STUTradeRec(oTrade.Price, oTrade.Volume);

  m_iCumQty += oTrade.Volume;
  m_iLeavesQty = m_iOrderQty - m_iCumQty;
}

CSgitTdSpi::STUOrder::STUOrder()
	: m_ssRealAccount("")
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

bool CSgitTdSpiHubTran::LoadConfig(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp)
{
  LOG(INFO_LOG_LEVEL, "ssSessionProp:%s", ssSessionProp.c_str());

  std::string ssAcctListProp = ssSessionProp + ".AccountList";
  if (!apSgitConf->hasProperty(ssAcctListProp))
  {
    LOG(ERROR_LOG_LEVEL, "Can not find property:%s", ssAcctListProp.c_str());
    return false;
  }

	Poco::SharedPtr<STUserInfo> spUserInfo(new STUserInfo());
	CToolkit::Convert2SessionIDBehalfCompID(ssSessionProp, spUserInfo->m_oSessionID, spUserInfo->m_ssOnBehalfOfCompID);

	if (apSgitConf->hasProperty(ssSessionProp + ".SymbolType"))
	{
    spUserInfo->m_enCvtType = (Convert::EnCvtType)apSgitConf->getInt(ssSessionProp + ".SymbolType");
	}

  m_stuTdParam.m_pSgitCtx->AddUserInfo(CToolkit::SessionProp2ID(ssSessionProp), spUserInfo);

  StringTokenizer stAccountList(apSgitConf->getString(ssAcctListProp), ";", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  std::string ssKey = "", ssRealAcct = "";
  for (StringTokenizer::Iterator it = stAccountList.begin(); it != stAccountList.end(); it++)
  {
		m_mapRealAcct2UserInfo[*it] = spUserInfo;
  }

  return true;
}

Convert::EnCvtType CSgitTdSpiHubTran::GetSymbolType(const std::string &ssRealAcct)
{
  std::map<std::string, Poco::SharedPtr<STUserInfo>>::const_iterator cit = m_mapRealAcct2UserInfo.find(ssRealAcct);
  if (cit != m_mapRealAcct2UserInfo.end())
  {
    return cit->second->m_enCvtType;
  }

  return Convert::Unknow;
}

Poco::SharedPtr<STUserInfo> CSgitTdSpiHubTran::GetUserInfo(const std::string &ssRealAcct)
{
  std::map<std::string, Poco::SharedPtr<STUserInfo>>::const_iterator cit = m_mapRealAcct2UserInfo.find(ssRealAcct);
  if (cit == m_mapRealAcct2UserInfo.end())
  {
    LOG(ERROR_LOG_LEVEL, "Can not find UserInfo by real account:%s", ssRealAcct.c_str());
    return NULL;
  }
  return cit->second;
}




CSgitTdSpiDirect::CSgitTdSpiDirect(const STUTdParam &stuTdParam)
  : CSgitTdSpi(stuTdParam)
  , m_spUserInfo(NULL)
{

}

CSgitTdSpiDirect::~CSgitTdSpiDirect()
{

}

bool CSgitTdSpiDirect::LoadConfig(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp)
{
  LOG(INFO_LOG_LEVEL, "ssProp:%s", ssSessionProp.c_str());

  if (apSgitConf->hasProperty(ssSessionProp + ".SymbolType"))
  {
    //m_stuTdParam.m_pSgitCtx->UpdateSymbolType(CToolkit::SessionProp2ID(ssSessionProp), (Convert::EnCvtType)apSgitConf->getInt(ssSessionProp + ".SymbolType"));
    //智能指针的同步修改
    m_spUserInfo->m_enCvtType = (Convert::EnCvtType)apSgitConf->getInt(ssSessionProp + ".SymbolType");
  }

  return true;
}

Convert::EnCvtType CSgitTdSpiDirect::GetSymbolType(const std::string &ssRealAcct)
{
  return m_spUserInfo->m_enCvtType;
}

bool CSgitTdSpiDirect::Init()
{
  m_spUserInfo = new STUserInfo();
  m_spUserInfo->m_oSessionID.fromString(m_stuTdParam.m_ssSessionID);
  m_stuTdParam.m_pSgitCtx->AddUserInfo(m_stuTdParam.m_ssSessionID, m_spUserInfo);

  return CSgitTdSpi::Init();
}

Poco::SharedPtr<STUserInfo> CSgitTdSpiDirect::GetUserInfo(const std::string &ssRealAcct)
{
  return m_spUserInfo;
}


