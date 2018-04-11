#ifndef __ORDER_H__
#define __ORDER_H__

#include <string>
#include <map>
#include "sgit/SgitFtdcTraderApi.h"
#include "Poco/Data/TypeHandler.h"


using namespace fstech;

struct STUTradeRec
{
  STUTradeRec();
  STUTradeRec(double dPrice, int iVolume);

  double  m_dMatchPrice;  //成交价格
  int     m_iMatchVolume; //成交数量
};

//oNewOrderSingle.get(account);
//oNewOrderSingle.get(clOrdID);
//oNewOrderSingle.get(symbol);
//oNewOrderSingle.get(orderQty);
//oNewOrderSingle.get(ordType);
//oNewOrderSingle.get(side);
//oNewOrderSingle.get(openClose);

class Order
{
public:
  std::string               m_ssUserID;//用户名
  std::string		            m_ssClOrdID;//11委托请求编号 撤单回报时为41
  std::string               m_ssOrderRef;//报单引用
  std::string               m_ssRecvAccount;//客户请求带的资金账号
  std::string               m_ssRealAccount;//真实资金账号
  std::string		            m_ssSymbol;//55
  int                       m_iOrderQty;//38委托数量
  char                      m_cOrdType;//40报价类型
  char					            m_cSide;//54方向
  char                      m_cOpenClose;//77开平
  double                    m_dPrice;//44价格

  char					            m_cOrderStatus;//39
  std::string               m_ssOrderSysID;//37合同编号
  int						            m_iLeavesQty;//151
  int						            m_iCumQty;//14
  std::string               m_ssCancelClOrdID;//要撤掉的原始委托请求编号 撤单回报时为11
  std::string               m_ssOrderTime;
  
  ////成交记录
  //std::map<std::string, STUTradeRec> m_mapTradeRec;

  Order();

  double AvgPx() const;
  void Update(const CThostFtdcInputOrderField& oInputOrder);
  //void Update(const CThostFtdcOrderField& oOrder, const STUTdParam &stuTdParam);
  void Update(const CThostFtdcTradeField& oTrade);
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
        pBinder->bind(pos++, obj.m_ssSymbol, dir);
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
        if(!pExt->extract(pos++, obj.m_ssSymbol)) obj.m_ssSymbol = defVal.m_ssSymbol;
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

