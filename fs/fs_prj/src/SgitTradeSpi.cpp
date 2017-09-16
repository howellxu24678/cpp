#include "SgitTradeSpi.h"
#include "SgitApiManager.h"
#include "Log.h"

CSgitTradeSpi::CSgitTradeSpi(CSgitContext *pMgr, CThostFtdcTraderApi *pReqApi, const std::string &ssSgitCfgPath, const std::string &ssTradeId)
  : m_pMgr(pMgr)
  , m_pTradeApi(pReqApi)
  , m_ssTradeID(ssTradeId)
{
	m_apSgitConf = new IniFileConfiguration(ssSgitCfgPath);


  //FIX::SessionID oSessionID = FIX::SessionID(
  //  apSgitConf->getString(*it + ".BeginString"), 
  //  apSgitConf->getString(*it + ".SenderCompID"), 
  //  apSgitConf->getString(*it + ".TargetCompID"));
  //std::string ssOnBehalfOfCompID = apSgitConf->hasProperty(*it + ".OnBehalfOfCompID") ? 
  //  apSgitConf->getString(*it + ".OnBehalfOfCompID") : "";
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
	strncpy(stuLogin.Password, m_apSgitConf->getString(m_ssTradeID + ".PassWord").c_str(), sizeof(stuLogin.Password));
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
	FIX::SenderCompID senderCompId;
	FIX::OnBehalfOfCompID  onBehalfOfCompId;
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
  strncpy(stuInputOrder.InvestorID, m_pMgr->GetRealAccont(oNewOrderSingleMsg).c_str(), sizeof(stuInputOrder.InvestorID));
  strncpy(stuInputOrder.OrderRef, clOrdID.getValue().c_str(), sizeof(stuInputOrder.OrderRef));
	
  strncpy(stuInputOrder.InstrumentID, symbol.getValue().c_str(), sizeof(stuInputOrder.InstrumentID));
  stuInputOrder.VolumeTotalOriginal = (int)orderQty.getValue();
	stuInputOrder.OrderPriceType = m_pMgr->GetCvtDict(ordType.getField(), ordType.getValue(), Convert::Normal);
  stuInputOrder.LimitPrice = price.getValue();
	stuInputOrder.Direction = side.getValue();
	//stuInputOrder.CombOffsetFlag[0] = openClose.getValue();
  stuInputOrder.CombOffsetFlag[0] = '0';
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
}

void CSgitTradeSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	LOG(INFO_LOG_LEVEL, __FUNCTION__);
}

void CSgitTradeSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	LOG(INFO_LOG_LEVEL, "%s", __FUNCTION__, pOrder);
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
