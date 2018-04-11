#include "Order.h"
#include "quickfix/Session.h"
//#include "quickfix/fix42/ExecutionReport.h"
//#include "quickfix/fix42/OrderCancelReject.h"



double Order::AvgPx() const
{
  //double dTurnover = 0.0;
  //int iTotalVolume = 0;
  //for (std::map<std::string, STUTradeRec>::const_iterator cit = m_mapTradeRec.begin(); cit != m_mapTradeRec.end(); cit++)
  //{
  //  dTurnover += cit->second.m_dMatchPrice * cit->second.m_iMatchVolume;
  //  iTotalVolume += cit->second.m_iMatchVolume; 
  //}

  //if (iTotalVolume == 0) return 0.0;

  //return dTurnover / iTotalVolume;

  return 0;
}

void Order::Update(const CThostFtdcInputOrderField& oInputOrder)
{
  if(m_ssOrderSysID.empty() && strlen(oInputOrder.OrderSysID) > 0)
  {
    m_ssOrderSysID = oInputOrder.OrderSysID;
  }
}

//void STUOrder::Update(const CThostFtdcOrderField& oOrder, const STUTdParam &stuTdParam)
//{
//  if (m_ssOrderID.empty() && strlen(oOrder.OrderSysID) > 0)
//  {
//    m_ssOrderID = oOrder.OrderSysID;
//  }
//
//  m_cOrderStatus = stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, oOrder.OrderStatus, Convert::Fix);
//  //m_iLeavesQty = oOrder.VolumeTotal;
//  //m_iCumQty = m_iOrderQty - m_iLeavesQty;
//}

void Order::Update(const CThostFtdcTradeField& oTrade)
{
  //m_mapTradeRec[oTrade.TradeID] = STUTradeRec(oTrade.Price, oTrade.Volume);

  //m_iCumQty += oTrade.Volume;
  //m_iLeavesQty = m_iOrderQty - m_iCumQty;
}

Order::Order()
  : m_ssUserID("")
  , m_ssClOrdID("")
  , m_ssOrderRef("")
  , m_ssRecvAccount("")
  , m_ssRealAccount("")
  , m_ssSymbol("")
  , m_iOrderQty(0)
  , m_cOrdType('*')
  , m_cSide('*')
  , m_cOpenClose('*')
  , m_dPrice(0.0)
  , m_cOrderStatus(FIX::OrdStatus_NEW)
  , m_ssOrderSysID("")
  , m_iLeavesQty(0)
  , m_iCumQty(0)
  , m_ssCancelClOrdID("")
  , m_ssOrderTime("")
{
}

STUTradeRec::STUTradeRec()
  : m_dMatchPrice(0.0)
  , m_iMatchVolume(0)
{}

STUTradeRec::STUTradeRec(double dPrice, int iVolume)
  : m_dMatchPrice(dPrice)
  , m_iMatchVolume(iVolume)
{}


