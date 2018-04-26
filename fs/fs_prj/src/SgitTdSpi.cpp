#include "SgitTdSpi.h"
#include "SgitContext.h"
#include "Log.h"
#include "Toolkit.h"

#include "Poco/Format.h"
#include "Poco/UUIDGenerator.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Data/Statement.h"
#include "Poco/Data/Session.h"

#include "quickfix/Session.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix42/OrderCancelReject.h"



using namespace Poco::Data;
using namespace Poco::Data::Keywords;

CSgitTdSpi::CSgitTdSpi(const STUTdParam &stuTdParam)
  : m_stuTdParam(stuTdParam)
	, m_acRequestId(0)
  , m_bNeedKeepLogin(false)
  , m_oEventConnected(false)
  , m_oEventLoginResp(true)
  , m_bLastLoginOk(false)
  , m_ssLoginRespErrMsg("")
  , m_acOrderRef(0)
  , m_iCurReqID(0)
{
  m_vBuffer.clear();
}

CSgitTdSpi::~CSgitTdSpi()
{
  //释放Api内存
  if( m_stuTdParam.m_pTdReqApi )
  {
    m_stuTdParam.m_pTdReqApi->RegisterSpi(NULL);
    m_stuTdParam.m_pTdReqApi->Release();
    m_stuTdParam.m_pTdReqApi = NULL;
  }
}

void CSgitTdSpi::OnFrontConnected()
{
  m_oEventConnected.set();
  
  if (m_bNeedKeepLogin && m_bLastLoginOk)
  {
    CThostFtdcReqUserLoginField stuLogin;
    memset(&stuLogin, 0, sizeof(CThostFtdcReqUserLoginField));
    strncpy(stuLogin.UserID, m_ssUserID.c_str(), sizeof(stuLogin.UserID));
    strncpy(stuLogin.Password, m_ssPassword.c_str(), sizeof(stuLogin.Password));
    m_stuTdParam.m_pTdReqApi->ReqUserLogin(&stuLogin, m_acRequestId++);

    LOG(INFO_LOG_LEVEL, "ReqUserLogin userID:%s", m_ssUserID.c_str());
  }
  //如果之前的账号登录成功，自动进行登录（保活），如果登录不成功（包含自动登出）则不管
}

void CSgitTdSpi::OnFrontDisconnected(int nReason)
{
	m_oEventConnected.reset();

	LOG(INFO_LOG_LEVEL, "%d", nReason);
}

void CSgitTdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, 
  CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  if (!pRspUserLogin || !pRspInfo) return;

  LOG(INFO_LOG_LEVEL, "userID:%s,MaxOrderRef:%s,errID:%d,errMsg:%s", 
    pRspUserLogin->UserID, pRspUserLogin->MaxOrderRef, pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  //登录成功
  if (!pRspInfo->ErrorID)
  {
    m_acOrderRef = MAX(m_acOrderRef, GetMaxOrderRefInDB());
    m_acOrderRef = MAX(m_acOrderRef, atoi(pRspUserLogin->MaxOrderRef));

    LOG(INFO_LOG_LEVEL, "m_acOrderRef set to:%d", m_acOrderRef.value());

    CThostFtdcSettlementInfoConfirmField stuConfirm;
    memset(&stuConfirm, 0, sizeof(CThostFtdcSettlementInfoConfirmField));
    strncpy(stuConfirm.InvestorID, pRspUserLogin->UserID, sizeof(stuConfirm.InvestorID));
    strncpy(stuConfirm.BrokerID, pRspUserLogin->BrokerID, sizeof(stuConfirm.BrokerID));

    m_stuTdParam.m_pTdReqApi->ReqSettlementInfoConfirm(&stuConfirm, m_acRequestId++);
  }

  m_bLastLoginOk = !pRspInfo->ErrorID;
  m_iLoginRespErrID = pRspInfo->ErrorID;
  m_ssLoginRespErrMsg = pRspInfo->ErrorMsg;

  m_oEventLoginResp.set();
}

bool CSgitTdSpi::ReqOrderInsert(const FIX42::NewOrderSingle& oNewOrderSingle, std::string& ssErrMsg)
{
	CThostFtdcInputOrderField stuInputOrder;
  Order oOrde;
	if(!Cvt(oNewOrderSingle, stuInputOrder, oOrde, ssErrMsg))
  {
    return SendExecutionReport(oNewOrderSingle, -1, ssErrMsg);
  }

	int iRet = m_stuTdParam.m_pTdReqApi->ReqOrderInsert(&stuInputOrder, stuInputOrder.RequestID);
	if (iRet != 0)
	{
    Poco::format(ssErrMsg, "Failed to call api:ReqOrderInsert,iRet:%d", iRet);
		LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return SendExecutionReport(oNewOrderSingle, iRet, ssErrMsg);
	}

  LOG(INFO_LOG_LEVEL, "Success call api:ReqOrderInsert,InvestorID:%s,InstrumentID:%s,OrderRef:%s", 
    stuInputOrder.InvestorID, stuInputOrder.InstrumentID, stuInputOrder.OrderRef);

  return true;
}

bool CSgitTdSpi::ReqOrderAction(const FIX42::OrderCancelRequest& oOrderCancel, std::string& ssErrMsg)
{
  CThostFtdcInputOrderActionField stuInputOrderAction;
	if(!Cvt(oOrderCancel, stuInputOrderAction, ssErrMsg))
  {
    return SendOrderCancelReject(oOrderCancel, ssErrMsg);
  }

  int iRet = m_stuTdParam.m_pTdReqApi->ReqOrderAction(&stuInputOrderAction, stuInputOrderAction.RequestID);
	if (iRet != 0)
	{
    Poco::format(ssErrMsg, "Failed to call api:ReqOrderAction,iRet:%d", iRet);
		LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
		return SendOrderCancelReject(oOrderCancel, ssErrMsg);
	}

  return true;
}


void CSgitTdSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pInputOrder || !pRspInfo) return;
	LOG(INFO_LOG_LEVEL, "UserID:%s,OrderRef:%s,OrderSysID:%s,ExchangeID:%s,ErrorID:%d, ErrorMsg:%s", 
		pInputOrder->UserID, pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID,pRspInfo->ErrorID, pRspInfo->ErrorMsg);
  
  Order oOrder;
	if(!GetOrderByOrderRef(pInputOrder->UserID, pInputOrder->OrderRef, oOrder)) return;

  oOrder.m_ssOrderSysID = pInputOrder->OrderSysID;
  
  //pending new
  oOrder.m_cOrderStatus = FIX::OrdStatus_PENDING_NEW;
  SendExecutionReport(oOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  //当有错误发生时，上一行代码已经回复了错误信息
  if(pRspInfo->ErrorID != 0) return;

  //new
  oOrder.m_cOrderStatus = FIX::OrdStatus_NEW;
	SendExecutionReport(oOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  UpdateOrder(oOrder);
}

void CSgitTdSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pInputOrderAction || !pRspInfo) return;
	LOG(INFO_LOG_LEVEL, "UserID:%s,OrderRef:%s,OrderSysID:%s,VolumeChange:%d,ErrorID:%d,ErrorMsg:%s", 
    pInputOrderAction->UserID, pInputOrderAction->OrderRef, pInputOrderAction->OrderSysID, pInputOrderAction->VolumeChange, 
		pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  Order oOrder;
  if(!GetOrderByOrderRef(pInputOrderAction->UserID, pInputOrderAction->OrderRef, oOrder)) return;

	if(pRspInfo->ErrorID != 0)
	{
		SendOrderCancelReject(oOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
	}
	else
	{
		SendExecutionReport(oOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg, true);
	}
}

//撤单的情况在此回复执行回报，其余情况只用于更新订单的最新状态参数
void CSgitTdSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	if (!pOrder) return;
  LOG(INFO_LOG_LEVEL, "UserID:%s,OrderRef:%s,OrderSysID:%s,OrderStatus:%c,VolumeTraded:%d,VolumeLeave:%d",
     pOrder->UserID, pOrder->OrderRef, pOrder->OrderSysID, pOrder->OrderStatus, pOrder->VolumeTraded, pOrder->VolumeTotal);

	Order oOrder;
  if(!GetOrderByOrderRef(pOrder->UserID, pOrder->OrderRef, oOrder)) return;

  oOrder.m_cOrderStatus = m_stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, pOrder->OrderStatus, Convert::Fix);
  oOrder.m_iCumQty = pOrder->VolumeTraded;
  oOrder.m_iLeavesQty = pOrder->VolumeTotal;
  
  //订单被拒绝
  if (pOrder->OrderSubmitStatus == THOST_FTDC_OSS_InsertRejected 
    || pOrder->OrderSubmitStatus == THOST_FTDC_OSS_CancelRejected)
  {
    SendExecutionReport(oOrder, -1, "Reject by Exchange");
  }
  //撤单回报
  else if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled)
  {
    SendExecutionReport(oOrder);
  }

  UpdateOrder(oOrder);
}


//收到成交，回复执行回报
void CSgitTdSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	if (!pTrade) return;
	LOG(INFO_LOG_LEVEL, "UserID:%s,OrderRef:%s,OrderSysID:%s,TradeID:%s,Price:%f,Volume:%d", 
    pTrade->UserID, pTrade->OrderRef, pTrade->OrderSysID, pTrade->TradeID, pTrade->Price, pTrade->Volume);

  Trade oTrade(*pTrade, m_ssUserID);
  if(!SaveTrade(oTrade)) return;

  SendExecutionReport(pTrade->UserID, pTrade->OrderRef);
}

void CSgitTdSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
	if (!pInputOrder || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "UserID:%s,OrderRef:%s,OrderSysID:%s,ExchangeID:%s,ErrorID:%d,ErrorMsg:%s", 
    pInputOrder->UserID, pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID,pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  SendExecutionReport(pInputOrder->UserID, pInputOrder->OrderRef, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
}

void CSgitTdSpi::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
	if (!pOrderAction || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "UserID:%s,OrderActionRef:%d,OrderRef:%s,OrderSysID:%s,ActionFlag:%c,VolumeChange:%d,ErrorID:%d,ErrorMsg:%s", 
    pOrderAction->UserID, pOrderAction->OrderActionRef, pOrderAction->OrderRef, pOrderAction->OrderSysID, pOrderAction->ActionFlag, 
    pOrderAction->VolumeChange, pRspInfo->ErrorID, pRspInfo->ErrorMsg);

	SendOrderCancelReject(pOrderAction->UserID, pOrderAction->OrderRef, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
}

bool CSgitTdSpi::SendExecutionReport(const Order& oOrder, int iErrCode /*= 0*/, const std::string& ssErrMsg /*= ""*/, bool bIsPendingCancel /*= false*/, bool bIsQueryRsp /*= false*/)
{
  std::vector<Trade> vTrade;
  if(!GetTradeByOrderRef(oOrder.m_ssOrderRef, vTrade)) return false;

  std::string ssUUid = CToolkit::GetUuid();
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,ClOrdID:%s,OrderID:%s,uuid:%s", 
    oOrder.m_ssOrderRef.c_str(), oOrder.m_ssClOrdID.c_str(), oOrder.m_ssOrderSysID.c_str(), ssUUid.c_str());

  FIX42::ExecutionReport executionReport = FIX42::ExecutionReport();
  executionReport.set(FIX::ClOrdID(oOrder.m_ssClOrdID));
  executionReport.set(FIX::OrderID(oOrder.m_ssOrderSysID.empty() ? CToolkit::GetUuid() : oOrder.m_ssOrderSysID));
  executionReport.set(FIX::ExecID(ssUUid));
  executionReport.set(FIX::Symbol(oOrder.m_ssRecvSymbol));
  executionReport.set(FIX::Price(oOrder.m_dPrice));
  executionReport.set(FIX::OrderQty(oOrder.m_iOrderQty));
  executionReport.set(FIX::Side(oOrder.m_cSide));
  executionReport.set(FIX::LeavesQty(oOrder.m_iLeavesQty));
  executionReport.set(FIX::CumQty(oOrder.m_iCumQty));
  executionReport.set(FIX::AvgPx(AvgPx(vTrade)));
  executionReport.set(FIX::ExecTransType(FIX::ExecTransType_NEW));

  if (bIsPendingCancel || vTrade.size() < 1)
  {
    executionReport.set(FIX::LastPx(0));
    executionReport.set(FIX::LastShares(0));
  }
  else
  {
    executionReport.set(FIX::LastPx(vTrade[0].m_dMatchPrice));
    executionReport.set(FIX::LastShares(vTrade[0].m_iMatchQty));
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
    executionReport.set(FIX::Text(ssErrMsg));
  }
  //正常情况
  else
  {
    executionReport.set(FIX::OrdStatus(oOrder.m_cOrderStatus));
    executionReport.set(FIX::ExecType(oOrder.m_cOrderStatus));
  }

  //pending cancel 和 canceled时 tag20为1
  if (bIsPendingCancel || oOrder.m_cOrderStatus == FIX::OrdStatus_CANCELED)
  {
    executionReport.set(FIX::ExecTransType(FIX::ExecTransType_CANCEL));
  }
  if (bIsQueryRsp)
  {
    executionReport.set(FIX::ExecTransType(FIX::ExecTransType_STATUS));
  }

  //回执行回报时，优先送入收到的账户，如果没有收到，找到配置中配置的账户别名，如果也没有找到，只能使用真实账户
  std::string ssAccount = oOrder.m_ssRealAccount;
  if (!oOrder.m_ssRecvAccount.empty())
  {
    ssAccount = oOrder.m_ssRecvAccount;
  }
  else if (m_mapReal2AliasAcct.count(oOrder.m_ssRealAccount) > 0)
  {
    ssAccount = m_mapReal2AliasAcct[oOrder.m_ssRealAccount];
  }
  executionReport.set(FIX::Account(ssAccount));

  return SendByRealAcct(oOrder.m_ssRealAccount, executionReport);
}

bool CSgitTdSpi::SendExecutionReport(const std::string &ssUserID, const std::string& ssOrderRef, int iErrCode /*= 0*/, const std::string& ssErrMsg /*= ""*/, bool bIsPendingCancel /*= false*/, bool bIsQueryRsp /*= false*/)
{
  Order oOrder;
  if (!GetOrderByOrderRef(ssUserID, ssOrderRef, oOrder)) return false;

  return SendExecutionReport(oOrder, iErrCode, ssErrMsg, bIsPendingCancel, bIsQueryRsp);
}

bool CSgitTdSpi::SendExecutionReport(const FIX42::OrderStatusRequest& oOrderStatusRequest, int iErrCode, const std::string& ssErrMsg)
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
    FIX::OrderID(orderID.getValue().empty() ? CToolkit::GetUuid() : orderID.getValue()),
    FIX::ExecID(CToolkit::GetUuid()),
    FIX::ExecTransType(FIX::ExecTransType_STATUS),
    FIX::ExecType(FIX::ExecType_NEW),
    FIX::OrdStatus(FIX::OrdStatus_NEW),
    FIX::Symbol(symbol),
    FIX::Side(side),
    FIX::LeavesQty(0),
    FIX::CumQty(0),
    FIX::AvgPx(0.0));

  if (!account.getValue().empty()) executionReport.set(FIX::Account(account));
  executionReport.set(FIX::ClOrdID(clOrdID));
  if (iErrCode != 0)
  {
    executionReport.set(FIX::OrdRejReason(FIX::OrdRejReason_BROKER_OPTION));
    executionReport.set(FIX::Text(ssErrMsg));
  }

  return CToolkit::Send(oOrderStatusRequest, executionReport);
}

bool CSgitTdSpi::SendExecutionReport(const FIX42::NewOrderSingle& oNewOrderSingle, int iErrCode /*= 0*/, const std::string& ssErrMsg /*= ""*/)
{
  FIX::ClOrdID clOrdID;
  FIX::Symbol symbol;
  FIX::Side side;
  FIX::Account account;

  oNewOrderSingle.get(clOrdID);
  oNewOrderSingle.get(symbol);
  oNewOrderSingle.get(side);
  oNewOrderSingle.getIfSet(account);

  FIX42::ExecutionReport executionReport = FIX42::ExecutionReport(
    FIX::OrderID(CToolkit::GetUuid()),
    FIX::ExecID(CToolkit::GetUuid()),
    FIX::ExecTransType(FIX::ExecTransType_NEW),
    FIX::ExecType(FIX::ExecType_PENDING_NEW),
    FIX::OrdStatus(FIX::OrdStatus_PENDING_NEW),
    FIX::Symbol(symbol),
    FIX::Side(side),
    FIX::LeavesQty(0),
    FIX::CumQty(0),
    FIX::AvgPx(0.0));
  
  if (!account.getValue().empty()) executionReport.set(FIX::Account(account));
  executionReport.set(FIX::ClOrdID(clOrdID));
  executionReport.set(FIX::LastPx(0));
  executionReport.set(FIX::LastShares(0));

  if (iErrCode != 0)
  {
    executionReport.set(FIX::ExecType(FIX::ExecType_REJECTED));
    executionReport.set(FIX::OrdStatus(FIX::OrdStatus_REJECTED));

    executionReport.set(FIX::OrdRejReason(FIX::OrdRejReason_BROKER_OPTION));
    executionReport.set(FIX::Text(ssErrMsg));
  }

  return CToolkit::Send(oNewOrderSingle, executionReport);
}

bool CSgitTdSpi::SendOrderCancelReject(const std::string &ssUserID, const std::string& ssOrderRef, int iErrCode, const std::string& ssErrMsg)
{
  Order oOrder;
  if(!GetOrderByOrderRef(ssUserID, ssOrderRef, oOrder)) return false;

  return SendOrderCancelReject(oOrder, iErrCode, ssErrMsg);
}

bool CSgitTdSpi::SendOrderCancelReject(const Order& oOrder, int iErrCode, const std::string& ssErrMsg)
{
  Order oCancelOrder;
  if (!GetCancelOrderByClOrdID(oOrder.m_ssClOrdID, oCancelOrder)) return false;

	FIX42::OrderCancelReject orderCancelReject = FIX42::OrderCancelReject(
		FIX::OrderID(oOrder.m_ssOrderSysID),
		FIX::ClOrdID(oCancelOrder.m_ssClOrdID),
		FIX::OrigClOrdID(oOrder.m_ssClOrdID),
		FIX::OrdStatus(oOrder.m_cOrderStatus),
		FIX::CxlRejResponseTo(FIX::CxlRejResponseTo_ORDER_CANCEL_REQUEST));

	orderCancelReject.set(FIX::Text(ssErrMsg));

	return SendByRealAcct(oOrder.m_ssRealAccount, orderCancelReject);
}

bool CSgitTdSpi::SendOrderCancelReject(const FIX42::OrderCancelRequest& oOrderCancel, const std::string& ssErrMsg)
{
  FIX::OrderID orderID;
  FIX::ClOrdID clOrderID;
  FIX::OrigClOrdID origClOrdID;

  oOrderCancel.getFieldIfSet(orderID);
  oOrderCancel.get(clOrderID);
  oOrderCancel.get(origClOrdID);

  FIX42::OrderCancelReject orderCancelReject = FIX42::OrderCancelReject(
    FIX::OrderID(orderID.getValue().empty() ? CToolkit::GetUuid() : orderID.getValue()),
    FIX::ClOrdID(clOrderID),
    FIX::OrigClOrdID(origClOrdID),
    FIX::OrdStatus(FIX::OrdStatus_NEW),
    FIX::CxlRejResponseTo(FIX::CxlRejResponseTo_ORDER_CANCEL_REQUEST));

  orderCancelReject.set(FIX::Text(ssErrMsg));

  return CToolkit::Send(oOrderCancel, orderCancelReject);
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

bool CSgitTdSpi::Cvt(const FIX42::NewOrderSingle& oNewOrderSingle, CThostFtdcInputOrderField& stuInputOrder, Order& oOrder, std::string& ssErrMsg)
{
	FIX::Account account;
	FIX::ClOrdID clOrdID;
	FIX::Symbol symbol;
	FIX::OrderQty orderQty;
	FIX::OrdType ordType;
	FIX::Price price;
	FIX::Side side;
	FIX::OpenClose openClose;
  FIX::TimeInForce timeInForce;
  FIX::ExpireDate expireDate;
  FIX::ExpireTime expireTime;
  FIX::MinQty minQty;

	oNewOrderSingle.get(account);
	oNewOrderSingle.get(clOrdID);
	oNewOrderSingle.get(symbol);
	oNewOrderSingle.get(orderQty);
	oNewOrderSingle.get(ordType);
	oNewOrderSingle.get(side);
	oNewOrderSingle.get(openClose);

  //STUOrder
  oOrder.m_ssUserID = m_ssUserID;
  oOrder.m_ssClOrdID = clOrdID.getValue();

  //由于不能确保送入的ClOrdID(11)严格递增，在这里递增生成一个报单引用并做关联
  std::string ssOrderRef = Poco::format(ssOrderRefFormat, ++m_acOrderRef);
  oOrder.m_ssOrderRef = ssOrderRef;
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,ClOrdID:%s", ssOrderRef.c_str(), clOrdID.getValue().c_str());

  oOrder.m_ssRecvAccount = account.getValue();
  if (!GetRealAccount(oNewOrderSingle, oOrder.m_ssRealAccount, ssErrMsg))
  {
    return false;
  }
  
  oOrder.m_ssRecvSymbol = symbol.getValue();
  oOrder.m_iOrderQty = (int)orderQty.getValue();
  oOrder.m_cOrdType = ordType.getValue();
  oOrder.m_cSide = side.getValue();
  oOrder.m_cOpenClose = openClose.getValue();
  oOrder.m_iLeavesQty = (int)orderQty.getValue();
  oOrder.m_iCumQty = 0;
  oOrder.m_ssOrderTime = CToolkit::GetNowDateTime();

  //价格(tag44)非必须字段
  if(oNewOrderSingle.getIfSet(price)) oOrder.m_dPrice = price.getValue();
	
  if(!SaveOrder(oOrder, ssErrMsg)) return false;
  
	Convert::EnCvtType enSymbolType = Convert::Unknow;
	if(!CheckIdSource(oNewOrderSingle, enSymbolType, ssErrMsg)) return false;

	memset(&stuInputOrder, 0, sizeof(CThostFtdcInputOrderField));
	strncpy(stuInputOrder.UserID, m_ssUserID.c_str(), sizeof(stuInputOrder.UserID));
	strncpy(stuInputOrder.InvestorID, oOrder.m_ssRealAccount.c_str(), sizeof(stuInputOrder.InvestorID));
	strncpy(stuInputOrder.OrderRef, ssOrderRef.c_str(), sizeof(stuInputOrder.OrderRef));
	strncpy(
		stuInputOrder.InstrumentID, 
		enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
		symbol.getValue().c_str() : m_stuTdParam.m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
		sizeof(stuInputOrder.InstrumentID));

	stuInputOrder.VolumeTotalOriginal = oOrder.m_iOrderQty;
	stuInputOrder.OrderPriceType = m_stuTdParam.m_pSgitCtx->CvtDict(ordType.getField(), ordType.getValue(), Convert::Sgit);
	stuInputOrder.LimitPrice = oOrder.m_dPrice;
	stuInputOrder.Direction = m_stuTdParam.m_pSgitCtx->CvtDict(side.getField(), side.getValue(), Convert::Sgit);

  stuInputOrder.TimeCondition = THOST_FTDC_TC_GFD;
  stuInputOrder.VolumeCondition = THOST_FTDC_VC_AV;
  stuInputOrder.MinVolume = oNewOrderSingle.getIfSet(minQty) ? (int)minQty.getValue() : 1;
  if(oNewOrderSingle.getIfSet(timeInForce))
  {
    stuInputOrder.TimeCondition = m_stuTdParam.m_pSgitCtx->CvtDict(
      timeInForce.getField(), timeInForce.getValue(), Convert::Sgit);

    //FOK 特殊处理
    if (timeInForce == FIX::TimeInForce_FILL_OR_KILL)
    {
      stuInputOrder.TimeCondition = THOST_FTDC_TC_IOC;
      stuInputOrder.VolumeCondition = THOST_FTDC_VC_CV;
      stuInputOrder.MinVolume = oOrder.m_iOrderQty;
    }
  }

  if (stuInputOrder.TimeCondition == THOST_FTDC_TC_GTD)
  {
    if(oNewOrderSingle.getIfSet(expireDate)) 
      strncpy(stuInputOrder.GTDDate, expireDate.getValue().c_str(), sizeof(stuInputOrder.GTDDate));
    else if(oNewOrderSingle.getIfSet(expireTime))
    {
      //虽然是utc时间，但是只取日期部分，暂不做本地时间转换
      sprintf(stuInputOrder.GTDDate, "%d", expireTime.getValue().getDate());
    }
  }

  Poco::SharedPtr<STUserInfo> spUserInfo = GetUserInfo(oOrder.m_ssRealAccount);
  if (!spUserInfo)
  {
    ssErrMsg = "Can not find UserInfo by real account:" + oOrder.m_ssRealAccount;
    LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return false;
  }

  //平今平昨处理
  stuInputOrder.CombOffsetFlag[0] = m_stuTdParam.m_pSgitCtx->CvtDict(openClose.getField(), openClose.getValue(), Convert::Sgit);
  //有自定义平今平昨tag，更改原先的赋值
  if (openClose == FIX::OpenClose_CLOSE && spUserInfo->m_iCloseTodayYesterdayTag > 0)
  {
    FIX::FieldBase fCloseTodayYesterday = FIX::FieldBase(spUserInfo->m_iCloseTodayYesterdayTag, "");
    if(oNewOrderSingle.getFieldIfSet(fCloseTodayYesterday))
    {
      stuInputOrder.CombOffsetFlag[0] = m_stuTdParam.m_pSgitCtx->CvtDict(
        fCloseTodayYesterday.getField(), fCloseTodayYesterday.getString(), Convert::Sgit)[0];
    }
  }

  //投机套保处理
  stuInputOrder.CombHedgeFlag[0] = spUserInfo->m_cDefaultSpecHedge; 
  //有自定义投机套保tag，更改原先的赋值
  if (spUserInfo->m_iSpecHedgeTag > 0)
  {
    FIX::FieldBase fSpecHedge = FIX::FieldBase(spUserInfo->m_iSpecHedgeTag, "");
    if(oNewOrderSingle.getFieldIfSet(fSpecHedge))
    {
      stuInputOrder.CombHedgeFlag[0] = m_stuTdParam.m_pSgitCtx->CvtDict(
        fSpecHedge.getField(), fSpecHedge.getString(), Convert::Sgit)[0];
    }
  }
	
	stuInputOrder.ContingentCondition = THOST_FTDC_CC_Immediately;
	stuInputOrder.StopPrice = 0.0;
	stuInputOrder.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	stuInputOrder.IsAutoSuspend = false;
	stuInputOrder.UserForceClose = false;
	stuInputOrder.IsSwapOrder = false;

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

  Order oOrigOrder;
  if(!GetOrderByClOrdID(origClOrdID.getValue(), oOrigOrder))
  {
    ssErrMsg = "Failed to GetOrderByClOrdID:" + origClOrdID.getValue();
    LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return false;
  }

  memset(&stuInputOrderAction, 0, sizeof(CThostFtdcInputOrderActionField));
  stuInputOrderAction.OrderActionRef = ++m_acOrderRef;
  std::string ssOrderActionRef = Poco::format(ssOrderRefFormat, stuInputOrderAction.OrderActionRef);
  LOG(INFO_LOG_LEVEL, "OrderActionRef:%s,ClOrdID:%s", ssOrderActionRef.c_str(), clOrdID.getValue().c_str());

  Order oOrderCancelReq;
  oOrderCancelReq.m_ssUserID = m_ssUserID;
  oOrderCancelReq.m_ssClOrdID = clOrdID.getValue();
  oOrderCancelReq.m_ssOrderRef = ssOrderActionRef;
  oOrderCancelReq.m_ssCancelClOrdID = origClOrdID.getValue();
  oOrderCancelReq.m_ssOrderTime = CToolkit::GetNowDateTime();

  if(!SaveOrder(oOrderCancelReq, ssErrMsg)) return false;
  
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
    strncpy(stuInputOrderAction.OrderRef, oOrigOrder.m_ssOrderRef.c_str(), sizeof(stuInputOrderAction.OrderRef));
    strncpy(stuInputOrderAction.UserID, m_ssUserID.c_str(), sizeof(stuInputOrderAction.UserID));
		strncpy(
			stuInputOrderAction.InstrumentID, 
			enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ?
			symbol.getValue().c_str() : m_stuTdParam.m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
			sizeof(stuInputOrderAction.InstrumentID));
  }

  stuInputOrderAction.RequestID = m_acRequestId++;

  return true;
}

bool CSgitTdSpi::ReqQryOrder(const FIX42::OrderStatusRequest& oOrderStatusRequest, std::string& ssErrMsg)
{
  FIX::ClOrdID clOrdID;
  FIX::Symbol symbol;
  FIX::OrderID orderID;
  FIX::Account account;
  
  oOrderStatusRequest.get(clOrdID);
  oOrderStatusRequest.get(symbol);
 
  std::string ssOrderSysID = "";
  if (oOrderStatusRequest.getIfSet(orderID))
  {
    ssOrderSysID = orderID.getValue();
  }
  else
  {
    Order oOrder;
    if(!GetOrderByClOrdID(clOrdID.getValue(), oOrder))
    {
      ssErrMsg = "Failed to GetOrderByClOrdID by clOrdID:" + clOrdID.getValue();
      LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
      return SendExecutionReport(oOrderStatusRequest, -1, ssErrMsg);
    }

    ssOrderSysID = oOrder.m_ssOrderSysID;
  }

  CThostFtdcQryOrderField stuQryOrder;
  memset(&stuQryOrder, 0, sizeof(CThostFtdcQryOrderField));
  if (!ssOrderSysID.empty())
  {
    strncpy(stuQryOrder.OrderSysID, ssOrderSysID.c_str(), sizeof(stuQryOrder.OrderSysID));
  }

  Convert::EnCvtType enSymbolType = m_stuTdParam.m_pSgitCtx->GetSymbolType(CToolkit::GetSessionKey(oOrderStatusRequest));
  strncpy(
    stuQryOrder.InstrumentID, 
    enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ?
    symbol.getValue().c_str() : m_stuTdParam.m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
    sizeof(stuQryOrder.InstrumentID));

  //有带上账号的情况
  if (oOrderStatusRequest.getIfSet(account))
  {
    std::string ssRealAccount = "";
    if (!GetRealAccount(oOrderStatusRequest, ssRealAccount, ssErrMsg))
    {
      return SendExecutionReport(oOrderStatusRequest, -1, ssErrMsg);
    }
    strncpy(stuQryOrder.InvestorID, ssRealAccount.c_str(), sizeof(stuQryOrder.InvestorID));
  }

  int iRet = m_stuTdParam.m_pTdReqApi->ReqQryOrder(&stuQryOrder, m_acRequestId++);
	if (iRet != 0)
	{
    ssErrMsg =  "Failed to call api:ReqQryOrder,iRet:" + iRet;
		LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return SendExecutionReport(oOrderStatusRequest, -1, ssErrMsg);
	}

  return true;
}

void CSgitTdSpi::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pOrder || !pRspInfo) return;
  LOG(INFO_LOG_LEVEL, "UserID:%s,OrderRef:%s,OrderSysID:%s,OrderStatus:%c,VolumeTraded:%d,VolumeLeave:%d",
    pOrder->UserID, pOrder->OrderRef, pOrder->OrderSysID, pOrder->OrderStatus, pOrder->VolumeTraded, pOrder->VolumeTotal);

  Order oOrder;
  if(!GetOrderByOrderRef(pOrder->UserID, pOrder->OrderRef, oOrder)) return;

  oOrder.m_cOrderStatus = m_stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, pOrder->OrderStatus, Convert::Fix);
  oOrder.m_iCumQty = pOrder->VolumeTraded;
  oOrder.m_iLeavesQty = pOrder->VolumeTotal;

  //订单被拒绝
  if (pOrder->OrderSubmitStatus == THOST_FTDC_OSS_InsertRejected 
    || pOrder->OrderSubmitStatus == THOST_FTDC_OSS_CancelRejected)
  {
    SendExecutionReport(oOrder, -1, "Reject by Exchange", false, true);
    return;
  }

  SendExecutionReport(oOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg, false, true);
}

void CSgitTdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  LOG(ERROR_LOG_LEVEL, "ErrorID:%d,ErrorMsg:%s,RequestID:%d", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID);
}

bool CSgitTdSpi::OnMessage(const FIX::Message& oMsg, const FIX::SessionID& oSessionID, std::string& ssErrMsg)
{
  FIX::MsgType msgType;
  oMsg.getHeader().getField(msgType);

  if (msgType == FIX::MsgType_NewOrderSingle)
  {
    return ReqOrderInsert((const FIX42::NewOrderSingle&) oMsg, ssErrMsg);
  }
  else if(msgType == FIX::MsgType_OrderCancelRequest)
  {
    return ReqOrderAction((const FIX42::NewOrderSingle&) oMsg, ssErrMsg);
  }
  else if(msgType == FIX::MsgType_OrderStatusRequest)
  {
    return ReqQryOrder((const FIX42::OrderStatusRequest&) oMsg, ssErrMsg);
  }
  else if(msgType == FIX::MsgType_AccountQuery)
  {
    return ReqAccountQuery(oMsg, ssErrMsg);
  }
  else if(msgType == FIX::MsgType_CapitalQuery)
  {
    return ReqCapitalQuery(oMsg, ssErrMsg);
  }
  else if(msgType == FIX::MsgType_PositionQuery)
  {
    return ReqPositionQuery(oMsg, ssErrMsg);
  }
  else if(msgType == FIX::MsgType_ContractQuery)
  {
    return ReqContractQuery(oMsg, ssErrMsg);
  }

  ssErrMsg = "unsupported message type";
  return false;
}

bool CSgitTdSpi::Init()
{
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

bool CSgitTdSpi::LoadUserInfo(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp, Poco::SharedPtr<STUserInfo> spUserInfo)
{
  //代码类型
  if (apSgitConf->hasProperty(ssSessionProp + ".SymbolType"))
  {
    spUserInfo->m_enCvtType = (Convert::EnCvtType)apSgitConf->getInt(ssSessionProp + ".SymbolType");
  }

  //平今平昨自定义tag
  if (apSgitConf->hasProperty(ssSessionProp + ".CloseTodayYesterdayTag"))
  {
    spUserInfo->m_iCloseTodayYesterdayTag = apSgitConf->getInt(ssSessionProp + ".CloseTodayYesterdayTag");
  }

  //投机套保的自定义tag
  if (apSgitConf->hasProperty(ssSessionProp + ".SpecHedgeTag"))
  {
    spUserInfo->m_iSpecHedgeTag = apSgitConf->getInt(ssSessionProp + ".SpecHedgeTag");
  }

  //默认投机套保值（不在投机套保tag中显式指明时取的默认值，不做字典转换）
  if (apSgitConf->hasProperty(ssSessionProp + ".DefaultSpecHedge"))
  {
    spUserInfo->m_cDefaultSpecHedge = apSgitConf->getString(ssSessionProp + ".DefaultSpecHedge")[0];
  }

  return true;
}

bool CSgitTdSpi::LoadAcctAlias(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp)
{
  std::string ssAccountAliasList = "";
  if(!CToolkit::GetString(apSgitConf, ssSessionProp + ".AccountAlias", ssAccountAliasList)) return false;
  StringTokenizer stAccountAliasList(ssAccountAliasList, ";", 
    StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
  std::string ssSessionIDKey = "", ssRealAcctProp = "", ssRealAcct = "";
  for (StringTokenizer::Iterator it = stAccountAliasList.begin(); it != stAccountAliasList.end(); it++)
  {
    ssSessionIDKey = CToolkit::SessionProp2ID(ssSessionProp) + "|" + *it;

    ssRealAcctProp = ssSessionProp + "." + *it;
    if(!CToolkit::GetString(apSgitConf, ssRealAcctProp, ssRealAcct)) return false;

    LOG(INFO_LOG_LEVEL, "load %s:%s", ssRealAcctProp.c_str(), ssRealAcct.c_str());
    std::pair<std::map<std::string, std::string>::iterator, bool> ret;
		ret = m_mapAlias2RealAcct.insert(std::pair<std::string, std::string>(ssSessionIDKey, ssRealAcct));
    if(!ret.second)
    {
      LOG(ERROR_LOG_LEVEL, "Alias account key:%s is dulplicated", ssSessionIDKey.c_str());
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

bool CSgitTdSpi::GetRealAccount(const FIX::Message& oRecvMsg, std::string &ssRealAccount, std::string &ssErrMsg)
{
	FIX::Account account;
	oRecvMsg.getField(account);

	if (!CToolkit::IsAliasAcct(account.getValue()))
  {
    ssRealAccount = account.getValue();
    return true;
  }

	std::string ssKey = CToolkit::GetSessionKey(oRecvMsg) + "|" + account.getValue();
	std::map<std::string, std::string>::const_iterator cit = 
		m_mapAlias2RealAcct.find(ssKey);
	if(cit != m_mapAlias2RealAcct.end())
  {
    ssRealAccount = cit->second;
    return true;
  }

  ssErrMsg = "Can not find real account by key:" + ssKey;
  LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
  return false;
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

bool CSgitTdSpi::SendByRealAcct(const std::string &ssRealAcct, FIX::Message& oMsg)
{
  Poco::SharedPtr<STUserInfo> spUserInfo = GetUserInfo(ssRealAcct);
  if (!spUserInfo)
  {
    LOG(ERROR_LOG_LEVEL, "Failed to GetUserInfo by real account:%s", ssRealAcct.c_str());
    return false;
  }

  return CToolkit::Send(oMsg, spUserInfo->m_oSessionID, spUserInfo->m_ssOnBehalfOfCompID);
}

bool CSgitTdSpi::ReqUserLogin(const std::string &ssUserID, const std::string &ssPassword, std::string &ssErrMsg)
{
  m_bNeedKeepLogin = true;

  m_ssUserID = ssUserID;
  m_ssPassword = ssPassword;

  if(!m_oEventConnected.tryWait(G_WAIT_TIME))
  {
    ssErrMsg = "wait for front connected time out";
    return false;
  }

  CThostFtdcReqUserLoginField stuLogin;
  memset(&stuLogin, 0, sizeof(CThostFtdcReqUserLoginField));
  strncpy(stuLogin.UserID, m_ssUserID.c_str(), sizeof(stuLogin.UserID));
  strncpy(stuLogin.Password, m_ssPassword.c_str(), sizeof(stuLogin.Password));
  m_stuTdParam.m_pTdReqApi->ReqUserLogin(&stuLogin, m_acRequestId++);
  LOG(INFO_LOG_LEVEL, "ReqUserLogin userID:%s", m_ssUserID.c_str());
  
  if (!m_oEventLoginResp.tryWait(G_WAIT_TIME))
  {
    ssErrMsg = "wait for front login response time out";
    return false;
  }

  if(!m_bLastLoginOk) Poco::format(ssErrMsg, "failed to login errcode:%d", m_iLoginRespErrID);
  return m_bLastLoginOk;
}

bool CSgitTdSpi::ReqAccountQuery(const FIX42::Message& oMessage, std::string& ssErrMsg)
{
  FIX::Account account;
  FIX::ReqID reqId;
  oMessage.getFieldIfSet(account);
  oMessage.getField(reqId);

  int iCurRequsetId = m_acRequestId++;
  m_expchReqId2Message.add(iCurRequsetId, oMessage);

  CThostFtdcQryTradingCodeField stuTradeCode;
  memset(&stuTradeCode, 0, sizeof(CThostFtdcQryTradingCodeField));
  if (!account.getValue().empty())
  {
    strncpy(stuTradeCode.InvestorID, account.getValue().c_str(), sizeof(stuTradeCode.InvestorID));
  }

  int iRet = m_stuTdParam.m_pTdReqApi->ReqQryTradingCode(&stuTradeCode, iCurRequsetId);
  if (iRet != 0)
  {
    Poco::format(ssErrMsg, "Failed to call api:ReqOrderInsert,iRet:%d", iRet);
    LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return false;
  }

  return true;
}

void CSgitTdSpi::OnRspQryTradingCode(CThostFtdcTradingCodeField *pTradingCode, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  if (pRspInfo) LOG(INFO_LOG_LEVEL, "errId:%d,errMsg:%s", pRspInfo->ErrorID, pRspInfo->ErrorMsg);
  if (pTradingCode)
    LOG(INFO_LOG_LEVEL, "BrokerID:%s,InvestorID:%s,ClientID:%s,ClientIDType:%c,ExchangeID:%s,nRequestID:%d,bIsLast:%d", 
    pTradingCode->BrokerID, pTradingCode->InvestorID, pTradingCode->ClientID, pTradingCode->ClientIDType, 
    pTradingCode->ExchangeID, nRequestID, bIsLast);
  
  AppendQryData(FIX::MsgType(FIX::MsgType_AccountQueryResp), (char*)pTradingCode, sizeof(CThostFtdcTradingCodeField), pRspInfo, nRequestID, bIsLast);
}

bool CSgitTdSpi::ReqCapitalQuery(const FIX42::Message& oMessage, std::string& ssErrMsg)
{
  FIX::Account account;
  FIX::ReqID reqId;
  oMessage.getFieldIfSet(account);
  oMessage.getField(reqId);

  int iCurRequsetId = m_acRequestId++;
  m_expchReqId2Message.add(iCurRequsetId, oMessage);

  CThostFtdcQryTradingAccountField stuAccount;
  memset(&stuAccount, 0, sizeof(CThostFtdcQryTradingAccountField));
  if (!account.getValue().empty())
  {
    strncpy(stuAccount.InvestorID, account.getValue().c_str(), sizeof(stuAccount.InvestorID));
  }
  int iRet = m_stuTdParam.m_pTdReqApi->ReqQryTradingAccount(&stuAccount, iCurRequsetId);
  if (iRet != 0)
  {
    Poco::format(ssErrMsg, "Failed to call api:ReqQryTradingAccount,iRet:%d", iRet);
    LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return false;
  }

  return true;
}

void CSgitTdSpi::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  LOG(INFO_LOG_LEVEL, "bIsLast:%d", bIsLast);
  if(pRspInfo) LOG(INFO_LOG_LEVEL, "errId:%d,errMsg:%s", pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  if(pTradingAccount) 
    LOG(INFO_LOG_LEVEL, "AccountID:%s,Available:%lf,PositionProfit:%lf,CloseProfit:%lf", pTradingAccount->AccountID, pTradingAccount->Available, pTradingAccount->PositionProfit, pTradingAccount->CloseProfit);

  AppendQryData(FIX::MsgType(FIX::MsgType_CapitalQueryResp), (char*)pTradingAccount, sizeof(CThostFtdcTradingAccountField), pRspInfo, nRequestID, bIsLast);
}

bool CSgitTdSpi::ReqPositionQuery(const FIX42::Message& oMessage, std::string& ssErrMsg)
{
  FIX::Account account;
  FIX::Symbol symbol;
  FIX::ReqID reqId;
  oMessage.getFieldIfSet(account);
  oMessage.getFieldIfSet(symbol);
  oMessage.getField(reqId);

  int iCurRequsetId = m_acRequestId++;
  m_expchReqId2Message.add(iCurRequsetId, oMessage);

  CThostFtdcQryInvestorPositionField stuPosition;
  memset(&stuPosition, 0, sizeof(CThostFtdcQryInvestorPositionField));
  if (!account.getValue().empty())
  {
    strncpy(stuPosition.InvestorID, account.getValue().c_str(), sizeof(stuPosition.InvestorID));
  }
  if (!symbol.getValue().empty())
  {
    strncpy(stuPosition.InstrumentID, symbol.getValue().c_str(), sizeof(stuPosition.InstrumentID));
  }

  int iRet = m_stuTdParam.m_pTdReqApi->ReqQryInvestorPosition(&stuPosition, iCurRequsetId);
  if (iRet != 0)
  {
    Poco::format(ssErrMsg, "Failed to call api:ReqQryInvestorPosition,iRet:%d", iRet);
    LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return false;
  }

  return true;
}

void CSgitTdSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  LOG(INFO_LOG_LEVEL, "bIsLast:%d", bIsLast);
  if(pRspInfo) LOG(INFO_LOG_LEVEL, "errId:%d,errMsg:%s", pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  if(pInvestorPosition)
    LOG(INFO_LOG_LEVEL, "InvestorID:%s,PosiDirection:%c,Position:%d,YdPosition:%d,OpenVolume:%d,CloseVolume:%d", 
    pInvestorPosition->InvestorID, pInvestorPosition->PosiDirection, pInvestorPosition->Position, pInvestorPosition->YdPosition,
    pInvestorPosition->OpenVolume, pInvestorPosition->CloseVolume);

  AppendQryData(FIX::MsgType(FIX::MsgType_PositionQueryResp), (char*)pInvestorPosition, sizeof(CThostFtdcInvestorPositionField), pRspInfo, nRequestID, bIsLast);
}

bool CSgitTdSpi::ReqContractQuery(const FIX42::Message& oMessage, std::string& ssErrMsg)
{
  FIX::Symbol symbol;
  FIX::ReqID reqId;
  oMessage.getFieldIfSet(symbol);
  oMessage.getField(reqId);

  int iCurRequsetId = m_acRequestId++;
  m_expchReqId2Message.add(iCurRequsetId, oMessage);

  CThostFtdcQryInstrumentField stuInstrument;
  memset(&stuInstrument, 0, sizeof(CThostFtdcQryInstrumentField));
  if (!symbol.getValue().empty())
  {
    std::string ssOriginalSymbol = m_stuTdParam.m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original);
    strncpy(stuInstrument.InstrumentID, ssOriginalSymbol.c_str(), sizeof(stuInstrument.InstrumentID));
  }
  int iRet = m_stuTdParam.m_pTdReqApi->ReqQryInstrument(&stuInstrument, iCurRequsetId);
  if (iRet != 0)
  {
    Poco::format(ssErrMsg, "Failed to call api:ReqQryInstrument,iRet:%d", iRet);
    LOG(ERROR_LOG_LEVEL, ssErrMsg.c_str());
    return false;
  }

  return true;
}

void CSgitTdSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
  LOG(INFO_LOG_LEVEL, "bIsLast:%d", bIsLast);
  if(pRspInfo) LOG(INFO_LOG_LEVEL, "errId:%d,errMsg:%s", pRspInfo->ErrorID, pRspInfo->ErrorMsg);

  if(pInstrument)
    LOG(INFO_LOG_LEVEL, "InstrumentID:%s,ExchangeID:%s,ProductID:%s,ExpireDate:%s,PriceTick:%lf",
    pInstrument->InstrumentID, pInstrument->ExchangeID, pInstrument->ProductID, pInstrument->ExpireDate, 
    pInstrument->PriceTick);

   AppendQryData(FIX::MsgType(FIX::MsgType_ContractQueryResp), (char*)pInstrument, sizeof(CThostFtdcInstrumentField), pRspInfo, nRequestID, bIsLast);
}

void CSgitTdSpi::AppendQryData(const FIX::MsgType &oMsgType, char *pRspData, int iDataSize, CThostFtdcRspInfoField *pRspInfo, int iReqID, bool bIsLast)
{
  //出现错误
  if (pRspInfo && pRspInfo->ErrorID)
  {
    FIX42::Message oMsg(oMsgType);
    oMsg.setField(FIX::RejectReason(pRspInfo->ErrorID));
    if (strlen(pRspInfo->ErrorMsg) > 0) oMsg.setField(FIX::Text(pRspInfo->ErrorMsg));

    return Send(iReqID, oMsg);
  }

  if (iReqID != m_iCurReqID)
  {
    m_vBuffer.clear();
    m_iCurReqID = iReqID;
  }

  //第一步，先将数据打到缓冲区中去
  if (pRspData)
  {
    int iPreQryDataSize = m_vBuffer.size();
    m_vBuffer.resize(iPreQryDataSize + iDataSize);
    memcpy(&m_vBuffer[iPreQryDataSize], pRspData, iDataSize);
  }

  //最后一个，组包发送
  if (!bIsLast) return;

  GenAndSend(oMsgType, iDataSize, iReqID);
}

void CSgitTdSpi::Send(int iReqID, FIX::Message& oMsg)
{
  Poco::SharedPtr<FIX42::Message> spMsgRecv =  m_expchReqId2Message.get(iReqID);
  if (!spMsgRecv)
  {
    LOG(ERROR_LOG_LEVEL, "Can not find MsgRecv by reqID:%d", iReqID);
    return;
  }

  FIX::ReqID reqId;
  oMsg.setField(spMsgRecv->getField(reqId));

  CToolkit::Send(*spMsgRecv, oMsg);
}

void CSgitTdSpi::GenAndSend(const FIX::MsgType &oMsgType, int iDataSize, int iReqID)
{
  FIX42::Message oMsg(oMsgType);
  int iItemCount = m_vBuffer.size() / iDataSize;

  Poco::SharedPtr<FIX42::Message> spMsgRecv =  m_expchReqId2Message.get(iReqID);
  if (!spMsgRecv)
  {
    LOG(ERROR_LOG_LEVEL, "Can not find MsgRecv by reqID:%d", iReqID);
    return;
  }

  Convert::EnCvtType enSymbolType = m_stuTdParam.m_pSgitCtx->GetSymbolType(CToolkit::GetSessionKey(*spMsgRecv));

  if (oMsgType == FIX::MsgType_AccountQueryResp)
  {
    CThostFtdcTradingCodeField  stuTradingCode;
    for (int i = 0; i < iItemCount; i++)
    {
      memset(&stuTradingCode, 0, sizeof(CThostFtdcTradingCodeField));
      memcpy(&stuTradingCode, &m_vBuffer[iDataSize * i], iDataSize);

      FIX::AccountGroup accountGroup;
      accountGroup.setField(FIX::Account(stuTradingCode.InvestorID));
      accountGroup.setField(FIX::ClientID(stuTradingCode.ClientID));
      accountGroup.setField(FIX::SecurityExchange(stuTradingCode.ExchangeID));
      oMsg.addGroup(accountGroup);
    }
  }
  else if(oMsgType == FIX::MsgType_CapitalQueryResp)
  {
    CThostFtdcTradingAccountField stuTradingAcct;
    for (int i = 0; i < iItemCount; i++)
    {
      memset(&stuTradingAcct, 0, sizeof(CThostFtdcTradingAccountField));
      memcpy(&stuTradingAcct, &m_vBuffer[iDataSize * i], iDataSize);

      FIX::AccountGroup accountGroup;
      accountGroup.setField(FIX::Account(stuTradingAcct.AccountID));

      FIX::CapitalFiledGroup capitalGroup;
      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_PreBalance));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.PreBalance));
      accountGroup.addGroup(capitalGroup);

      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_Available));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.Available));
      accountGroup.addGroup(capitalGroup);

      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_CurrMargin));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.CurrMargin));
      accountGroup.addGroup(capitalGroup);

      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_Commission));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.Commission));
      accountGroup.addGroup(capitalGroup);

      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_PositionProfit));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.PositionProfit));
      accountGroup.addGroup(capitalGroup);

      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_CloseProfit));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.CloseProfit));
      accountGroup.addGroup(capitalGroup);

      //权益=上日结存+平仓盈亏+浮动盈亏+出入金-手续费 
      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_Interest));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.PreBalance + stuTradingAcct.CloseProfit + 
        stuTradingAcct.PositionProfit + stuTradingAcct.Deposit - stuTradingAcct.Withdraw - stuTradingAcct.Commission));
      accountGroup.addGroup(capitalGroup);

      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_FrozenMargin));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.FrozenMargin));
      accountGroup.addGroup(capitalGroup);

      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_Deposit));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.Deposit));
      accountGroup.addGroup(capitalGroup);

      capitalGroup.setField(FIX::CapitalFieldType(FIX::CapitalFieldType_Withdraw));
      capitalGroup.setField(FIX::CapitalFieldValue(stuTradingAcct.Withdraw));
      accountGroup.addGroup(capitalGroup);

      oMsg.addGroup(accountGroup);
    }
  }
  else if(oMsgType == FIX::MsgType_PositionQueryResp)
  {
    CThostFtdcInvestorPositionField stuPosition;
    for (int i = 0; i < iItemCount; i++)
    {
      memset(&stuPosition, 0, sizeof(CThostFtdcInvestorPositionField));
      memcpy(&stuPosition, &m_vBuffer[iDataSize * i], iDataSize);

      FIX::AccountGroup accountGroup;
      accountGroup.setField(FIX::Account(stuPosition.InvestorID));
      accountGroup.setField(FIX::Symbol(enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
        stuPosition.InstrumentID : m_stuTdParam.m_pSgitCtx->CvtSymbol(stuPosition.InstrumentID, enSymbolType)));

      FIX::PositionFieldGroup positionGroup;
      positionGroup.setField(FIX::PositionFieldType(stuPosition.PosiDirection == THOST_FTDC_PD_Long ? 
        FIX::PositionFieldType_YesterdayLong : FIX::PositionFieldType_YesterdayShort));
      positionGroup.setField(FIX::PositionFieldValue(stuPosition.YdPosition));
      accountGroup.addGroup(positionGroup);

      positionGroup.setField(FIX::PositionFieldType(stuPosition.PosiDirection == THOST_FTDC_PD_Long ? 
        FIX::PositionFieldType_TodayLong : FIX::PositionFieldType_TodayShort));
      positionGroup.setField(FIX::PositionFieldValue(stuPosition.TodayPosition));
      accountGroup.addGroup(positionGroup);

      positionGroup.setField(FIX::PositionFieldType(stuPosition.PosiDirection == THOST_FTDC_PD_Long ? 
        FIX::PositionFieldType_TodayOpenLong : FIX::PositionFieldType_TodayOpenShort));
      positionGroup.setField(FIX::PositionFieldValue(stuPosition.OpenVolume));
      accountGroup.addGroup(positionGroup);

      positionGroup.setField(FIX::PositionFieldType(stuPosition.PosiDirection == THOST_FTDC_PD_Long ? 
        FIX::PositionFieldType_TodayCloseLong : FIX::PositionFieldType_TodayCloseShort));
      positionGroup.setField(FIX::PositionFieldValue(stuPosition.CloseVolume));
      accountGroup.addGroup(positionGroup);

      oMsg.addGroup(accountGroup);
    }
  }
  else if(oMsgType == FIX::MsgType_ContractQueryResp)
  {
    CThostFtdcInstrumentField stuInstrument;
    for (int i = 0; i < iItemCount; i++)
    {
      memset(&stuInstrument, 0, sizeof(CThostFtdcInstrumentField));
      memcpy(&stuInstrument, &m_vBuffer[iDataSize * i], iDataSize);

      FIX::ContractGroup contractGroup;
      contractGroup.setField(FIX::Symbol(enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
        stuInstrument.InstrumentID : m_stuTdParam.m_pSgitCtx->CvtSymbol(stuInstrument.InstrumentID, enSymbolType)));
      contractGroup.setField(FIX::SecurityExchange(enSymbolType == Convert::Original ||  enSymbolType == Convert::Unknow ? 
        stuInstrument.ExchangeID : m_stuTdParam.m_pSgitCtx->CvtExchange(stuInstrument.ExchangeID, enSymbolType)));
      contractGroup.setField(FIX::MaturityDate(stuInstrument.ExpireDate));
      contractGroup.setField(FIX::MinPriceIncrement(stuInstrument.PriceTick));

      oMsg.addGroup(contractGroup);
    }
  }

  if (iItemCount < 1)
  {
    oMsg.setField(FIX::RejectReason(FIX::RejectReason_Empty));
  }
  else
  {
    oMsg.setField(FIX::RejectReason(FIX::RejectReason_Success));
  }

  Send(iReqID, oMsg);
}

bool CSgitTdSpi::SaveOrder(Order &oOrder, std::string &ssErrMsg)
{
  try
  {
    oOrder.m_ssTradingDay = m_stuTdParam.m_pTdReqApi->GetTradingDay();
    
    //if(!m_stuTdParam.m_spSQLiteSession)
    //{
    //  ssErrMsg = "SQLiteSession is not initialized";
    //  LOG(WARN_LOG_LEVEL, ssErrMsg.c_str());
    //  return false;
    //}

    *m_stuTdParam.m_spSQLiteSession << 
      "INSERT INTO [Order] VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", use(oOrder), now;
  }

  catch (Poco::Exception &e)
  {
    LOG(ERROR_LOG_LEVEL, e.displayText().c_str());
    ssErrMsg = e.displayText();
    return false;
  }

  return true;
}

bool CSgitTdSpi::UpdateOrder(Order &oOrder)
{
  try
  {
    *m_stuTdParam.m_spSQLiteSession << 
      "UPDATE [Order] SET orderStatus = ?, orderSysID = ?, leavesQty = ?, cumQty = ? WHERE userID = ? AND clOrdID = ?", 
      use(oOrder.m_cOrderStatus), use(oOrder.m_ssOrderSysID), use(oOrder.m_iLeavesQty), use(oOrder.m_iCumQty), 
      use(oOrder.m_ssUserID), use(oOrder.m_ssClOrdID), now;
  }

  catch (Poco::Exception &e)
  {
    LOG(ERROR_LOG_LEVEL, e.displayText().c_str());
    //ssErrMsg = e.displayText();
    return false;
  }
  return true;
}

bool CSgitTdSpi::SaveTrade(Trade &oTrade)
{
  try
  {
    *m_stuTdParam.m_spSQLiteSession << 
      "INSERT INTO [Trade] VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)", use(oTrade), now;
  }

  catch (Poco::Exception &e)
  {
    LOG(ERROR_LOG_LEVEL, e.displayText().c_str());
    //ssErrMsg = e.displayText();
    return false;
  }

  return true;
}

bool CSgitTdSpi::GetOrderByOrderRef(const std::string &ssUserID, const std::string &ssOrderRef, Order &oOrder)
{
  try
  {
    *m_stuTdParam.m_spSQLiteSession << "SELECT * FROM [Order] WHERE userID = ? and orderRef = ? ", 
      useRef(ssUserID), useRef(ssOrderRef), into(oOrder), limit(1, true), now;
  }
  catch (Poco::Exception &e)
  {
    LOG(ERROR_LOG_LEVEL, e.displayText().c_str());
    return false;
  }

  return true;
}


bool CSgitTdSpi::GetOrderByClOrdID(const std::string &ssClOrdID, Order &oOrder)
{
  try
  {
    *m_stuTdParam.m_spSQLiteSession << "SELECT * FROM [Order] WHERE userID = ? and clOrdID = ? ", 
      use(m_ssUserID), useRef(ssClOrdID), into(oOrder), limit(1, true), now;
  }
  catch (Poco::Exception &e)
  {
    LOG(ERROR_LOG_LEVEL, e.displayText().c_str());
    return false;
  }

  return true;
}

int CSgitTdSpi::GetMaxOrderRefInDB()
{
  int iMaxOrderRef = 0;
  try
  {
    *m_stuTdParam.m_spSQLiteSession << "SELECT MAX(orderRef) FROM [Order] WHERE userID = ?", 
      use(m_ssUserID), into(iMaxOrderRef), now;
  }
  catch (Poco::Exception &e)
  {
    LOG(ERROR_LOG_LEVEL, e.displayText().c_str());
  }

  return iMaxOrderRef;
}

bool CSgitTdSpi::GetCancelOrderByClOrdID(const std::string &ssClOrdID, Order &oOrder)
{
  try
  {
    *m_stuTdParam.m_spSQLiteSession << "SELECT * FROM [Order] WHERE userID = ? and cancelClOrdID = ? ORDER BY time desc", 
      use(m_ssUserID), useRef(ssClOrdID), into(oOrder), limit(1), now;
  }
  catch (Poco::Exception &e)
  {
    LOG(ERROR_LOG_LEVEL, e.displayText().c_str());
    return false;
  }

  return true;
}

bool CSgitTdSpi::GetTradeByOrderRef(const std::string &ssOrderRef, std::vector<Trade> &vTrade)
{
  try
  {
    *m_stuTdParam.m_spSQLiteSession << "SELECT * FROM [Trade] WHERE userID = ? and orderRef = ? ORDER BY tradeID desc", 
      use(m_ssUserID), useRef(ssOrderRef), into(vTrade), now;
  }
  catch (Poco::Exception &e)
  {
    LOG(ERROR_LOG_LEVEL, e.displayText().c_str());
    return false;
  }

  return true;
}

double CSgitTdSpi::AvgPx(const std::vector<Trade> &vTrade) const
{
  double dTurnover = 0.0;
  int iTotalVolume = 0;
  for (std::vector<Trade>::const_iterator cit = vTrade.begin(); cit != vTrade.end(); cit++)
  {
    dTurnover += cit->m_dMatchPrice * cit->m_iMatchQty;
    iTotalVolume += cit->m_iMatchQty; 
  }

  if (iTotalVolume == 0) return 0.0;

  return dTurnover / iTotalVolume;
}

std::string CSgitTdSpi::GetTradingDay()
{
  return m_stuTdParam.m_pTdReqApi->GetTradingDay();
}

void CSgitTdSpi::SetSQLiteSession(SharedPtr<Session> spSQLiteSession)
{
  m_stuTdParam.m_spSQLiteSession = spSQLiteSession;

  //对于网关自动登录的账户，初始化时数据库会话并没有初始化，设置数据库会话时顺便确认一下报单引用的值保持最大
  m_acOrderRef = MAX(m_acOrderRef, GetMaxOrderRefInDB());
  LOG(INFO_LOG_LEVEL, "m_acOrderRef set to:%d", m_acOrderRef.value());
}

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

	CToolkit::SessionKey2SessionIDBehalfCompID(CToolkit::SessionProp2ID(ssSessionProp), spUserInfo->m_oSessionID, spUserInfo->m_ssOnBehalfOfCompID);

  LoadUserInfo(apSgitConf, ssSessionProp, spUserInfo);

  m_stuTdParam.m_pSgitCtx->AddUserInfo(CToolkit::SessionProp2ID(ssSessionProp), spUserInfo);

  std::string ssAccountList = "";
  if(!CToolkit::GetString(apSgitConf, ssAcctListProp, ssAccountList)) return false;

  StringTokenizer stAccountList(ssAccountList, ";", 
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
  std::map<std::string, Poco::SharedPtr<STUserInfo> >::const_iterator cit = m_mapRealAcct2UserInfo.find(ssRealAcct);
  if (cit != m_mapRealAcct2UserInfo.end())
  {
    return cit->second->m_enCvtType;
  }

  return Convert::Unknow;
}

Poco::SharedPtr<STUserInfo> CSgitTdSpiHubTran::GetUserInfo(const std::string &ssRealAcct)
{
  std::map<std::string, Poco::SharedPtr<STUserInfo> >::const_iterator cit = m_mapRealAcct2UserInfo.find(ssRealAcct);
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

  LoadUserInfo(apSgitConf, ssSessionProp, m_spUserInfo);

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

void CSgitTdSpiDirect::ReqUserLogout()
{
  m_bNeedKeepLogin = false;

  CThostFtdcUserLogoutField stuLogout;
  memset(&stuLogout, 0, sizeof(CThostFtdcUserLogoutField));
  strncpy(stuLogout.UserID, m_ssUserID.c_str(), sizeof(stuLogout.UserID));
  m_stuTdParam.m_pTdReqApi->ReqUserLogout(&stuLogout, m_acRequestId++);

  LOG(INFO_LOG_LEVEL, "ReqUserLogout userID:%s", m_ssUserID.c_str());
}

