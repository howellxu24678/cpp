#include "SgitTradeSpi.h"
#include "SgitContext.h"
#include "Log.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix42/OrderCancelReject.h"
#include "Poco/Format.h"
#include "Poco/UUIDGenerator.h"
#include "Toolkit.h"

CSgitTradeSpi::CSgitTradeSpi(CSgitContext *pSgitCtx, CThostFtdcTraderApi *pReqApi, const std::string &ssSgitCfgPath, const std::string &ssTradeId) 
  : m_pSgitCtx(pSgitCtx)
  , m_pTradeApi(pReqApi)
  , m_ssSgitCfgPath(ssSgitCfgPath)
  , m_ssTradeID(ssTradeId)
  , m_enSymbolType(Convert::Original)
	, m_acRequestId(0)
	, m_acOrderRef(0)
	, m_chOrderRef2ClOrderID(12*60*60*1000)//超时设为12小时
	, m_chClOrderID2OrderRef(12*60*60*1000)//超时设为12小时
  , m_chOrderRef2Order(12*60*60*1000)
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

    m_pTradeApi->ReqSettlementInfoConfirm(&stuConfirm, m_acRequestId++);
  }
}

void CSgitTradeSpi::ReqOrderInsert(const FIX42::NewOrderSingle& oNewOrderSingle)
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

	std::string ssOrderRef = format(ssOrderRefFormat, ++m_acOrderRef);
	AddOrderRefClOrdID(ssOrderRef, clOrdID.getValue());
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,ClOrdID:%s", ssOrderRef.c_str(), clOrdID.getValue().c_str());
  
	strncpy(stuInputOrder.UserID, m_ssTradeID.c_str(), sizeof(stuInputOrder.UserID));
  strncpy(stuInputOrder.InvestorID, m_pSgitCtx->GetRealAccont(oNewOrderSingle).c_str(), sizeof(stuInputOrder.InvestorID));
  strncpy(stuInputOrder.OrderRef, ssOrderRef.c_str(), sizeof(stuInputOrder.OrderRef));
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
	int iRet = m_pTradeApi->ReqOrderInsert(&stuInputOrder, stuInputOrder.RequestID);
	if (iRet != 0)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to call api ReqOrderInsert,iRet:%d", iRet);
		CThostFtdcRspInfoField stuRspInfo;
		memset(&stuRspInfo, 0, sizeof(CThostFtdcRspInfoField));
		stuRspInfo.ErrorID = iRet;
		strncpy(stuRspInfo.ErrorMsg, "Failed to call api ReqOrderInsert", sizeof(stuRspInfo.ErrorMsg));

		SendExecutionReport(&stuInputOrder, &stuRspInfo);
	}
}

void CSgitTradeSpi::ReqOrderAction(const FIX42::OrderCancelRequest& oOrderCancel)
{
	//两种撤单组合：1.OrderSysID+ExchangeID 2.OrderRef+UserID+InstrumentID

	//本地号
	FIX::ClOrdID clOrdID;
	//被撤单本地号
	FIX::OrigClOrdID origClOrdID;
	//被撤单系统报单号
  FIX::OrderID orderID;
	FIX::SecurityExchange securityExchange;
  //FIX::Side side;
  FIX::Symbol symbol;
  //FIX::Account account;

	oOrderCancel.get(clOrdID);
	oOrderCancel.get(origClOrdID);
	oOrderCancel.getIfSet(orderID);
	oOrderCancel.getIfSet(securityExchange);
  //oOrderCancel.get(side);
  oOrderCancel.get(symbol);
  //oOrderCancel.get(account);

  CThostFtdcInputOrderActionField stuInputOrderAction;
  memset(&stuInputOrderAction, 0, sizeof(CThostFtdcInputOrderActionField));

	stuInputOrderAction.OrderActionRef = ++m_acOrderRef;
	std::string ssOrderActionRef = format(ssOrderRefFormat, stuInputOrderAction.OrderActionRef);
	LOG(INFO_LOG_LEVEL, "OrderActionRef:%s,ClOrdID:%s", ssOrderActionRef.c_str(), clOrdID.getValue().c_str());
	AddOrderRefClOrdID(ssOrderActionRef, clOrdID.getValue());

	//1.OrderSysID+ExchangeID
	if (!orderID.getValue().empty() && !securityExchange.getValue().empty())
	{
		strncpy(stuInputOrderAction.OrderSysID, orderID.getValue().c_str(), sizeof(stuInputOrderAction.OrderSysID));
		strncpy(
			stuInputOrderAction.ExchangeID, 
			m_enSymbolType == Convert::Original ? 
			securityExchange.getValue().c_str() : m_pSgitCtx->CvtExchange(securityExchange.getValue(), Convert::Original).c_str(),
			sizeof(stuInputOrderAction.ExchangeID));
	}
	//2.OrderRef+UserID+InstrumentID
	else
	{
		std::string ssClOrdId = "";
		if(!GetOrderRef(origClOrdID.getValue(), ssClOrdId))
		{
			//回复撤单拒绝
			//SendOrderCancelReject(&stuInputOrderAction, &stuRspInfo);
			return;
		}
		strncpy(stuInputOrderAction.OrderRef, ssClOrdId.c_str(), sizeof(stuInputOrderAction.OrderRef));
		strncpy(stuInputOrderAction.UserID, m_ssTradeID.c_str(), sizeof(stuInputOrderAction.UserID));
		strncpy(
			stuInputOrderAction.InstrumentID, 
			m_enSymbolType == Convert::Original ? 
			symbol.getValue().c_str() : m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
			sizeof(stuInputOrderAction.InstrumentID));
	}

  stuInputOrderAction.RequestID = m_acRequestId++;
  int iRet = m_pTradeApi->ReqOrderAction(&stuInputOrderAction, stuInputOrderAction.RequestID);
	if (iRet != 0)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to call api ReqOrderAction,iRet:%d", iRet);
		CThostFtdcRspInfoField stuRspInfo;
		memset(&stuRspInfo, 0, sizeof(CThostFtdcRspInfoField));
		stuRspInfo.ErrorID = iRet;
		strncpy(stuRspInfo.ErrorMsg, "Failed to call api ReqOrderAction", sizeof(stuRspInfo.ErrorMsg));

		//SendOrderCancelReject(&stuInputOrderAction, &stuRspInfo);
	}
}


void CSgitTradeSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	LOG(INFO_LOG_LEVEL, "ErrorID:%d, ErrorMsg:%s,OrderRef:%s,OrderSysID:%s,ExchangeID:%s", 
		pRspInfo->ErrorID, pRspInfo->ErrorMsg, pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID);

	SendExecutionReport(pInputOrder, pRspInfo);
}

void CSgitTradeSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	LOG(INFO_LOG_LEVEL, "ErrorID:%d,ErrorMsg:%s,OrderActionRef:%d,OrderRef:%s,OrderSysID:%s,VolumeChange:%d", 
    pRspInfo->ErrorID, pRspInfo->ErrorMsg, pInputOrderAction->OrderActionRef, pInputOrderAction->OrderRef, 
    pInputOrderAction->OrderSysID, pInputOrderAction->VolumeChange);

	std::string ssClOrdID = "";
	if (!GetClOrdID(format(ssOrderRefFormat, pInputOrderAction->OrderActionRef), ssClOrdID)) return;

	std::string ssOrigClOrdID = "";
	if (strlen(pInputOrderAction->OrderRef) > 0 && 
		!GetClOrdID(format(ssOrderRefFormat, (std::string)pInputOrderAction->OrderRef), ssOrigClOrdID)) return;

	LOG(INFO_LOG_LEVEL, "OrderActionRef:%d,ClOrdID:%s,OrigClOrdID:%s",
		pInputOrderAction->OrderActionRef, ssClOrdID.c_str(), ssOrigClOrdID.c_str());

	if (pRspInfo->ErrorID != 0)
	{
		return SendOrderCancelReject(pInputOrderAction, pRspInfo, ssClOrdID, ssOrigClOrdID);
	}

	//std::string& ssUUid = CToolkit::GetUuid();
	//FIX42::ExecutionReport executionReport = FIX42::ExecutionReport();
	//executionReport.set(FIX::OrderID( strlen(pInputOrderAction->OrderSysID) < 1 ? " " : pInputOrderAction->OrderSysID));
	//executionReport.set(FIX::ExecID(ssUUid));
	//executionReport.set(FIX::Symbol(m_enSymbolType == Convert::Original ? 
	//	pInputOrderAction->InstrumentID : m_pSgitCtx->CvtSymbol(pInputOrderAction->InstrumentID, m_enSymbolType).c_str()));
	///*executionReport.set(FIX::Side(m_pSgitCtx->CvtDict(FIX::FIELD::Side, pInputOrderAction->Direction, Convert::Fix)));*/
	//executionReport.set(FIX::LeavesQty(pInputOrder->VolumeTotalOriginal));
	//executionReport.set(FIX::CumQty(0));
	//executionReport.set(FIX::AvgPx(0));
	//executionReport.set(FIX::ClOrdID(ssClOrdID));
	//executionReport.set(FIX::OrderQty(pInputOrder->VolumeTotalOriginal));
	//executionReport.set(FIX::LastShares(0));
	//executionReport.set(FIX::LastPx(0));

	//executionReport.set(FIX::ExecTransType(FIX::ExecTransType_NEW));
	//executionReport.set(FIX::OrdStatus(isSuccess ? FIX::OrdStatus_NEW : FIX::OrdStatus_REJECTED));
	//executionReport.set(FIX::ExecType(isSuccess ? FIX::ExecType_NEW : FIX::ExecType_REJECTED));

	//if(!isSuccess)
	//{
	//	executionReport.set(FIX::OrdRejReason(FIX::OrdRejReason_BROKER_OPTION));
	//	executionReport.set(FIX::Text(format("errID:%d,errMsg:%s", pRspInfo->ErrorID, (std::string)(pRspInfo->ErrorMsg))));
	//}

	//m_pSgitCtx->Send(pInputOrder->InvestorID, executionReport);
	
}

//撤单的情况在此回复执行回报，其余情况只用于更新订单的最新状态参数
void CSgitTradeSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
  if(strlen(pOrder->OrderRef) < 1) return;
  std::string ssOrderRef = std::string(pOrder->OrderRef);
  if (!m_chOrderRef2Order.has(ssOrderRef))
  {
    STUOrder stuOrder;
    m_chOrderRef2Order.add(ssOrderRef, stuOrder);
  }
  m_chOrderRef2Order.get(ssOrderRef)->Update(*pOrder);

	std::string ssClOrdID = "";
	if (!GetClOrdID(pOrder->OrderRef, ssClOrdID)) return;

  std::string& ssUUid = CToolkit::GetUuid();
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
  executionReport.set(FIX::ClOrdID(ssClOrdID.c_str()));

  //if (strlen(pOrder->RelativeOrderSysID) > 1 )
  //  executionReport.set(FIX::OrigClOrdID(pOrder->RelativeOrderSysID));

  executionReport.set(FIX::OrderQty(pOrder->VolumeTotalOriginal));
  executionReport.set(FIX::LastShares(pOrder->VolumeTraded));
  executionReport.set(FIX::LastPx(pOrder->LimitPrice));

  executionReport.set(FIX::ExecTransType(FIX::ExecTransType_NEW));
  char chOrderStatus = m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, pOrder->OrderStatus, Convert::Fix);
  executionReport.set(FIX::OrdStatus(chOrderStatus));
  executionReport.set(FIX::ExecType(chOrderStatus));


  m_pSgitCtx->Send(pOrder->InvestorID, executionReport);
}


//收到成交，回复执行回报
void CSgitTradeSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,TradeID:%s,OrderSysID:%s,Price:%f,Volume:%d", 
    pTrade->OrderRef, pTrade->TradeID, pTrade->OrderSysID, pTrade->Price, pTrade->Volume);
}

void CSgitTradeSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
  LOG(INFO_LOG_LEVEL, "ErrorID:%d, ErrorMsg:%s,OrderRef:%s, OrderSysID:%s, ExchangeID:%s", 
    pRspInfo->ErrorID, pRspInfo->ErrorMsg, 
    pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID);

	SendExecutionReport(pInputOrder, pRspInfo);
}

void CSgitTradeSpi::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
  LOG(INFO_LOG_LEVEL, "ErrorID:%d,ErrorMsg:%s,OrderActionRef:%s,OrderRef:%s,OrderSysID:%s,ActionFlag:%c,VolumeChange:%d", 
    pRspInfo->ErrorID, pRspInfo->ErrorMsg, pOrderAction->OrderActionRef, pOrderAction->OrderRef, 
    pOrderAction->OrderSysID, pOrderAction->ActionFlag, pOrderAction->VolumeChange);

	//SendOrderCancelReject(pOrderAction, pRspInfo);
}

void CSgitTradeSpi::Init()
{
  AutoPtr<IniFileConfiguration> apSgitConf = new IniFileConfiguration(m_ssSgitCfgPath);
  m_ssPassword = apSgitConf->getString(m_ssTradeID + ".PassWord");
  m_enSymbolType = (Convert::EnCvtType)apSgitConf->getInt(m_ssTradeID + ".SymbolType");
}

void CSgitTradeSpi::SendExecutionReport(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
	std::string ssClOrdID = "";
	if (!GetClOrdID(pInputOrder->OrderRef, ssClOrdID)) return;

	std::string& ssUUid = CToolkit::GetUuid();
	LOG(INFO_LOG_LEVEL, "OrderRef:%s,ClOrdID:%s,OrderSysID:%s,uuid:%s", 
		pInputOrder->OrderRef, ssClOrdID.c_str(), pInputOrder->OrderSysID, ssUUid.c_str());

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
	executionReport.set(FIX::ClOrdID(ssClOrdID));
	executionReport.set(FIX::OrderQty(pInputOrder->VolumeTotalOriginal));
	executionReport.set(FIX::LastShares(0));
	executionReport.set(FIX::LastPx(0));

	executionReport.set(FIX::ExecTransType(FIX::ExecTransType_NEW));
	executionReport.set(FIX::OrdStatus(isSuccess ? FIX::OrdStatus_NEW : FIX::OrdStatus_REJECTED));
	executionReport.set(FIX::ExecType(isSuccess ? FIX::ExecType_NEW : FIX::ExecType_REJECTED));

	if(!isSuccess)
	{
		executionReport.set(FIX::OrdRejReason(FIX::OrdRejReason_BROKER_OPTION));
		executionReport.set(FIX::Text(format("errID:%d,errMsg:%s", pRspInfo->ErrorID, (std::string)(pRspInfo->ErrorMsg))));
	}

	m_pSgitCtx->Send(pInputOrder->InvestorID, executionReport);
}

void CSgitTradeSpi::SendOrderCancelReject(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo, const std::string& ssClOrdID, 
	const std::string& ssOrigClOrdID)
{
	//OrderCancelReject(
	//	const FIX::OrderID& aOrderID,
	//	const FIX::ClOrdID& aClOrdID,
	//	const FIX::OrigClOrdID& aOrigClOrdID,
	//	const FIX::OrdStatus& aOrdStatus,
	//	const FIX::CxlRejResponseTo& aCxlRejResponseTo )
	//std::string ssClOrdID = "";
	//if (!GetClOrdID(format(ssOrderRefFormat, pOrderAction->OrderActionRef), ssClOrdID)) return;
	FIX42::OrderCancelReject orderCancelReject = FIX42::OrderCancelReject(
		FIX::OrderID(pOrderAction->OrderSysID),
		FIX::ClOrdID(ssClOrdID),
		FIX::OrigClOrdID(ssOrigClOrdID),
		//需要传订单状态，那需要缓存订单状态？
		FIX::OrdStatus(FIX::OrdStatus_NEW),
		FIX::CxlRejResponseTo(FIX::CxlRejResponseTo_ORDER_CANCEL_REQUEST));
}

void CSgitTradeSpi::SendOrderCancelReject(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, const std::string& ssClOrdID, 
	const std::string& ssOrigClOrdID)
{

}

bool CSgitTradeSpi::GetClOrdID(const std::string& ssOrderRef, std::string& ssClOrdID)
{
	return Get(m_chOrderRef2ClOrderID, ssOrderRef, ssClOrdID);
}

bool CSgitTradeSpi::GetOrderRef(const std::string& ssClOrdID, std::string& ssOrderRef)
{
	return Get(m_chClOrderID2OrderRef, ssClOrdID, ssOrderRef);
}

bool CSgitTradeSpi::Get( ExpireCache<std::string, std::string>& oExpCache, const std::string& ssKey, std::string& ssValue)
{
	if(!oExpCache.has(ssKey))
	{
		LOG(ERROR_LOG_LEVEL, "Can not find key:%s in ExpireCache", ssKey.c_str());
		return false;
	}

	ssValue = *oExpCache.get(ssKey);
	return true;
}

void CSgitTradeSpi::AddOrderRefClOrdID(const std::string& ssOrderRef, const std::string& ssClOrdID)
{
	m_chOrderRef2ClOrderID.add(ssOrderRef, ssClOrdID);
	m_chClOrderID2OrderRef.add(ssClOrdID, ssOrderRef);
}

double CSgitTradeSpi::STUOrder::AvgPx() const
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

void CSgitTradeSpi::STUOrder::Update(const CThostFtdcOrderField& oOrder)
{
  //struct STUOrder
  //{
  //  std::string               m_ssOrderRef;//报单引用
  //  std::string		            m_ssClOrdID;//11
  //  std::string               m_ssOrigClOrdID;//41 撤单回报需要用到
  //  char					            m_cOrderStatus;//39
  //  std::string		            m_ssSymbol;//55
  //  char					            m_cSide;//54
  //  int						            m_iLeavesQty;//151
  //  int						            m_iCumQty;//14
  //  std::vector<STUTradeRec>  m_vTradeRec;
  //  //double				            m_dAvgPx;//6

  if (m_ssOrderRef.length() < 1) m_ssOrderRef = oOrder.OrderRef;
  
  
}

void CSgitTradeSpi::STUOrder::Update(const CThostFtdcTradeField& oTrade)
{

}
