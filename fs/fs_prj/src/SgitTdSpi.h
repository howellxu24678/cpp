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

    double  m_dMatchPrice;  //�ɽ��۸�
    int     m_iMatchVolume; //�ɽ�����
  };

	struct STUOrder
	{
    std::string               m_ssAccout;//�ʽ��˺ţ���ʵ��
    std::string               m_ssOrderRef;//��������
    std::string               m_ssOrderID;//37��ͬ���
		std::string		            m_ssClOrdID;//11ί�б��(�����ر�ʱΪ41)
    std::string               m_ssCancelClOrdID;//���������� �����ر�ʱΪ11
		char					            m_cOrderStatus;//39
		std::string		            m_ssSymbol;//55
		char					            m_cSide;//54
    int                       m_iOrderQty;//38ί������
    double                    m_dPrice;//44�۸�
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

	///����¼������
	void ReqOrderInsert(const FIX42::NewOrderSingle& oNewOrderSingle);

  void ReqOrderAction(const FIX42::OrderCancelRequest& oOrderCancel);

  void ReqQryOrder(const FIX42::OrderStatusRequest& oOrderStatusRequest);

  ///���ͻ����뽻�׺�̨������ͨ������ʱ����δ��¼ǰ�����÷��������á�
  virtual void OnFrontConnected();

  ///���ͻ����뽻�׺�̨ͨ�����ӶϿ�ʱ���÷��������á���������������API���Զ��������ӣ��ͻ��˿ɲ�������
  ///@param nReason ����ԭ��
  ///        0x1001 �����ʧ��
  ///        0x1002 ����дʧ��
  ///        0x2001 ����������ʱ
  ///        0x2002 ��������ʧ��
  ///        0x2003 �յ�������
  virtual void OnFrontDisconnected(int nReason){};

  ///������ʱ���档����ʱ��δ�յ�����ʱ���÷��������á�
  ///@param nTimeLapse �����ϴν��ձ��ĵ�ʱ��
  virtual void OnHeartBeatWarning(int nTimeLapse){};

  ///�ͻ�����֤��Ӧ
  virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///��¼������Ӧ
  virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///�ǳ�������Ӧ
  virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�û��������������Ӧ
  virtual void OnRspUserPasswordUpdate(CThostFtdcUserPasswordUpdateField *pUserPasswordUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�ʽ��˻��������������Ӧ
  virtual void OnRspTradingAccountPasswordUpdate(CThostFtdcTradingAccountPasswordUpdateField *pTradingAccountPasswordUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///����¼��������Ӧ
  virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///Ԥ��¼��������Ӧ
  virtual void OnRspParkedOrderInsert(CThostFtdcParkedOrderField *pParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///Ԥ�񳷵�¼��������Ӧ
  virtual void OnRspParkedOrderAction(CThostFtdcParkedOrderActionField *pParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///��������������Ӧ
  virtual void OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///��ѯ��󱨵�������Ӧ
  virtual void OnRspQueryMaxOrderVolume(CThostFtdcQueryMaxOrderVolumeField *pQueryMaxOrderVolume, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///Ͷ���߽�����ȷ����Ӧ
  virtual void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///ɾ��Ԥ����Ӧ
  virtual void OnRspRemoveParkedOrder(CThostFtdcRemoveParkedOrderField *pRemoveParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///ɾ��Ԥ�񳷵���Ӧ
  virtual void OnRspRemoveParkedOrderAction(CThostFtdcRemoveParkedOrderActionField *pRemoveParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///ִ������¼��������Ӧ
  virtual void OnRspExecOrderInsert(CThostFtdcInputExecOrderField *pInputExecOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///ִ���������������Ӧ
  virtual void OnRspExecOrderAction(CThostFtdcInputExecOrderActionField *pInputExecOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///ѯ��¼��������Ӧ
  virtual void OnRspForQuoteInsert(CThostFtdcInputForQuoteField *pInputForQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///����¼��������Ӧ
  virtual void OnRspQuoteInsert(CThostFtdcInputQuoteField *pInputQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///���۲���������Ӧ
  virtual void OnRspQuoteAction(CThostFtdcInputQuoteActionField *pInputQuoteAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///������������������Ӧ
  virtual void OnRspBatchOrderAction(CThostFtdcInputBatchOrderActionField *pInputBatchOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�������¼��������Ӧ
  virtual void OnRspCombActionInsert(CThostFtdcInputCombActionField *pInputCombAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ������Ӧ
  virtual void OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///�����ѯ�ɽ���Ӧ
  virtual void OnRspQryTrade(CThostFtdcTradeField *pTrade, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯͶ���ֲ߳���Ӧ
  virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ�ʽ��˻���Ӧ
  virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯͶ������Ӧ
  virtual void OnRspQryInvestor(CThostFtdcInvestorField *pInvestor, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ���ױ�����Ӧ
  virtual void OnRspQryTradingCode(CThostFtdcTradingCodeField *pTradingCode, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��Լ��֤������Ӧ
  virtual void OnRspQryInstrumentMarginRate(CThostFtdcInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��Լ����������Ӧ
  virtual void OnRspQryInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��������Ӧ
  virtual void OnRspQryExchange(CThostFtdcExchangeField *pExchange, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��Ʒ��Ӧ
  virtual void OnRspQryProduct(CThostFtdcProductField *pProduct, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��Լ��Ӧ
  virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ������Ӧ
  virtual void OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯͶ���߽�������Ӧ
  virtual void OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯת��������Ӧ
  virtual void OnRspQryTransferBank(CThostFtdcTransferBankField *pTransferBank, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯͶ���ֲ߳���ϸ��Ӧ
  virtual void OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ�ͻ�֪ͨ��Ӧ
  virtual void OnRspQryNotice(CThostFtdcNoticeField *pNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ������Ϣȷ����Ӧ
  virtual void OnRspQrySettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯͶ���ֲ߳���ϸ��Ӧ
  virtual void OnRspQryInvestorPositionCombineDetail(CThostFtdcInvestorPositionCombineDetailField *pInvestorPositionCombineDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///��ѯ��֤����ϵͳ���͹�˾�ʽ��˻���Կ��Ӧ
  virtual void OnRspQryCFMMCTradingAccountKey(CThostFtdcCFMMCTradingAccountKeyField *pCFMMCTradingAccountKey, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ�ֵ��۵���Ϣ��Ӧ
  virtual void OnRspQryEWarrantOffset(CThostFtdcEWarrantOffsetField *pEWarrantOffset, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯͶ����Ʒ��/��Ʒ�ֱ�֤����Ӧ
  virtual void OnRspQryInvestorProductGroupMargin(CThostFtdcInvestorProductGroupMarginField *pInvestorProductGroupMargin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��������֤������Ӧ
  virtual void OnRspQryExchangeMarginRate(CThostFtdcExchangeMarginRateField *pExchangeMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ������������֤������Ӧ
  virtual void OnRspQryExchangeMarginRateAdjust(CThostFtdcExchangeMarginRateAdjustField *pExchangeMarginRateAdjust, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ������Ӧ
  virtual void OnRspQryExchangeRate(CThostFtdcExchangeRateField *pExchangeRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ�����������Ա����Ȩ����Ӧ
  virtual void OnRspQrySecAgentACIDMap(CThostFtdcSecAgentACIDMapField *pSecAgentACIDMap, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��Ʒ���ۻ���
  virtual void OnRspQryProductExchRate(CThostFtdcProductExchRateField *pProductExchRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��Ʒ��
  virtual void OnRspQryProductGroup(CThostFtdcProductGroupField *pProductGroup, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��Ȩ���׳ɱ���Ӧ
  virtual void OnRspQryOptionInstrTradeCost(CThostFtdcOptionInstrTradeCostField *pOptionInstrTradeCost, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��Ȩ��Լ��������Ӧ
  virtual void OnRspQryOptionInstrCommRate(CThostFtdcOptionInstrCommRateField *pOptionInstrCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯִ��������Ӧ
  virtual void OnRspQryExecOrder(CThostFtdcExecOrderField *pExecOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯѯ����Ӧ
  virtual void OnRspQryForQuote(CThostFtdcForQuoteField *pForQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ������Ӧ
  virtual void OnRspQryQuote(CThostFtdcQuoteField *pQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��Ϻ�Լ��ȫϵ����Ӧ
  virtual void OnRspQryCombInstrumentGuard(CThostFtdcCombInstrumentGuardField *pCombInstrumentGuard, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ���������Ӧ
  virtual void OnRspQryCombAction(CThostFtdcCombActionField *pCombAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯת����ˮ��Ӧ
  virtual void OnRspQryTransferSerial(CThostFtdcTransferSerialField *pTransferSerial, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ����ǩԼ��ϵ��Ӧ
  virtual void OnRspQryAccountregister(CThostFtdcAccountregisterField *pAccountregister, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///����Ӧ��
  virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///����֪ͨ
  virtual void OnRtnOrder(CThostFtdcOrderField *pOrder);

  ///�ɽ�֪ͨ
  virtual void OnRtnTrade(CThostFtdcTradeField *pTrade);

  ///����¼�����ر�
  virtual void OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo);

  ///������������ر�
  virtual void OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo);

  ///��Լ����״̬֪ͨ
  virtual void OnRtnInstrumentStatus(CThostFtdcInstrumentStatusField *pInstrumentStatus) {};

  ///����֪ͨ
  virtual void OnRtnTradingNotice(CThostFtdcTradingNoticeInfoField *pTradingNoticeInfo) {};

  ///��ʾ������У�����
  virtual void OnRtnErrorConditionalOrder(CThostFtdcErrorConditionalOrderField *pErrorConditionalOrder) {};

  ///ִ������֪ͨ
  virtual void OnRtnExecOrder(CThostFtdcExecOrderField *pExecOrder) {};

  ///ִ������¼�����ر�
  virtual void OnErrRtnExecOrderInsert(CThostFtdcInputExecOrderField *pInputExecOrder, CThostFtdcRspInfoField *pRspInfo) {};

  ///ִ�������������ر�
  virtual void OnErrRtnExecOrderAction(CThostFtdcExecOrderActionField *pExecOrderAction, CThostFtdcRspInfoField *pRspInfo) {};

  ///ѯ��¼�����ر�
  virtual void OnErrRtnForQuoteInsert(CThostFtdcInputForQuoteField *pInputForQuote, CThostFtdcRspInfoField *pRspInfo) {};

  ///����֪ͨ
  virtual void OnRtnQuote(CThostFtdcQuoteField *pQuote) {};

  ///����¼�����ر�
  virtual void OnErrRtnQuoteInsert(CThostFtdcInputQuoteField *pInputQuote, CThostFtdcRspInfoField *pRspInfo) {};

  ///���۲�������ر�
  virtual void OnErrRtnQuoteAction(CThostFtdcQuoteActionField *pQuoteAction, CThostFtdcRspInfoField *pRspInfo) {};

  ///ѯ��֪ͨ
  virtual void OnRtnForQuoteRsp(CThostFtdcForQuoteRspField *pForQuoteRsp) {};

  ///��֤���������û�����
  virtual void OnRtnCFMMCTradingAccountToken(CThostFtdcCFMMCTradingAccountTokenField *pCFMMCTradingAccountToken) {};

  ///����������������ر�
  virtual void OnErrRtnBatchOrderAction(CThostFtdcBatchOrderActionField *pBatchOrderAction, CThostFtdcRspInfoField *pRspInfo) {};

  ///�������֪ͨ
  virtual void OnRtnCombAction(CThostFtdcCombActionField *pCombAction) {};

  ///�������¼�����ر�
  virtual void OnErrRtnCombActionInsert(CThostFtdcInputCombActionField *pInputCombAction, CThostFtdcRspInfoField *pRspInfo) {};

  ///�����ѯǩԼ������Ӧ
  virtual void OnRspQryContractBank(CThostFtdcContractBankField *pContractBank, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯԤ����Ӧ
  virtual void OnRspQryParkedOrder(CThostFtdcParkedOrderField *pParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯԤ�񳷵���Ӧ
  virtual void OnRspQryParkedOrderAction(CThostFtdcParkedOrderActionField *pParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ����֪ͨ��Ӧ
  virtual void OnRspQryTradingNotice(CThostFtdcTradingNoticeField *pTradingNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ���͹�˾���ײ�����Ӧ
  virtual void OnRspQryBrokerTradingParams(CThostFtdcBrokerTradingParamsField *pBrokerTradingParams, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ���͹�˾�����㷨��Ӧ
  virtual void OnRspQryBrokerTradingAlgos(CThostFtdcBrokerTradingAlgosField *pBrokerTradingAlgos, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�����ѯ��������û�����
  virtual void OnRspQueryCFMMCTradingAccountToken(CThostFtdcQueryCFMMCTradingAccountTokenField *pQueryCFMMCTradingAccountToken, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///���з��������ʽ�ת�ڻ�֪ͨ
  virtual void OnRtnFromBankToFutureByBank(CThostFtdcRspTransferField *pRspTransfer) {};

  ///���з����ڻ��ʽ�ת����֪ͨ
  virtual void OnRtnFromFutureToBankByBank(CThostFtdcRspTransferField *pRspTransfer) {};

  ///���з����������ת�ڻ�֪ͨ
  virtual void OnRtnRepealFromBankToFutureByBank(CThostFtdcRspRepealField *pRspRepeal) {};

  ///���з�������ڻ�ת����֪ͨ
  virtual void OnRtnRepealFromFutureToBankByBank(CThostFtdcRspRepealField *pRspRepeal) {};

  ///�ڻ����������ʽ�ת�ڻ�֪ͨ
  virtual void OnRtnFromBankToFutureByFuture(CThostFtdcRspTransferField *pRspTransfer) {};

  ///�ڻ������ڻ��ʽ�ת����֪ͨ
  virtual void OnRtnFromFutureToBankByFuture(CThostFtdcRspTransferField *pRspTransfer) {};

  ///ϵͳ����ʱ�ڻ����ֹ������������ת�ڻ��������д�����Ϻ��̷��ص�֪ͨ
  virtual void OnRtnRepealFromBankToFutureByFutureManual(CThostFtdcRspRepealField *pRspRepeal) {};

  ///ϵͳ����ʱ�ڻ����ֹ���������ڻ�ת�����������д�����Ϻ��̷��ص�֪ͨ
  virtual void OnRtnRepealFromFutureToBankByFutureManual(CThostFtdcRspRepealField *pRspRepeal) {};

  ///�ڻ������ѯ�������֪ͨ
  virtual void OnRtnQueryBankBalanceByFuture(CThostFtdcNotifyQueryAccountField *pNotifyQueryAccount) {};

  ///�ڻ����������ʽ�ת�ڻ�����ر�
  virtual void OnErrRtnBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo) {};

  ///�ڻ������ڻ��ʽ�ת���д���ر�
  virtual void OnErrRtnFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo) {};

  ///ϵͳ����ʱ�ڻ����ֹ������������ת�ڻ�����ر�
  virtual void OnErrRtnRepealBankToFutureByFutureManual(CThostFtdcReqRepealField *pReqRepeal, CThostFtdcRspInfoField *pRspInfo) {};

  ///ϵͳ����ʱ�ڻ����ֹ���������ڻ�ת���д���ر�
  virtual void OnErrRtnRepealFutureToBankByFutureManual(CThostFtdcReqRepealField *pReqRepeal, CThostFtdcRspInfoField *pRspInfo) {};

  ///�ڻ������ѯ����������ر�
  virtual void OnErrRtnQueryBankBalanceByFuture(CThostFtdcReqQueryAccountField *pReqQueryAccount, CThostFtdcRspInfoField *pRspInfo) {};

  ///�ڻ������������ת�ڻ��������д�����Ϻ��̷��ص�֪ͨ
  virtual void OnRtnRepealFromBankToFutureByFuture(CThostFtdcRspRepealField *pRspRepeal) {};

  ///�ڻ���������ڻ�ת�����������д�����Ϻ��̷��ص�֪ͨ
  virtual void OnRtnRepealFromFutureToBankByFuture(CThostFtdcRspRepealField *pRspRepeal) {};

  ///�ڻ����������ʽ�ת�ڻ�Ӧ��
  virtual void OnRspFromBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�ڻ������ڻ��ʽ�ת����Ӧ��
  virtual void OnRspFromFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�ڻ������ѯ�������Ӧ��
  virtual void OnRspQueryBankAccountMoneyByFuture(CThostFtdcReqQueryAccountField *pReqQueryAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///���з������ڿ���֪ͨ
  virtual void OnRtnOpenAccountByBank(CThostFtdcOpenAccountField *pOpenAccount) {};

  ///���з�����������֪ͨ
  virtual void OnRtnCancelAccountByBank(CThostFtdcCancelAccountField *pCancelAccount) {};

  ///���з����������˺�֪ͨ
  virtual void OnRtnChangeAccountByBank(CThostFtdcChangeAccountField *pChangeAccount) {};

  /// ���յ���Լ��λ��ѯӦ��ʱ�ص��ú���
  virtual void onRspMBLQuot(CThostMBLQuotData *pMBLQuotData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

  STUTdParam                              m_stuTdParam;
private:
  //CThostFtdcTraderApi											*m_pTdReqApi;
  //CSgitContext														*m_pSgitCtx;
  //std::string															m_ssUserId;
  //std::string															m_ssPassword;

  AtomicCounter														m_acRequestId;

	AtomicCounter														m_acOrderRef;
  //���ǵ����������Ҫ��ʱ�䲻�������У���Ҫʹ�ó�ʱ���棬���򣬿���map���
	//OrderRef -> ClOrderID (��������->fix���ر������)
	std::map<std::string, std::string>			m_mapOrderRef2ClOrdID;

	//ClOrderID -> OrderRef (fix���ر������->��������)
	std::map<std::string, std::string>			m_mapClOrdID2OrderRef;

  //OrderRef -> STUOrder (��������->ί��)
  std::map<std::string, STUOrder>					m_mapOrderRef2Order;

  //�˻�����->��ʵ�˻�
  std::map<std::string, std::string>      m_mapAcctAlias2Real;

	std::fstream														m_fOrderRef2ClOrdID;
	Poco::FastMutex													m_fastMutexOrderRef2ClOrdID;
};

//������hubת��
class CSgitTdSpiHubTran : public CSgitTdSpi
{
public:
  CSgitTdSpiHubTran(const STUTdParam &stuTdParam);
  virtual ~CSgitTdSpiHubTran();

  bool LoadConfig(AutoPtr<IniFileConfiguration> apSgitConf, const std::string &ssSessionProp);

	Convert::EnCvtType GetSymbolType(const std::string &ssRealAcct);

	void SetSymbolType(const std::string &ssRealAcct, Convert::EnCvtType enSymbolType);
private:
  //��ʵ�ʽ��˺�->�������
  std::map<std::string, Convert::EnCvtType>   m_mapRealAcct2SymbolType;

  //��ʵ�ʽ��˺�->SessionKey
  std::map<std::string, std::string>          m_mapRealAcct2SessionKey;
};


//����ֱ��
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
