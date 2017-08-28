#include "SgitTradeSpi.h"

CSgitTradeSpi::CSgitTradeSpi(CThostFtdcTraderApi *pReqApi)
  : m_pTradeApi(pReqApi)
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
