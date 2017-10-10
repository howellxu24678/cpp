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
	//, m_chOrderRef2ClOrderID(12*60*60*1000)//��ʱ��Ϊ12Сʱ
	, m_chClOrderID2OrderRef(12*60*60*1000)//��ʱ��Ϊ12Сʱ
  , m_chOrderRef2Order(12*60*60*1000)//��ʱ��Ϊ12Сʱ
{

}

CSgitTradeSpi::~CSgitTradeSpi()
{
  //�ͷ�Api�ڴ�
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

  //fs�Ƿ���Ҫ��һ��
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
	CThostFtdcInputOrderField stuInputOrder;
	STUOrder stuOrder;
	Cvt(oNewOrderSingle, stuInputOrder, stuOrder);

	m_chOrderRef2Order.add(stuOrder.m_ssOrderRef, stuOrder);

	int iRet = m_pTradeApi->ReqOrderInsert(&stuInputOrder, stuInputOrder.RequestID);
	if (iRet != 0)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to call api ReqOrderInsert,iRet:%d", iRet);
    SendExecutionReport(stuOrder, iRet, "Failed to call api ReqOrderInsert");
	}
}

void CSgitTradeSpi::ReqOrderAction(const FIX42::OrderCancelRequest& oOrderCancel)
{
  CThostFtdcInputOrderActionField stuInputOrderAction;
	Cvt(oOrderCancel, stuInputOrderAction);

  int iRet = m_pTradeApi->ReqOrderAction(&stuInputOrderAction, stuInputOrderAction.RequestID);
	if (iRet != 0)
	{
		LOG(ERROR_LOG_LEVEL, "Failed to call api ReqOrderAction,iRet:%d", iRet);

		SendOrderCancelReject(stuInputOrderAction.OrderRef, iRet, "Failed to call api ReqOrderAction");
	}
}


void CSgitTradeSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
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

void CSgitTradeSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
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
		//[CSgitTradeSpi::OnRspOrderAction] OrderRef:000000000001,OrderSysID:00000001,VolumeChange:1,ErrorID:0,ErrorMsg:�����ɹ��� [SgitTradeSpi.cpp:121]
		//[CSgitTradeSpi::OnRtnOrder] OrderRef:000000000001,OrderSysID:00000001,OrderStatus:5,VolumeTraded:0 [SgitTradeSpi.cpp:144]
		SendExecutionReport(*spStuOrder, pRspInfo->ErrorID, pRspInfo->ErrorMsg, true);
	}
}

//����������ڴ˻ظ�ִ�лر����������ֻ���ڸ��¶���������״̬����
void CSgitTradeSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
  LOG(INFO_LOG_LEVEL, "OrderRef:%s,OrderSysID:%s,OrderStatus:%c,VolumeTraded:%d,VolumeLeave:%d",
     pOrder->OrderRef, pOrder->OrderSysID, pOrder->OrderStatus, pOrder->VolumeTraded, pOrder->VolumeTotal);

  Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(pOrder->OrderRef);
  if (spStuOrder.isNull())
  {
    LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", pOrder->OrderRef);
    return;
  }

  spStuOrder->Update(*pOrder);

	//�����Ƿ񱻾ܾ�
	bool isReject = pOrder->OrderSubmitStatus == THOST_FTDC_OSS_InsertRejected 
		|| pOrder->OrderSubmitStatus == THOST_FTDC_OSS_CancelRejected;

	//�ܾ��������ر�
	if (isReject || pOrder->OrderStatus == THOST_FTDC_OST_Canceled)
	{
		SendExecutionReport(*spStuOrder, isReject ? -1 : 0, isReject ? "Reject by Exchange" : "");
	}
}


//�յ��ɽ����ظ�ִ�лر�
void CSgitTradeSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
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

void CSgitTradeSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
  LOG(INFO_LOG_LEVEL, "ErrorID:%d, ErrorMsg:%s,OrderRef:%s, OrderSysID:%s, ExchangeID:%s", 
    pRspInfo->ErrorID, pRspInfo->ErrorMsg, 
    pInputOrder->OrderRef, pInputOrder->OrderSysID, pInputOrder->ExchangeID);

  SendExecutionReport(pInputOrder->OrderRef, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
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

void CSgitTradeSpi::SendExecutionReport(const STUOrder& oStuOrder, int iErrCode, const std::string& ssErrMsg, bool bIsPendingCancel /*= false*/)
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

  if (oStuOrder.m_vTradeRec.size() < 1)
  {
    executionReport.set(FIX::LastPx(0));
    executionReport.set(FIX::LastShares(0));
  }
  else
  {
    executionReport.set(FIX::LastPx(oStuOrder.m_vTradeRec.back().m_dMatchPrice));
    executionReport.set(FIX::LastShares(oStuOrder.m_vTradeRec.back().m_iMatchVolume));
  }

	//����Ӧ��
	if(bIsPendingCancel)
	{
		executionReport.set(FIX::OrdStatus(FIX::ExecType_PENDING_CANCEL));
		executionReport.set(FIX::ExecType(FIX::ExecType_PENDING_CANCEL));
	}
	//�д�����
  else if (iErrCode != 0)
  {
		executionReport.set(FIX::ExecType(FIX::ExecType_REJECTED));
		executionReport.set(FIX::OrdStatus(FIX::OrdStatus_REJECTED));

		executionReport.set(FIX::OrdRejReason(FIX::OrdRejReason_BROKER_OPTION));
		executionReport.set(FIX::Text(format("errID:%d,errMsg:%s", iErrCode, ssErrMsg)));
  }
	//�������
  else
  {
		char chOrderStatus = m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, oStuOrder.m_cOrderStatus, Convert::Fix);
		executionReport.set(FIX::OrdStatus(chOrderStatus));
		executionReport.set(FIX::ExecType(chOrderStatus));
  }

  m_pSgitCtx->Send(oStuOrder.m_ssAccout, executionReport);
}

void CSgitTradeSpi::SendExecutionReport(const std::string& ssOrderRef, int iErrCode /*= 0*/, const std::string& ssErrMsg /*= ""*/)
{
  Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(ssOrderRef);
  if (spStuOrder.isNull())
  {
    LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", ssOrderRef.c_str());
    return;
  }

  return SendExecutionReport(*spStuOrder, iErrCode, ssErrMsg);
}

void CSgitTradeSpi::SendOrderCancelReject(const std::string& ssOrderRef, int iErrCode, const std::string& ssErrMsg)
{
	Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(ssOrderRef);
	if (spStuOrder.isNull())
	{
		LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", ssOrderRef.c_str());
		return;
	}

	return SendOrderCancelReject(*spStuOrder, iErrCode, ssErrMsg);
}

void CSgitTradeSpi::SendOrderCancelReject(const STUOrder& oStuOrder, int iErrCode, const std::string& ssErrMsg)
{
	FIX42::OrderCancelReject orderCancelReject = FIX42::OrderCancelReject(
		FIX::OrderID(oStuOrder.m_ssOrderID),
		FIX::ClOrdID(oStuOrder.m_ssCancelClOrdID),
		FIX::OrigClOrdID(oStuOrder.m_ssClOrdID),
		FIX::OrdStatus(m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, oStuOrder.m_cOrderStatus, Convert::Fix)),
		FIX::CxlRejResponseTo(FIX::CxlRejResponseTo_ORDER_CANCEL_REQUEST));

	orderCancelReject.set(FIX::Text(ssErrMsg));

	m_pSgitCtx->Send(oStuOrder.m_ssAccout, orderCancelReject);
}

//bool CSgitTradeSpi::GetClOrdID(const std::string& ssOrderRef, std::string& ssClOrdID)
//{
//	return Get(m_chOrderRef2ClOrderID, ssOrderRef, ssClOrdID);
//}

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
	//if (m_chOrderRef2ClOrderID.has(ssOrderRef))
	//{
	//	LOG(WARN_LOG_LEVEL, "%s in m_chOrderRef2ClOrderID will be replace", ssOrderRef.c_str());
	//	m_chOrderRef2ClOrderID.update(ssOrderRef, ssClOrdID);
	//}
	//else
	//{
	//	m_chOrderRef2ClOrderID.add(ssOrderRef, ssClOrdID);
	//}

	if (m_chClOrderID2OrderRef.has(ssClOrdID))
	{
		LOG(WARN_LOG_LEVEL, "%s in m_chClOrderID2OrderRef will be replace", ssClOrdID.c_str());
		m_chClOrderID2OrderRef.update(ssClOrdID, ssOrderRef);
	}
	else
	{
		m_chClOrderID2OrderRef.add(ssClOrdID, ssOrderRef);
	}
}

void CSgitTradeSpi::Cvt(const FIX42::NewOrderSingle& oNewOrderSingle, CThostFtdcInputOrderField& stuInputOrder, STUOrder& stuOrder)
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

	memset(&stuInputOrder, 0, sizeof(CThostFtdcInputOrderField));
	//���ڲ���ȷ�������ClOrdID(11)�ϸ�������������������һ���������ò�������
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

	//STUOrder
  stuOrder.m_ssAccout = stuInputOrder.InvestorID;
	stuOrder.m_ssOrderRef = ssOrderRef;
	stuOrder.m_ssClOrdID = clOrdID.getValue();
	stuOrder.m_cOrderStatus = THOST_FTDC_OST_NoTradeQueueing;
	stuOrder.m_ssSymbol = symbol.getValue();
	stuOrder.m_cSide = side.getValue();
  stuOrder.m_iOrderQty = (int)orderQty.getValue();
  stuOrder.m_dPrice = price.getValue();
	stuOrder.m_iLeavesQty = (int)orderQty.getValue();
	stuOrder.m_iCumQty = 0;
}

void CSgitTradeSpi::Cvt(const FIX42::OrderCancelRequest& oOrderCancel, CThostFtdcInputOrderActionField& stuInputOrderAction)
{
  //���ֳ�����ϣ�1.OrderSysID+ExchangeID 2.OrderRef+UserID+InstrumentID

  //���غ�
  FIX::ClOrdID clOrdID;
  //���������غ�
  FIX::OrigClOrdID origClOrdID;
  //������ϵͳ������
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
		LOG(ERROR_LOG_LEVEL, "Can not GetOrderRef by origClOrdID:%s", origClOrdID.getValue().c_str());
    return;
  }

	//����������ID��ֵ��ԭ��ί�нṹ����
  Poco::SharedPtr<STUOrder> spStuOrder = m_chOrderRef2Order.get(ssOrderRef);
  if (spStuOrder.isNull())
  {
    LOG(ERROR_LOG_LEVEL, "Can not find orderRef:%s in cache m_chOrderRef2Order", ssOrderRef.c_str());
    return;
  }
  spStuOrder->m_ssCancelClOrdID = origClOrdID.getValue();

  memset(&stuInputOrderAction, 0, sizeof(CThostFtdcInputOrderActionField));
  stuInputOrderAction.OrderActionRef = ++m_acOrderRef;
  std::string ssOrderActionRef = format(ssOrderRefFormat, stuInputOrderAction.OrderActionRef);
  LOG(INFO_LOG_LEVEL, "OrderActionRef:%s,ClOrdID:%s", ssOrderActionRef.c_str(), clOrdID.getValue().c_str());
  //AddOrderRefClOrdID(ssOrderActionRef, clOrdID.getValue());

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
    strncpy(stuInputOrderAction.OrderRef, ssOrderRef.c_str(), sizeof(stuInputOrderAction.OrderRef));
    strncpy(stuInputOrderAction.UserID, m_ssTradeID.c_str(), sizeof(stuInputOrderAction.UserID));
    strncpy(
      stuInputOrderAction.InstrumentID, 
      m_enSymbolType == Convert::Original ? 
      symbol.getValue().c_str() : m_pSgitCtx->CvtSymbol(symbol.getValue(), Convert::Original).c_str(), 
      sizeof(stuInputOrderAction.InstrumentID));
  }

  stuInputOrderAction.RequestID = m_acRequestId++;
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

void CSgitTradeSpi::STUOrder::Update(const CThostFtdcInputOrderField& oInputOrder)
{
  if(m_ssOrderID.empty() && strlen(oInputOrder.OrderSysID) > 0)
  {
    m_ssOrderID = oInputOrder.OrderSysID;
  }
}

void CSgitTradeSpi::STUOrder::Update(const CThostFtdcOrderField& oOrder)
{
  if (m_ssOrderID.empty() && strlen(oOrder.OrderSysID) > 0)
  {
    m_ssOrderID = oOrder.OrderSysID;
  }

  m_cOrderStatus = oOrder.OrderStatus;
  m_iLeavesQty = oOrder.VolumeTotal;
  m_iCumQty = m_iOrderQty - m_iLeavesQty;
}

void CSgitTradeSpi::STUOrder::Update(const CThostFtdcTradeField& oTrade)
{
  m_vTradeRec.push_back(STUTradeRec(oTrade.Price, oTrade.Volume));
}

CSgitTradeSpi::STUOrder::STUOrder()
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

CSgitTradeSpi::STUTradeRec::STUTradeRec()
  : m_dMatchPrice(0.0)
  , m_iMatchVolume(0)
{}

CSgitTradeSpi::STUTradeRec::STUTradeRec(double dPrice, int iVolume)
  : m_dMatchPrice(dPrice)
  , m_iMatchVolume(iVolume)
{}
