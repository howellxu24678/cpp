#include "SgitTradeSpi.h"
#include "Log.h"

CSgitTradeSpi::CSgitTradeSpi(CThostFtdcTraderApi *pReqApi, std::string ssSgitCfgPath)
  : m_pTradeApi(pReqApi)
{
	m_apSgitConf = new IniFileConfiguration(ssSgitCfgPath);
	AbstractConfiguration::Keys keys;
	m_apSgitConf->keys("account",keys);
	for (AbstractConfiguration::Keys::iterator it = keys.begin(); it != keys.end(); it++)
	{
		LOG(INFO_LOG_LEVEL, "keys:%s",it->c_str());
	}
}

CSgitTradeSpi::~CSgitTradeSpi()
{
  //ÊÍ·ÅApiÄÚ´æ
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
}
