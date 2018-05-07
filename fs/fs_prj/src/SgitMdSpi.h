#ifndef __SGITMDSPI_H__
#define __SGITMDSPI_H__

#include "sgit/SgitFtdcMdApi.h"
#include "quickfix/fix42/MarketDataRequest.h"
#include "quickfix/fix42/MarketDataSnapshotFullRefresh.h"
#include "Convert.h"
#include <float.h>
#include "Poco/RWLock.h"
#include "Const.h"
#include <math.h>

using namespace fstech;
using namespace  std;

std::string const ALL_SYMBOL = "all";
double const THRESHOLD = 2*DBL_MIN;

inline bool feq( double const x, double const y ) { return fabs( x - y ) < THRESHOLD; }

class CSgitContext;
class CSgitMdSpi : public CThostFtdcMdSpi
{
public:
  CSgitMdSpi(CSgitContext *pSgitCtx, CThostFtdcMdApi *pMdReqApi, const std::string &ssTradeId, const std::string &ssPassword);
  virtual ~CSgitMdSpi();

  bool OnMessage(const FIX::Message& oMsg, const FIX::SessionID& oSessionID, std::string& ssErrMsg);

  bool MarketDataRequest(const FIX42::MarketDataRequest& oMarketDataRequest, std::string& ssErrMsg);

protected:

  bool GetMarketData(const std::string ssSymbol, CThostFtdcDepthMarketDataField &stuMarketData);

  bool CheckValid(
    const std::set<std::string> &symbolSetIn, std::set<std::string> &symbolSetOrg,
    const std::string &ssMDReqID, char chScrbReqType, const STUScrbParm &stuScrbParm, char &chRejReason, std::string &ssErrMsg);

  //���Ϳ���
  bool SendMarketDataSet(const FIX42::MarketDataRequest& oMarketDataRequest, const std::set<std::string> &symbolSet, const STUScrbParm &stuScrbParm, std::string &ssErrMsg);

  //��������
  void PubMarketData(const CThostFtdcDepthMarketDataField &stuDepthMarketData);

  FIX42::MarketDataSnapshotFullRefresh CreateSnapShot(
    const CThostFtdcDepthMarketDataField &stuMarketData, 
    const STUScrbParm &stuScrbParm, 
    const std::string &ssMDReqID = "");

  void AddPrice(FIX42::MarketDataSnapshotFullRefresh &oMdSnapShot, char chEntryType, double dPrice, int iVolume = 0, int iPos = 0);

  //�������Ĺ�ϵ
  void AddSub(const std::set<std::string> &symbolSet, const STUScrbParm &stuScrbParm);

  //ȡ�����Ĺ�ϵ
  void DelSub(const std::set<std::string> &symbolSet, const STUScrbParm &stuScrbParm);

  void Send(const std::string &ssSessionKey, FIX42::MarketDataSnapshotFullRefresh oMdSnapShot);

  void AddFixInfo(const FIX::Message& oMsg);

  ///���ͻ����뽻�׺�̨������ͨ������ʱ����δ��¼ǰ�����÷��������á�
  virtual void OnFrontConnected();

  ///���ͻ����뽻�׺�̨ͨ�����ӶϿ�ʱ���÷��������á���������������API���Զ��������ӣ��ͻ��˿ɲ�������
  ///@param nReason ����ԭ��
  ///        0x1001 �����ʧ��
  ///        0x1002 ����дʧ��
  ///        0x2001 ����������ʱ
  ///        0x2002 ��������ʧ��
  ///        0x2003 �յ�������
  virtual void OnFrontDisconnected(int nReason);

  ///������ʱ���档����ʱ��δ�յ�����ʱ���÷��������á�
  ///@param nTimeLapse �����ϴν��ձ��ĵ�ʱ��
  virtual void OnHeartBeatWarning(int nTimeLapse);


  ///��¼������Ӧ
  virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///�ǳ�������Ӧ
  virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///����Ӧ��
  virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///��������Ӧ��
  virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///ȡ����������Ӧ��
  virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///����ѯ��Ӧ��
  virtual void OnRspSubForQuoteRsp(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///ȡ������ѯ��Ӧ��
  virtual void OnRspUnSubForQuoteRsp(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///�������֪ͨ
  virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData);

  ///ѯ��֪ͨ
  virtual void OnRtnForQuoteRsp(CThostFtdcForQuoteRspField *pForQuoteRsp) {};

  ///���ӽ�������
  virtual void OnRtnDeferDeliveryQuot(CThostDeferDeliveryQuot* pQuot){};

private:
  CSgitContext														              *m_pSgitCtx;
  CThostFtdcMdApi													              *m_pMdReqApi;
	AtomicCounter														              m_acRequestId;
  CThostFtdcReqUserLoginField                           m_stuLogin;

  //����MDReqID (262)��¼�������ж��Ƿ����ظ�
  std::map<std::string, std::set<std::string> >         m_mapMDReqID;
  FastMutex													                    m_fastmutexLockMDReqID;

  //���Ĺ�ϵ ����->����session
  std::map<std::string, std::set<STUScrbParm> >         m_mapCode2ScrbParmSet; 
  RWLock                                                m_rwLockCode2ScrbParmSet;

  ////����ȫ�������session
  //std::set<std::string>                                 m_setSubAllCodeSession;

  //����ȫ�г��������
  std::map<std::string, CThostFtdcDepthMarketDataField> m_mapSnapshot;
  //ȫ�г�������ն�д��
  RWLock                                                m_rwLockSnapShot;
};

#endif // __SGITMDSPI_H__
