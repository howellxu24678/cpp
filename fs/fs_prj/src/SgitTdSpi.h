#ifndef __SGITTRADESPI_H__
#define __SGITTRADESPI_H__

#include <fstream>
#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/ExpireCache.h"
#include "Poco/RWLock.h"

#include "sgit/SgitFtdcTraderApi.h"

#include "quickfix/fix42/NewOrderSingle.h"
#include "quickfix/fix42/OrderCancelRequest.h"
#include "quickfix/fix42/OrderStatusRequest.h"

#include "Convert.h"
#include "Const.h"

using namespace fstech;

using namespace Poco;
using namespace Poco::Util;

const std::string ssOrderRefFormat = "%012d";

class CSgitContext;
class CSgitTdSpi : public CThostFtdcTraderSpi
{
public:
  struct STUTdParam
  {
    STUTdParam()
      : m_pSgitCtx(NULL)
      , m_pTdReqApi(NULL)
      , m_ssUserId("")
      , m_ssPassword("")
      , m_ssSessionID("")
			, m_ssDataPath("")
    {}

    CSgitContext        *m_pSgitCtx;
    CThostFtdcTraderApi *m_pTdReqApi;
    std::string         m_ssUserId;
    std::string         m_ssPassword;
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
    std::string               m_ssAccout;//资金账号（真实）
    std::string               m_ssOrderRef;//报单引用
    std::string               m_ssOrderID;//37合同编号
		std::string		            m_ssClOrdID;//11委托编号(撤单回报时为41)
    std::string               m_ssCancelClOrdID;//撤单请求编号 撤单回报时为11
		char					            m_cOrderStatus;//39
		std::string		            m_ssSymbol;//55
		char					            m_cSide;//54
    int                       m_iOrderQty;//38委托数量
    double                    m_dPrice;//44价格
		int						            m_iLeavesQty;//151
		int						            m_iCumQty;//14
    std::vector<STUTradeRec>  m_vTradeRec;

		STUOrder();
    double AvgPx() const;
    void Update(const CThostFtdcInputOrderField& oInputOrder);
    void Update(const CThostFtdcOrderField& oOrder);
    void Update(const CThostFtdcTradeField& oTrade);
	};

  //CSgitTdSpi(CSgitContext *pSgitCtx, CThostFtdcTraderApi *pReqApi, const std::string &ssUserId, const std::string &ssPassword);
  CSgitTdSpi(const STUTdParam &stuTdParam);
  virtual ~CSgitTdSpi();

  virtual bool Init();

  void OnMessage(const FIX::Message& oMsg, const FIX::SessionID& oSessionID);

protected:
  bool LoadConfig();

  virtual bool LoadConfig(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp) = 0;

	virtual Convert::EnCvtType GetSymbolType(const std::string &ssRealAcct) = 0;

	virtual void SetSymbolType(const std::string &ssRealAcct, Convert::EnCvtType enSymbolType) = 0;

  virtual bool LoadAcctAlias(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp);

	std::string GetRealAccont(const FIX::Message& oRecvMsg);

	bool AddOrderRefClOrdID(const std::string& ssOrderRef, const std::string& ssClOrdID, std::string& ssErrMsg);

	//bool GetClOrdID(const std::string& ssOrderRef, std::string& ssClOrdID);

	bool GetOrderRef(const std::string& ssClOrdID, std::string& ssOrderRef);

	bool Get( std::map<std::string, std::string> &oMap, const std::string& ssKey, std::string& ssValue);

	void SendExecutionReport(const STUOrder& oStuOrder, int iErrCode = 0, const std::string& ssErrMsg = "", bool bIsPendingCancel = false);

	void SendExecutionReport(const std::string& ssOrderRef, int iErrCode = 0, const std::string& ssErrMsg = "");

	void SendOrderCancelReject(const std::string& ssOrderRef, int iErrCode, const std::string& ssErrMsg);

	void SendOrderCancelReject(const STUOrder& oStuOrder, int iErrCode, const std::string& ssErrMsg);

	void SendOrderCancelReject(const FIX42::OrderCancelRequest& oOrderCancel, const std::string& ssErrMsg);

	bool Cvt(const FIX42::NewOrderSingle& oNewOrderSingle, CThostFtdcInputOrderField& stuInputOrder, STUOrder& stuOrder, std::string& ssErrMsg);

	bool Cvt(const FIX42::OrderCancelRequest& oOrderCancel, CThostFtdcInputOrderActionField& stuInputOrderAction, std::string& ssErrMsg);

	bool GetStuOrder(const std::string &ssOrderRef, STUOrder &stuOrder);

	std::string GetOrderRefDatFileName();

	bool LoadDatFile();

	void WriteDatFile(const std::string &ssOrderRef, const std::string &ssClOrdID);

	///报单录入请求
	void ReqOrderInsert(const FIX42::NewOrderSingle& oNewOrderSingle);

  void ReqOrderAction(const FIX42::OrderCancelRequest& oOrderCancel);

  void ReqQryOrder(const FIX42::OrderStatusRequest& oOrderStatusRequest);

  ///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
  virtual void OnFrontConnected();

  ///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
  ///@param nReason 错误原因
  ///        0x1001 网络读失败
  ///        0x1002 网络写失败
  ///        0x2001 接收心跳超时
  ///        0x2002 发送心跳失败
  ///        0x2003 收到错误报文
  virtual void OnFrontDisconnected(int nReason){};

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
  virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询资金账户响应
  virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询投资者响应
  virtual void OnRspQryInvestor(CThostFtdcInvestorField *pInvestor, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询交易编码响应
  virtual void OnRspQryTradingCode(CThostFtdcTradingCodeField *pTradingCode, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询合约保证金率响应
  virtual void OnRspQryInstrumentMarginRate(CThostFtdcInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询合约手续费率响应
  virtual void OnRspQryInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询交易所响应
  virtual void OnRspQryExchange(CThostFtdcExchangeField *pExchange, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询产品响应
  virtual void OnRspQryProduct(CThostFtdcProductField *pProduct, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///请求查询合约响应
  virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

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
private:
  //CThostFtdcTraderApi											*m_pTdReqApi;
  //CSgitContext														*m_pSgitCtx;
  //std::string															m_ssUserId;
  //std::string															m_ssPassword;

  AtomicCounter														m_acRequestId;

	AtomicCounter														m_acOrderRef;
  //考虑到程序如果需要长时间不重启运行，需要使用超时缓存，否则，可用map替代
	//OrderRef -> ClOrderID (报单引用->fix本地报单编号)
	std::map<std::string, std::string>			m_mapOrderRef2ClOrdID;

	//ClOrderID -> OrderRef (fix本地报单编号->报单引用)
	std::map<std::string, std::string>			m_mapClOrdID2OrderRef;

  //OrderRef -> STUOrder (报单引用->委托)
  std::map<std::string, STUOrder>					m_mapOrderRef2Order;

  //账户别名->真实账户
  std::map<std::string, std::string>      m_mapAcctAlias2Real;

	std::fstream														m_fOrderRef2ClOrdID;
	Poco::FastMutex													m_fastMutexOrderRef2ClOrdID;
};

//处理经过hub转发
class CSgitTdSpiHubTran : public CSgitTdSpi
{
public:
  CSgitTdSpiHubTran(const STUTdParam &stuTdParam);
  virtual ~CSgitTdSpiHubTran();

  bool LoadConfig(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp);

	Convert::EnCvtType GetSymbolType(const std::string &ssRealAcct);

	void SetSymbolType(const std::string &ssRealAcct, Convert::EnCvtType enSymbolType);
private:
  //真实资金账号->代码类别
  std::map<std::string, Convert::EnCvtType>   m_mapRealAcct2SymbolType;

  //真实资金账号->SessionKey
  std::map<std::string, std::string>          m_mapRealAcct2SessionKey;
};


//处理直连
class CSgitTdSpiDirect : public CSgitTdSpi
{
public:
  CSgitTdSpiDirect(const STUTdParam &stuTdParam);
  virtual ~CSgitTdSpiDirect();

  bool LoadConfig(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp);

	Convert::EnCvtType GetSymbolType(const std::string &ssRealAcct);

	void SetSymbolType(const std::string &ssRealAcct, Convert::EnCvtType enSymbolType);
private:

  Convert::EnCvtType											m_enSymbolType;
};
#endif // __SGITTRADESPI_H__
