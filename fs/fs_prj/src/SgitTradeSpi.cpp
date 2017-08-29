#include "SgitTradeSpi.h"
#include "Log.h"

CSgitTradeSpi::CSgitTradeSpi(CThostFtdcTraderApi *pReqApi, std::string ssSgitCfgPath)
  : m_pTradeApi(pReqApi)
{
	m_apSgitConf = new IniFileConfiguration(ssSgitCfgPath);
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
