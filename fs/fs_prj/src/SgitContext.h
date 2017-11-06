#ifndef __SGITAPIMANAGER_H__
#define __SGITAPIMANAGER_H__


#include "SgitTdSpi.h"
#include "SgitMdSpi.h"
#include "Convert.h"
#include "quickfix/Message.h"
#include "Poco/Util/JSONConfiguration.h"
#include "Poco/Util/XMLConfiguration.h"

using namespace Poco::Util;

class CSgitContext
{
public:
  CSgitContext(const std::string &ssSgitCfgPath, const std::string &ssCvtCfgPath);
  ~CSgitContext();

  bool Init();

  void Deal(const FIX::Message& oMsg, const FIX::SessionID& oSessionID);

  SharedPtr<CSgitTdSpi> GetTdSpi(const FIX::SessionID& oSessionID);

  SharedPtr<CSgitMdSpi> GetMdSpi(const FIX::SessionID& oSessionID);

  char CvtDict(const int iField, const char cValue, const Convert::EnDictType enDstDictType);

  std::string CvtSymbol(const std::string &ssSymbol, const Convert::EnCvtType enDstType);

	std::string CvtExchange(const std::string &ssExchange, const Convert::EnCvtType enDstType);

  void Send(const std::string &ssAcct, FIX::Message &oMsg);

  void SetSymbolType(const std::string &ssSessionKey, Convert::EnCvtType enSymbolType);

  Convert::EnCvtType GetSymbolType(const std::string &ssSessionKey);

protected:
  bool InitConvert();

  bool InitSgitApi();

  SharedPtr<CSgitTdSpi> CreateTdSpi(const std::string &ssFlowPath, const std::string &ssTradeServerAddr, CSgitTdSpi::STUTdParam &stuTdParam, CSgitTdSpi::EnTdSpiRole enTdSpiRole);

  void CreateMdSpi(const std::string &ssFlowPath, const std::string &ssMdServerAddr, const std::string &ssTradeId, const std::string &ssPassword);

  bool LinkSessionID2TdSpi(const std::string &ssSessionID, SharedPtr<CSgitTdSpi> spTdSpi);

  //bool GetFixInfo(const std::string &ssAcct, STUFixInfo &stuFixInfo);

  //void SetFixInfo(const STUFixInfo &stuFixInfo, FIX::Message &oMsg);

  //void AddFixInfo(const std::string &ssKey, const STUFixInfo &stuFixInfo);

  ////预先发起登录
  //bool PreLogin();
private:
  std::string                                         m_ssSgitCfgPath;
  Convert                                             m_oConvert;
  std::string                                         m_ssCvtCfgPath;

	AutoPtr<IniFileConfiguration>					              m_apSgitConf;

  SharedPtr<CSgitMdSpi>                               m_spMdSpi;

  ////账户别名(SessionID+onBehalfOfCompID)->真实资金账户
  //std::map<std::string, std::string>              m_mapAlias2Acct;

  ////真实资金账户->SessionID + onBehalfOfCompID + 原始送入账户 + 所用代码类型 --用于交易推送
  //std::map<std::string, Poco::SharedPtr<STUserInfo>>  m_mapAcct2FixInfo;
  //RWLock                                              m_rwAcct2FixInfo;

  //fix用户(SessionID+onBehalfOfCompID)->代码类型 --用于交易行情推送
  std::map<std::string, Convert::EnCvtType>           m_mapFixUser2SymbolType;
  RWLock                                              m_rwFixUser2SymbolType;

  ////fix用户(SessionID+onBehalfOfCompID)->所用代码类型等信息 --用于交易行情推送
  ////std::map<std::string, SharedPtr<STUserInfo>>        m_mapFixUser2Info;
  ////RWLock                                              m_rwFixUser2Info;

  //SessionID->TdSpi实例
  std::map<std::string, SharedPtr<CSgitTdSpi>>        m_mapSessionID2TdSpi;
  RWLock                                              m_rwSessionID2TdSpi;
};
#endif // __SGITAPIMANAGER_H__
