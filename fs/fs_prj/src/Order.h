#ifndef __ORDER_H__
#define __ORDER_H__

#include <string>
#include "sgit/SgitFtdcTraderApi.h"
#include "Poco/Data/TypeHandler.h"

class Order
{
public:
  Order()
    : m_ssUserID("")
    , m_ssClOrdID("")
    , m_ssOrderRef("")
    , m_ssRecvAccount("")
    , m_ssRealAccount("")
    , m_ssRecvSymbol("")
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
  {}

  //Order(const Order &oOrder)
  //  : m_ssUserID(oOrder.m_ssUserID)
  //  , m_ssClOrdID(oOrder.m_ssClOrdID)
  //  , m_ssOrderRef(oOrder.m_ssClOrdID)
  //  , m_ssRecvAccount(oOrder.m_ssClOrdID)
  //  , m_ssRealAccount(oOrder.m_ssClOrdID)
  //  , m_ssRecvSymbol(oOrder.m_ssClOrdID)
  //  , m_iOrderQty(oOrder.m_ssClOrdID)
  //  , m_cOrdType(oOrder.m_ssClOrdID)
  //  , m_cSide('*')
  //  , m_cOpenClose('*')
  //  , m_dPrice(0.0)
  //  , m_cOrderStatus(FIX::OrdStatus_NEW)
  //  , m_ssOrderSysID("")
  //  , m_iLeavesQty(0)
  //  , m_iCumQty(0)
  //  , m_ssCancelClOrdID("")
  //  , m_ssOrderTime("")
  //{}

  std::string               m_ssUserID;//用户名
  std::string		            m_ssClOrdID;//11委托请求编号 撤单回报时为41
  std::string               m_ssOrderRef;//报单引用
  std::string               m_ssRecvAccount;//客户请求带的资金账号
  std::string               m_ssRealAccount;//真实资金账号
  std::string		            m_ssRecvSymbol;//55
  int                       m_iOrderQty;//38委托数量
  char                      m_cOrdType;//40报价类型
  char					            m_cSide;//54方向
  char                      m_cOpenClose;//77开平
  double                    m_dPrice;//44价格

  char					            m_cOrderStatus;//39
  std::string               m_ssOrderSysID;//37合同编号
  int						            m_iLeavesQty;//151 剩余数量
  int						            m_iCumQty;//14 累计成交数量
  std::string               m_ssCancelClOrdID;//要撤掉的原始委托请求编号 撤单回报时为11
  std::string               m_ssOrderTime;
  //double AvgPx() const;
  //void Update(const CThostFtdcInputOrderField& oInputOrder);
  ////void Update(const CThostFtdcOrderField& oOrder, const STUTdParam &stuTdParam);
  //void Update(const CThostFtdcTradeField& oTrade);

  //double Order::AvgPx() const
  //{
  //  //double dTurnover = 0.0;
  //  //int iTotalVolume = 0;
  //  //for (std::map<std::string, STUTradeRec>::const_iterator cit = m_mapTradeRec.begin(); cit != m_mapTradeRec.end(); cit++)
  //  //{
  //  //  dTurnover += cit->second.m_dMatchPrice * cit->second.m_iMatchVolume;
  //  //  iTotalVolume += cit->second.m_iMatchVolume; 
  //  //}
  //
  //  //if (iTotalVolume == 0) return 0.0;
  //
  //  //return dTurnover / iTotalVolume;
  //
  //  return 0;
  //}
  //
  //void Order::Update(const CThostFtdcInputOrderField& oInputOrder)
  //{
  //  if(m_ssOrderSysID.empty() && strlen(oInputOrder.OrderSysID) > 0)
  //  {
  //    m_ssOrderSysID = oInputOrder.OrderSysID;
  //  }
  //}
  //
  ////void STUOrder::Update(const CThostFtdcOrderField& oOrder, const STUTdParam &stuTdParam)
  ////{
  ////  if (m_ssOrderID.empty() && strlen(oOrder.OrderSysID) > 0)
  ////  {
  ////    m_ssOrderID = oOrder.OrderSysID;
  ////  }
  ////
  ////  m_cOrderStatus = stuTdParam.m_pSgitCtx->CvtDict(FIX::FIELD::OrdStatus, oOrder.OrderStatus, Convert::Fix);
  ////  //m_iLeavesQty = oOrder.VolumeTotal;
  ////  //m_iCumQty = m_iOrderQty - m_iLeavesQty;
  ////}
  //
  //void Order::Update(const CThostFtdcTradeField& oTrade)
  //{
  //  //m_mapTradeRec[oTrade.TradeID] = STUTradeRec(oTrade.Price, oTrade.Volume);
  //
  //  //m_iCumQty += oTrade.Volume;
  //  //m_iLeavesQty = m_iOrderQty - m_iCumQty;
  //}
};

namespace Poco {
  namespace Data {

    template <>
    class TypeHandler<Order>
    {
    public:
      static std::size_t size()
      {
        return 17;
      }

      static void bind(std::size_t pos, const Order& obj, AbstractBinder::Ptr pBinder, AbstractBinder::Direction dir)
      {
        // the table is defined as Person (LastName VARCHAR(30), FirstName VARCHAR, Address VARCHAR, Age INTEGER(3))
        poco_assert_dbg (!pBinder.isNull());
        pBinder->bind(pos++, obj.m_ssUserID, dir);
        pBinder->bind(pos++, obj.m_ssClOrdID, dir);
        pBinder->bind(pos++, obj.m_ssOrderRef, dir);
        pBinder->bind(pos++, obj.m_ssRecvAccount, dir);
        pBinder->bind(pos++, obj.m_ssRealAccount, dir);
        pBinder->bind(pos++, obj.m_ssRecvSymbol, dir);
        pBinder->bind(pos++, obj.m_iOrderQty, dir);
        pBinder->bind(pos++, obj.m_cOrdType, dir);
        pBinder->bind(pos++, obj.m_cSide, dir);
        pBinder->bind(pos++, obj.m_cOpenClose, dir);
        pBinder->bind(pos++, obj.m_dPrice, dir);
        pBinder->bind(pos++, obj.m_cOrderStatus, dir);
        pBinder->bind(pos++, obj.m_ssOrderSysID, dir);
        pBinder->bind(pos++, obj.m_iLeavesQty, dir);
        pBinder->bind(pos++, obj.m_iCumQty, dir);
        pBinder->bind(pos++, obj.m_ssCancelClOrdID, dir);
        pBinder->bind(pos++, obj.m_ssOrderTime, dir);
      }

      static void prepare(std::size_t pos, const Order& obj, AbstractPreparator::Ptr pPrepare)
      {
        // no-op (SQLite is prepare-less connector)
      }

      static void extract(std::size_t pos, Order& obj, const Order& defVal, AbstractExtractor::Ptr pExt)
      {
        poco_assert_dbg (!pExt.isNull());

        if(!pExt->extract(pos++, obj.m_ssUserID)) obj.m_ssUserID = defVal.m_ssUserID;
        if(!pExt->extract(pos++, obj.m_ssClOrdID)) obj.m_ssClOrdID = defVal.m_ssClOrdID;
        if(!pExt->extract(pos++, obj.m_ssOrderRef)) obj.m_ssOrderRef = defVal.m_ssOrderRef;
        if(!pExt->extract(pos++, obj.m_ssRecvAccount)) obj.m_ssRecvAccount = defVal.m_ssRecvAccount;
        if(!pExt->extract(pos++, obj.m_ssRealAccount)) obj.m_ssRealAccount = defVal.m_ssRealAccount;
        if(!pExt->extract(pos++, obj.m_ssRecvSymbol)) obj.m_ssRecvSymbol = defVal.m_ssRecvSymbol;
        if(!pExt->extract(pos++, obj.m_iOrderQty)) obj.m_iOrderQty = defVal.m_iOrderQty;
        if(!pExt->extract(pos++, obj.m_cOrdType)) obj.m_cOrdType = defVal.m_cOrdType;
        if(!pExt->extract(pos++, obj.m_cSide)) obj.m_cSide = defVal.m_cSide;
        if(!pExt->extract(pos++, obj.m_cOpenClose)) obj.m_cOpenClose = defVal.m_cOpenClose;
        if(!pExt->extract(pos++, obj.m_dPrice)) obj.m_dPrice = defVal.m_dPrice;
        if(!pExt->extract(pos++, obj.m_cOrderStatus)) obj.m_cOrderStatus = defVal.m_cOrderStatus;
        if(!pExt->extract(pos++, obj.m_ssOrderSysID)) obj.m_ssOrderSysID = defVal.m_ssOrderSysID;
        if(!pExt->extract(pos++, obj.m_iLeavesQty)) obj.m_iLeavesQty = defVal.m_iLeavesQty;
        if(!pExt->extract(pos++, obj.m_iCumQty)) obj.m_iCumQty = defVal.m_iCumQty;
        if(!pExt->extract(pos++, obj.m_ssCancelClOrdID)) obj.m_ssCancelClOrdID = defVal.m_ssCancelClOrdID;
        if(!pExt->extract(pos++, obj.m_ssOrderTime)) obj.m_ssOrderTime = defVal.m_ssOrderTime;
      }

    private:
      TypeHandler();
      ~TypeHandler();
      TypeHandler(const TypeHandler&);
      TypeHandler& operator=(const TypeHandler&);
    };


  } } // namespace Poco::Data

#endif // __ORDER_H__

