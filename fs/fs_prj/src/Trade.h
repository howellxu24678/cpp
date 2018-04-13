#ifndef __TRADE_H__
#define __TRADE_H__

#include <string>
#include "Poco/Data/TypeHandler.h"
#include "sgit/SgitFtdcUserApiStruct.h"
using namespace fstech;

class Trade
{
public:
  std::string               m_ssTradeID;//成交编号
  std::string               m_ssTradingDay;//交易日
  std::string               m_ssMatchTime;//成交时间
  std::string               m_ssOrderSysID;//合同编号
  double                    m_dMatchPrice;//成交价格
  int                       m_iMatchQty;//成交数量
  std::string               m_ssUserID;//用户名
  std::string               m_ssOrderRef;//报单引用
  std::string               m_ssRealAccount;//真实资金账号  

  Trade()
    : m_ssTradeID("")
    , m_ssTradingDay("")
    , m_ssMatchTime("")
    , m_ssOrderSysID("")
    , m_dMatchPrice(0.0)
    , m_iMatchQty(0)
    , m_ssUserID("")
    , m_ssOrderRef("")
    , m_ssRealAccount("")
  {}

  Trade(const CThostFtdcTradeField &stuTrade, const std::string &ssUserID)
    : m_ssTradeID(stuTrade.TradeID)
    , m_ssTradingDay(stuTrade.TradingDay)
    , m_ssMatchTime(stuTrade.TradeTime)
    , m_ssOrderSysID(stuTrade.OrderSysID)
    , m_dMatchPrice(stuTrade.Price)
    , m_iMatchQty(stuTrade.Volume)
    , m_ssUserID(ssUserID)
    , m_ssOrderRef(stuTrade.OrderRef)
    , m_ssRealAccount(stuTrade.InvestorID)
  {}
};


namespace Poco {
  namespace Data {

    template <>
    class TypeHandler<Trade>
    {
    public:
      static std::size_t size()
      {
        return 9;
      }

      static void bind(std::size_t pos, const Trade& obj, AbstractBinder::Ptr pBinder, AbstractBinder::Direction dir)
      {
        // the table is defined as Person (LastName VARCHAR(30), FirstName VARCHAR, Address VARCHAR, Age INTEGER(3))
        poco_assert_dbg (!pBinder.isNull());
        pBinder->bind(pos++, obj.m_ssTradeID, dir);
        pBinder->bind(pos++, obj.m_ssTradingDay, dir);
        pBinder->bind(pos++, obj.m_ssMatchTime, dir);
        pBinder->bind(pos++, obj.m_ssOrderSysID, dir);
        pBinder->bind(pos++, obj.m_dMatchPrice, dir);
        pBinder->bind(pos++, obj.m_iMatchQty, dir);
        pBinder->bind(pos++, obj.m_ssUserID, dir);
        pBinder->bind(pos++, obj.m_ssOrderRef, dir);
        pBinder->bind(pos++, obj.m_ssRealAccount, dir);
      }

      static void prepare(std::size_t pos, const Trade& obj, AbstractPreparator::Ptr pPrepare)
      {
        // no-op (SQLite is prepare-less connector)
      }

      static void extract(std::size_t pos, Trade& obj, const Trade& defVal, AbstractExtractor::Ptr pExt)
      {
        poco_assert_dbg (!pExt.isNull());

        if(!pExt->extract(pos++, obj.m_ssTradeID)) obj.m_ssTradeID = defVal.m_ssTradeID;
        if(!pExt->extract(pos++, obj.m_ssTradingDay)) obj.m_ssTradingDay = defVal.m_ssTradingDay;
        if(!pExt->extract(pos++, obj.m_ssMatchTime)) obj.m_ssMatchTime = defVal.m_ssMatchTime;
        if(!pExt->extract(pos++, obj.m_ssOrderSysID)) obj.m_ssOrderSysID = defVal.m_ssOrderSysID;
        if(!pExt->extract(pos++, obj.m_dMatchPrice)) obj.m_dMatchPrice = defVal.m_dMatchPrice;
        if(!pExt->extract(pos++, obj.m_iMatchQty)) obj.m_iMatchQty = defVal.m_iMatchQty;
        if(!pExt->extract(pos++, obj.m_ssUserID)) obj.m_ssUserID = defVal.m_ssUserID;
        if(!pExt->extract(pos++, obj.m_ssOrderRef)) obj.m_ssOrderRef = defVal.m_ssOrderRef;
        if(!pExt->extract(pos++, obj.m_ssRealAccount)) obj.m_ssRealAccount = defVal.m_ssRealAccount;
      }

    private:
      TypeHandler();
      ~TypeHandler();
      TypeHandler(const TypeHandler&);
      TypeHandler& operator=(const TypeHandler&);
    };


  } } // namespace Poco::Data

#endif // __TRADE_H__
