#include "SgitTradeSpi.h"
#include "Log.h"

CSgitTradeSpi::CSgitTradeSpi(CThostFtdcTraderApi *pReqApi, const std::string &ssSgitCfgPath, const std::string &ssTradeId)
  : m_pTradeApi(pReqApi)
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

  AbstractConfiguration::Keys keys;
  m_apSgitConf->keys("account",keys);

  CThostFtdcReqUserLoginField stuLogin;
  for (AbstractConfiguration::Keys::iterator it = keys.begin(); it != keys.end(); it++)
  {
    memset(&stuLogin, 0, sizeof(CThostFtdcReqUserLoginField));
    strncpy(stuLogin.UserID, it->c_str(), sizeof(stuLogin.UserID));
    strncpy(stuLogin.Password, m_apSgitConf->getString("account." + *it).c_str(), sizeof(stuLogin.Password));
    m_pTradeApi->ReqUserLogin(&stuLogin, m_acRequestId++);
    LOG(INFO_LOG_LEVEL, "ReqUserLogin userID:%s",stuLogin.UserID);
  }
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
	FIX::Symbol symbol;
	FIX::OrderQty orderQty;
	FIX::HandlInst handInst;
	FIX::OrdType ordType;
	FIX::Price price;
	FIX::Side side;
	FIX::OpenClose openClose;
	FIX::TransactTime transTime;
	FIX::TimeInForce timeInForce;
	//if ( ordType != FIX::OrdType_LIMIT )
	//  throw FIX::IncorrectTagValue( ordType.getField() );
	oNewOrderSingleMsg.get( account );
	oNewOrderSingleMsg.get( clOrdID );
	oNewOrderSingleMsg.get( symbol );
	oNewOrderSingleMsg.get( orderQty );
	oNewOrderSingleMsg.get( handInst );
	oNewOrderSingleMsg.get( ordType );
	oNewOrderSingleMsg.get( price );
	oNewOrderSingleMsg.get( side );

	CThostFtdcInputOrderField stuInputOrder;
	memset(&stuInputOrder, 0, sizeof(CThostFtdcInputOrderField));


	m_pTradeApi->ReqOrderInsert(&stuInputOrder, m_acRequestId++);
}
