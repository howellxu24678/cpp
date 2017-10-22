#ifndef __SGITMDSPI_H__
#define __SGITMDSPI_H__

#include "sgit/SgitFtdcMdApi.h"
#include "quickfix/fix42/MarketDataRequest.h"
#include "Convert.h"
#include <float.h>
#include "Poco/RWLock.h"

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
  ~CSgitMdSpi();

  void MarketDataRequest(const FIX42::MarketDataRequest& oMarketDataRequest);

protected:
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
  virtual void OnHeartBeatWarning(int nTimeLapse);


  ///登录请求响应
  virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///登出请求响应
  virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///错误应答
  virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///订阅行情应答
  virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///取消订阅行情应答
  virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

  ///订阅询价应答
  virtual void OnRspSubForQuoteRsp(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///取消订阅询价应答
  virtual void OnRspUnSubForQuoteRsp(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

  ///深度行情通知
  virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData);

  ///询价通知
  virtual void OnRtnForQuoteRsp(CThostFtdcForQuoteRspField *pForQuoteRsp) {};

  ///递延交割行情
  virtual void OnRtnDeferDeliveryQuot(CThostDeferDeliveryQuot* pQuot){};

	bool CheckValid(const std::set<std::string> &symbolSet, const std::string &ssMDReqID, const std::string &ssSessionID);

  //发送快照
  void SendSnapShot(const std::set<std::string> &symbolSet, const std::string &ssMDReqID, const std::string &ssSessionID);

  //建立订阅关系
  void AddSub(const std::set<std::string> &symbolSet, const std::string &ssSessionID);

private:
  CThostFtdcMdApi													              *m_pMdReqApi;
	CSgitContext														              *m_pSgitCtx;
	AtomicCounter														              m_acRequestId;
  CThostFtdcReqUserLoginField                           m_stuLogin;
  //订阅关系 代码->订阅session
  std::map<std::string, std::set<std::string>>          m_mapCode2SubSession; 

  //订阅全量代码的session
  std::set<std::string>                                 m_lSubAllCodeSession;

  //保存全市场行情快照
  std::map<std::string, CThostFtdcDepthMarketDataField> m_mapSnapshot;
  //全市场行情快照读写锁
  RWLock                                                m_rwLockSnapShot;
};

#endif // __SGITMDSPI_H__
