#ifndef __SGITTRADESPI_H__
#define __SGITTRADESPI_H__

#include <fstream>

#include "Convert.h"

#include "sgit/SgitFtdcTraderApi.h"

#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/ExpireCache.h"
#include "Poco/RWLock.h"
#include "Poco/Event.h"

#include "quickfix/fix42/NewOrderSingle.h"
#include "quickfix/fix42/OrderCancelRequest.h"
#include "quickfix/fix42/OrderStatusRequest.h"

#include "Const.h"

using namespace fstech;

using namespace Poco;
using namespace Poco::Util;

const std::string ssOrderRefFormat = "%012d";

namespace FIX
{
  USER_DEFINE_INT(ReqID, 13002);
  USER_DEFINE_NUMINGROUP(ListCount, 13003);
  //USER_DEFINE_EXCHANGE(MktID, 13004);
  USER_DEFINE_CHAR(RejectReason, 13005);

  USER_DEFINE_NUMINGROUP(FieldCount, 13012);
  USER_DEFINE_INT(CapitalFieldType, 13013);
  USER_DEFINE_AMT(CapitalFieldValue, 13014);
  USER_DEFINE_INT(PositionFieldType, 13015);
  USER_DEFINE_AMT(PositionFieldValue, 13016);
  USER_DEFINE_PRICE(LongPositionPrice, 13017);
  USER_DEFINE_PRICE(SortPositionPrice, 13018);

  class AccountGroup: public FIX::Group
  {
  public:
    AccountGroup() : FIX::Group(13003, 1, FIX::message_order(1, 109, 207, 0)) {}
    FIELD_SET(*this, FIX::Account);
    FIELD_SET(*this, FIX::ClientID);
    FIELD_SET(*this, FIX::SecurityExchange);
  };

  class CapitalFiledGroup : public FIX::Group
  {
  public:
    CapitalFiledGroup() : FIX::Group(13012, 13013, FIX::message_order(13013, 13014, 0)){}
    FIELD_SET(*this, FIX::CapitalFieldType);
    FIELD_SET(*this, FIX::CapitalFieldValue);
  };

  class PositionFieldGroup : public FIX::Group
  {
  public:
    PositionFieldGroup() : FIX::Group(13012, 13015, FIX::message_order(13015, 13016, 0)){}
    FIELD_SET(*this, FIX::PositionFieldType);
    FIELD_SET(*this, FIX::PositionFieldValue);
  };

  class ContractGroup : public FIX::Group
  {
  public:
    ContractGroup() : FIX::Group(13003, 55){}
  };
}

/*
执行回报推送的流程：1.报单引用-》委托单(包括成交信息)-》资金账号
资金账号-》FIX路由信息
*/
class CSgitContext;
class CSgitTdSpi : public CThostFtdcTraderSpi
{
public:
  struct STUTdParam
  {
    STUTdParam()
      : m_pSgitCtx(NULL)
      , m_pTdReqApi(NULL)
      //, m_ssUserId("")
      //, m_ssPassword("")
      , m_ssSessionID("")
			, m_ssDataPath("")
    {}

    CSgitContext        *m_pSgitCtx;
    CThostFtdcTraderApi *m_pTdReqApi;
    //std::string         m_ssUserId;
    //std::string         m_ssPassword;
    std::string         m_ssSessionID;
    std::string         m_ssSgitCfgPath;
		std::string					m_ssDataPath;
  };

  enum EnTdSpiRole {HubTran, Direct};

  struct STUTradeRec
  {
    STUTradeRec();
    STUTradeRec(double dPrice, int iVolume);

    double  m_dMatchPrice;  //成交价格
    int     m_iMatchVolume; //成交数量
  };

	struct STUOrder
	{
		std::string								m_ssRecvAccount;//客户请求带的资金账号
		std::string               m_ssRealAccount;//真实资金账号
    std::string               m_ssOrderRef;//报单引用
    std::string               m_ssOrderID;//37合同编号
		std::string		            m_ssClOrdID;//11委托请求编号 撤单回报时为41
    std::string               m_ssCancelClOrdID;//要撤掉的原始委托请求编号 撤单回报时为11
		char					            m_cOrderStatus;//39
		std::string		            m_ssSymbol;//55
		char					            m_cSide;//54
    int                       m_iOrderQty;//38委托数量
    double                    m_dPrice;//44价格
		int						            m_iLeavesQty;//151
		int						            m_iCumQty;//14
		//成交记录
		std::map<std::string, STUTradeRec> m_mapTradeRec;

		STUOrder();
    double AvgPx() const;
    void Update(const CThostFtdcInputOrderField& oInputOrder);
    void Update(const CThostFtdcOrderField& oOrder, const STUTdParam &stuTdParam);
    void Update(const CThostFtdcTradeField& oTrade);
	};

  CSgitTdSpi(const STUTdParam &stuTdParam);
  virtual ~CSgitTdSpi();

  virtual bool Init();

  void OnMessage(const FIX::Message& oMsg, const FIX::SessionID& oSessionID);

  virtual bool ReqUserLogin(const std::string &ssUserID, const std::string &ssPassword, std::string &ssErrMsg);

  virtual void ReqUserLogout(){}
protected:
  bool LoadConfig();

  bool LoadUserInfo(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp, Poco::SharedPtr<STUserInfo> spUserInfo);

  virtual bool LoadConfig(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp) = 0;

	virtual Convert::EnCvtType GetSymbolType(const std::string &ssRealAcct) = 0;

  virtual Poco::SharedPtr<STUserInfo> GetUserInfo(const std::string &ssRealAcct) = 0;

	void SendByRealAcct(const std::string &ssRealAcct, FIX::Message& oMsg);

  void Send(int iReqID, FIX::Message& oMsg);

  virtual bool LoadAcctAlias(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp);

	std::string GetRealAccont(const FIX::Message& oRecvMsg);

	bool AddOrderRefClOrdID(const std::string& ssOrderRef, const std::string& ssClOrdID, std::string& ssErrMsg);

	//代码类型校验
	bool CheckIdSource(const FIX::Message& oRecvMsg, Convert::EnCvtType &enSymbolType, std::string& ssErrMsg);

	bool GetClOrdID(const std::string& ssOrderRef, std::string& ssClOrdID);

	bool GetOrderRef(const std::string& ssClOrdID, std::string& ssOrderRef);

	bool Get( std::map<std::string, std::string> &oMap, const std::string& ssKey, std::string& ssValue);

	void SendExecutionReport(const STUOrder& stuOrder, int iErrCode = 0, const std::string& ssErrMsg = "", bool bIsPendingCancel = false, bool bIsQueryRsp = false);

	void SendExecutionReport(const std::string& ssOrderRef, int iErrCode = 0, const std::string& ssErrMsg = "");

  void SendExecutionReport(const FIX42::OrderStatusRequest& oOrderStatusRequest, int iErrCode, const std::string& ssErrMsg);

  void SendExecutionReport(const FIX42::NewOrderSingle& oNewOrderSingle);

	void SendOrderCancelReject(const std::string& ssOrderRef, int iErrCode, const std::string& ssErrMsg);

	void SendOrderCancelReject(const STUOrder& stuOrder, int iErrCode, const std::string& ssErrMsg);

	void SendOrderCancelReject(const FIX42::OrderCancelRequest& oOrderCancel, const std::string& ssErrMsg);

	bool Cvt(const FIX42::NewOrderSingle& oNewOrderSingle, CThostFtdcInputOrderField& stuInputOrder, STUOrder& stuOrder, std::string& ssErrMsg);

	bool Cvt(const FIX42::OrderCancelRequest& oOrderCancel, CThostFtdcInputOrderActionField& stuInputOrderAction, std::string& ssErrMsg);

	void  Cvt(const CThostFtdcOrderField &stuFtdcOrder, STUOrder &stuOrder);

	Poco::SharedPtr<CSgitTdSpi::STUOrder> GetStuOrder(const std::string &ssOrderRef);

	std::string GetOrderRefDatFileName();

	bool LoadDatFile();

	void WriteDatFile(const std::string &ssOrderRef, const std::string &ssClOrdID);

  void UpsertOrder(const CThostFtdcOrderField &stuFtdcOrder, STUOrder &stuOrder);

	///报单录入请求
	void ReqOrderInsert(const FIX42::NewOrderSingle& oNewOrderSingle);

  void ReqOrderAction(const FIX42::OrderCancelRequest& oOrderCancel);

  void ReqQryOrder(const FIX42::OrderStatusRequest& oOrderStatusRequest);

  void ReqAccountQuery(const FIX42::Message& oMessage);

  void ReqCapitalQuery(const FIX42::Message& oMessage);

  void ReqPositionQuery(const FIX42::Message& oMessage);

  void ReqContractQuery(const FIX42::Message& oMessage);

  void AppendQryData(const FIX::MsgType &oMsgType, char *pRspData, int iDataSize, CThostFtdcRspInfoField *pRspInfo, int iReqID, bool bIsLast);

  void GenAndSend(const FIX::MsgType &oMsgType, int iDataSize, int iReqID);

  ///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
  virtual void OnFrontConnected();

  ///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
  ///@param nReason 错误原因
  ///        0x1001 网络读失败
  ///        0x1002 网络写失败
  ///        0x2001 接收心跳超时
  ///        0x2002 发送心跳失败
  ///        0x2003 收到错误报文
  virtual void OnFrontDisconnected(int nReason);

  ///心跳超时警告。当长时间未收到报文时，该方法被调用。
  ///@param nTimeLapse 距离上次接收报文的时间
  virtual void OnHeartBeatWarning(int nTimeLapse){};

  ///客户端认证响应
  virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///登录请求响应
  virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///登出请求响应
  virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///用户口令更新请求响应
  virtual void OnRspUserPasswordUpdate(CThostFtdcUserPasswordUpdateField *pUserPasswordUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///资金账户口令更新请求响应
  virtual void OnRspTradingAccountPasswordUpdate(CThostFtdcTradingAccountPasswordUpdateField *pTradingAccountPasswordUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///报单录入请求响应
  virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///预埋单录入请求响应
  virtual void OnRspParkedOrderInsert(CThostFtdcParkedOrderField *pParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///预埋撤单录入请求响应
  virtual void OnRspParkedOrderAction(CThostFtdcParkedOrderActionField *pParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///报单操作请求响应
  virtual void OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///查询最大报单数量响应
  virtual void OnRspQueryMaxOrderVolume(CThostFtdcQueryMaxOrderVolumeField *pQueryMaxOrderVolume, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///投资者结算结果确认响应
  virtual void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///删除预埋单响应
  virtual void OnRspRemoveParkedOrder(CThostFtdcRemoveParkedOrderField *pRemoveParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///删除预埋撤单响应
  virtual void OnRspRemoveParkedOrderAction(CThostFtdcRemoveParkedOrderActionField *pRemoveParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///执行宣告录入请求响应
  virtual void OnRspExecOrderInsert(CThostFtdcInputExecOrderField *pInputExecOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///执行宣告操作请求响应
  virtual void OnRspExecOrderAction(CThostFtdcInputExecOrderActionField *pInputExecOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///询价录入请求响应
  virtual void OnRspForQuoteInsert(CThostFtdcInputForQuoteField *pInputForQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///报价录入请求响应
  virtual void OnRspQuoteInsert(CThostFtdcInputQuoteField *pInputQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///报价操作请求响应
  virtual void OnRspQuoteAction(CThostFtdcInputQuoteActionField *pInputQuoteAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///批量报单操作请求响应
  virtual void OnRspBatchOrderAction(CThostFtdcInputBatchOrderActionField *pInputBatchOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///申请组合录入请求响应
  virtual void OnRspCombActionInsert(CThostFtdcInputCombActionField *pInputCombAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询报单响应
  virtual void OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///请求查询成交响应
  virtual void OnRspQryTrade(CThostFtdcTradeField *pTrade, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询投资者持仓响应
  virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///请求查询资金账户响应
  virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///请求查询投资者响应
  virtual void OnRspQryInvestor(CThostFtdcInvestorField *pInvestor, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询交易编码响应
  virtual void OnRspQryTradingCode(CThostFtdcTradingCodeField *pTradingCode, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///请求查询合约保证金率响应
  virtual void OnRspQryInstrumentMarginRate(CThostFtdcInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询合约手续费率响应
  virtual void OnRspQryInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询交易所响应
  virtual void OnRspQryExchange(CThostFtdcExchangeField *pExchange, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询产品响应
  virtual void OnRspQryProduct(CThostFtdcProductField *pProduct, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询合约响应
  virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///请求查询行情响应
  virtual void OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询投资者结算结果响应
  virtual void OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询转帐银行响应
  virtual void OnRspQryTransferBank(CThostFtdcTransferBankField *pTransferBank, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询投资者持仓明细响应
  virtual void OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询客户通知响应
  virtual void OnRspQryNotice(CThostFtdcNoticeField *pNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询结算信息确认响应
  virtual void OnRspQrySettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询投资者持仓明细响应
  virtual void OnRspQryInvestorPositionCombineDetail(CThostFtdcInvestorPositionCombineDetailField *pInvestorPositionCombineDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///查询保证金监管系统经纪公司资金账户密钥响应
  virtual void OnRspQryCFMMCTradingAccountKey(CThostFtdcCFMMCTradingAccountKeyField *pCFMMCTradingAccountKey, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询仓单折抵信息响应
  virtual void OnRspQryEWarrantOffset(CThostFtdcEWarrantOffsetField *pEWarrantOffset, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询投资者品种/跨品种保证金响应
  virtual void OnRspQryInvestorProductGroupMargin(CThostFtdcInvestorProductGroupMarginField *pInvestorProductGroupMargin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询交易所保证金率响应
  virtual void OnRspQryExchangeMarginRate(CThostFtdcExchangeMarginRateField *pExchangeMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询交易所调整保证金率响应
  virtual void OnRspQryExchangeMarginRateAdjust(CThostFtdcExchangeMarginRateAdjustField *pExchangeMarginRateAdjust, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询汇率响应
  virtual void OnRspQryExchangeRate(CThostFtdcExchangeRateField *pExchangeRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询二级代理操作员银期权限响应
  virtual void OnRspQrySecAgentACIDMap(CThostFtdcSecAgentACIDMapField *pSecAgentACIDMap, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询产品报价汇率
  virtual void OnRspQryProductExchRate(CThostFtdcProductExchRateField *pProductExchRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询产品组
  virtual void OnRspQryProductGroup(CThostFtdcProductGroupField *pProductGroup, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询期权交易成本响应
  virtual void OnRspQryOptionInstrTradeCost(CThostFtdcOptionInstrTradeCostField *pOptionInstrTradeCost, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询期权合约手续费响应
  virtual void OnRspQryOptionInstrCommRate(CThostFtdcOptionInstrCommRateField *pOptionInstrCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询执行宣告响应
  virtual void OnRspQryExecOrder(CThostFtdcExecOrderField *pExecOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询询价响应
  virtual void OnRspQryForQuote(CThostFtdcForQuoteField *pForQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询报价响应
  virtual void OnRspQryQuote(CThostFtdcQuoteField *pQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询组合合约安全系数响应
  virtual void OnRspQryCombInstrumentGuard(CThostFtdcCombInstrumentGuardField *pCombInstrumentGuard, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询申请组合响应
  virtual void OnRspQryCombAction(CThostFtdcCombActionField *pCombAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询转帐流水响应
  virtual void OnRspQryTransferSerial(CThostFtdcTransferSerialField *pTransferSerial, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询银期签约关系响应
  virtual void OnRspQryAccountregister(CThostFtdcAccountregisterField *pAccountregister, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///错误应答
  virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///报单通知
  virtual void OnRtnOrder(CThostFtdcOrderField *pOrder);

  ///成交通知
  virtual void OnRtnTrade(CThostFtdcTradeField *pTrade);

  ///报单录入错误回报
  virtual void OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo);

  ///报单操作错误回报
  virtual void OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo);

  ///合约交易状态通知
  virtual void OnRtnInstrumentStatus(CThostFtdcInstrumentStatusField *pInstrumentStatus) {};

  ///交易通知
  virtual void OnRtnTradingNotice(CThostFtdcTradingNoticeInfoField *pTradingNoticeInfo) {};

  ///提示条件单校验错误
  virtual void OnRtnErrorConditionalOrder(CThostFtdcErrorConditionalOrderField *pErrorConditionalOrder) {};

  ///执行宣告通知
  virtual void OnRtnExecOrder(CThostFtdcExecOrderField *pExecOrder) {};

  ///执行宣告录入错误回报
  virtual void OnErrRtnExecOrderInsert(CThostFtdcInputExecOrderField *pInputExecOrder, CThostFtdcRspInfoField *pRspInfo) {};

  ///执行宣告操作错误回报
  virtual void OnErrRtnExecOrderAction(CThostFtdcExecOrderActionField *pExecOrderAction, CThostFtdcRspInfoField *pRspInfo) {};

  ///询价录入错误回报
  virtual void OnErrRtnForQuoteInsert(CThostFtdcInputForQuoteField *pInputForQuote, CThostFtdcRspInfoField *pRspInfo) {};

  ///报价通知
  virtual void OnRtnQuote(CThostFtdcQuoteField *pQuote) {};

  ///报价录入错误回报
  virtual void OnErrRtnQuoteInsert(CThostFtdcInputQuoteField *pInputQuote, CThostFtdcRspInfoField *pRspInfo) {};

  ///报价操作错误回报
  virtual void OnErrRtnQuoteAction(CThostFtdcQuoteActionField *pQuoteAction, CThostFtdcRspInfoField *pRspInfo) {};

  ///询价通知
  virtual void OnRtnForQuoteRsp(CThostFtdcForQuoteRspField *pForQuoteRsp) {};

  ///保证金监控中心用户令牌
  virtual void OnRtnCFMMCTradingAccountToken(CThostFtdcCFMMCTradingAccountTokenField *pCFMMCTradingAccountToken) {};

  ///批量报单操作错误回报
  virtual void OnErrRtnBatchOrderAction(CThostFtdcBatchOrderActionField *pBatchOrderAction, CThostFtdcRspInfoField *pRspInfo) {};

  ///申请组合通知
  virtual void OnRtnCombAction(CThostFtdcCombActionField *pCombAction) {};

  ///申请组合录入错误回报
  virtual void OnErrRtnCombActionInsert(CThostFtdcInputCombActionField *pInputCombAction, CThostFtdcRspInfoField *pRspInfo) {};

  ///请求查询签约银行响应
  virtual void OnRspQryContractBank(CThostFtdcContractBankField *pContractBank, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询预埋单响应
  virtual void OnRspQryParkedOrder(CThostFtdcParkedOrderField *pParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询预埋撤单响应
  virtual void OnRspQryParkedOrderAction(CThostFtdcParkedOrderActionField *pParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询交易通知响应
  virtual void OnRspQryTradingNotice(CThostFtdcTradingNoticeField *pTradingNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询经纪公司交易参数响应
  virtual void OnRspQryBrokerTradingParams(CThostFtdcBrokerTradingParamsField *pBrokerTradingParams, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询经纪公司交易算法响应
  virtual void OnRspQryBrokerTradingAlgos(CThostFtdcBrokerTradingAlgosField *pBrokerTradingAlgos, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询监控中心用户令牌
  virtual void OnRspQueryCFMMCTradingAccountToken(CThostFtdcQueryCFMMCTradingAccountTokenField *pQueryCFMMCTradingAccountToken, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///银行发起银行资金转期货通知
  virtual void OnRtnFromBankToFutureByBank(CThostFtdcRspTransferField *pRspTransfer) {};

  ///银行发起期货资金转银行通知
  virtual void OnRtnFromFutureToBankByBank(CThostFtdcRspTransferField *pRspTransfer) {};

  ///银行发起冲正银行转期货通知
  virtual void OnRtnRepealFromBankToFutureByBank(CThostFtdcRspRepealField *pRspRepeal) {};

  ///银行发起冲正期货转银行通知
  virtual void OnRtnRepealFromFutureToBankByBank(CThostFtdcRspRepealField *pRspRepeal) {};

  ///期货发起银行资金转期货通知
  virtual void OnRtnFromBankToFutureByFuture(CThostFtdcRspTransferField *pRspTransfer) {};

  ///期货发起期货资金转银行通知
  virtual void OnRtnFromFutureToBankByFuture(CThostFtdcRspTransferField *pRspTransfer) {};

  ///系统运行时期货端手工发起冲正银行转期货请求，银行处理完毕后报盘发回的通知
  virtual void OnRtnRepealFromBankToFutureByFutureManual(CThostFtdcRspRepealField *pRspRepeal) {};

  ///系统运行时期货端手工发起冲正期货转银行请求，银行处理完毕后报盘发回的通知
  virtual void OnRtnRepealFromFutureToBankByFutureManual(CThostFtdcRspRepealField *pRspRepeal) {};

  ///期货发起查询银行余额通知
  virtual void OnRtnQueryBankBalanceByFuture(CThostFtdcNotifyQueryAccountField *pNotifyQueryAccount) {};

  ///期货发起银行资金转期货错误回报
  virtual void OnErrRtnBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo) {};

  ///期货发起期货资金转银行错误回报
  virtual void OnErrRtnFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo) {};

  ///系统运行时期货端手工发起冲正银行转期货错误回报
  virtual void OnErrRtnRepealBankToFutureByFutureManual(CThostFtdcReqRepealField *pReqRepeal, CThostFtdcRspInfoField *pRspInfo) {};

  ///系统运行时期货端手工发起冲正期货转银行错误回报
  virtual void OnErrRtnRepealFutureToBankByFutureManual(CThostFtdcReqRepealField *pReqRepeal, CThostFtdcRspInfoField *pRspInfo) {};

  ///期货发起查询银行余额错误回报
  virtual void OnErrRtnQueryBankBalanceByFuture(CThostFtdcReqQueryAccountField *pReqQueryAccount, CThostFtdcRspInfoField *pRspInfo) {};

  ///期货发起冲正银行转期货请求，银行处理完毕后报盘发回的通知
  virtual void OnRtnRepealFromBankToFutureByFuture(CThostFtdcRspRepealField *pRspRepeal) {};

  ///期货发起冲正期货转银行请求，银行处理完毕后报盘发回的通知
  virtual void OnRtnRepealFromFutureToBankByFuture(CThostFtdcRspRepealField *pRspRepeal) {};

  ///期货发起银行资金转期货应答
  virtual void OnRspFromBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///期货发起期货资金转银行应答
  virtual void OnRspFromFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///期货发起查询银行余额应答
  virtual void OnRspQueryBankAccountMoneyByFuture(CThostFtdcReqQueryAccountField *pReqQueryAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///银行发起银期开户通知
  virtual void OnRtnOpenAccountByBank(CThostFtdcOpenAccountField *pOpenAccount) {};

  ///银行发起银期销户通知
  virtual void OnRtnCancelAccountByBank(CThostFtdcCancelAccountField *pCancelAccount) {};

  ///银行发起变更银行账号通知
  virtual void OnRtnChangeAccountByBank(CThostFtdcChangeAccountField *pChangeAccount) {};

  /// 当收到合约价位查询应答时回调该函数
  virtual void onRspMBLQuot(CThostMBLQuotData *pMBLQuotData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

  STUTdParam                              m_stuTdParam;
  std::string                             m_ssUserID;
  AtomicCounter														m_acRequestId;
  bool                                    m_bNeedKeepLogin;   //是否需要在前置机连上的回调中自动进行用户登录
private:
  std::string                             m_ssPassword;

  Event                                   m_oEventConnected;  //前置机连接上事件
  Event                                   m_oEventLoginResp;  //登录应答事件
  bool                                    m_bLastLoginOk;     //记录最新的一次登录是否成功
  int                                     m_iLoginRespErrID;  //登录响应错误码
  std::string                             m_ssLoginRespErrMsg;//登录响应错误信息

	AtomicCounter														          m_acOrderRef;
  //考虑到程序如果需要长时间不重启运行，需要使用超时缓存，否则，可用map替代
	//OrderRef -> ClOrderID (报单引用->fix本地报单编号)
	std::map<std::string, std::string>			          m_mapOrderRef2ClOrdID;

	//ClOrderID -> OrderRef (fix本地报单编号->报单引用)
	std::map<std::string, std::string>			          m_mapClOrdID2OrderRef;

  //OrderRef -> STUOrder (报单引用->委托)
  std::map<std::string, Poco::SharedPtr<STUOrder> >  m_mapOrderRef2Order;

  //账户别名->真实账户
  std::map<std::string, std::string>                m_mapAlias2RealAcct;

	//真实账户->账户别名
	std::map<std::string, std::string>			          m_mapReal2AliasAcct;

	std::fstream														          m_fOrderRef2ClOrdID;
	Poco::FastMutex													          m_fastMutexOrderRef2ClOrdID;

  //m_acRequestId -> FIX::ReqID(13002) 默认超时10分钟             
  Poco::ExpireCache<int, FIX42::Message>            m_expchReqId2Message;

  std::vector<char>                                 m_vBuffer;
  int                                               m_iCurReqID;
};

//处理经过hub转发
class CSgitTdSpiHubTran : public CSgitTdSpi
{
public:
  CSgitTdSpiHubTran(const STUTdParam &stuTdParam);
  virtual ~CSgitTdSpiHubTran();

  bool LoadConfig(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp);

	Convert::EnCvtType GetSymbolType(const std::string &ssRealAcct);

  Poco::SharedPtr<STUserInfo> GetUserInfo(const std::string &ssRealAcct);

	//void SetSymbolType(const std::string &ssSessionKey, Convert::EnCvtType enSymbolType);

	//void Send(const std::string &ssRealAcct, FIX::Message& oMsg);
private:
  //真实账号->所用代码类型等信息 --用于交易推送
	std::map<std::string, Poco::SharedPtr<STUserInfo> >	m_mapRealAcct2UserInfo;

	////SessionKey -> 真实资金账号列表
	//std::map<std::string, std::set<std::string> >				m_mapSessionKey2AcctSet; 
};


//处理直连
class CSgitTdSpiDirect : public CSgitTdSpi
{
public:
  CSgitTdSpiDirect(const STUTdParam &stuTdParam);
  virtual ~CSgitTdSpiDirect();

  virtual bool Init();

  bool LoadConfig(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp);

	Convert::EnCvtType GetSymbolType(const std::string &ssRealAcct);

  Poco::SharedPtr<STUserInfo> GetUserInfo(const std::string &ssRealAcct);

  virtual void ReqUserLogout();

private:
  Poco::SharedPtr<STUserInfo>                   m_spUserInfo;
};
#endif // __SGITTRADESPI_H__
