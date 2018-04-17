#ifndef __SGITAPIMANAGER_H__
#define __SGITAPIMANAGER_H__


#include "SgitTdSpi.h"
#include "SgitMdSpi.h"
#include "Convert.h"
#include "quickfix/Message.h"
#include "Poco/Util/JSONConfiguration.h"
#include "Poco/Util/XMLConfiguration.h"
#include "Poco/Data/Session.h"
#include "Poco/Data/Statement.h"
#include "Poco/Data/SQLite/Connector.h"

using namespace Poco::Util;
using namespace Poco::Data;
using namespace Poco::Data::Keywords;

class CSgitContext
{
public:
  CSgitContext(const std::string &ssSgitCfgPath, const std::string &ssCvtCfgPath);
  ~CSgitContext();

  bool Init();

  bool Deal(const FIX::Message& oMsg, const FIX::SessionID& oSessionID, std::string& ssErrMsg);

  SharedPtr<CSgitTdSpi> GetOrCreateTdSpi(const FIX::SessionID& oSessionID, EnTdSpiRole enTdSpiRole);

  SharedPtr<CSgitTdSpi> GetTdSpi(const FIX::SessionID& oSessionID);

  SharedPtr<CSgitMdSpi> GetMdSpi(const FIX::SessionID& oSessionID);

  char CvtDict(const int iField, const char cFrom, const Convert::EnDictType enDstDictType);

  std::string CvtDict(const int iField, const std::string  &ssFrom, const Convert::EnDictType enDstDictType);

  std::string CvtSymbol(const std::string &ssSymbol, const Convert::EnCvtType enDstType);

	std::string CvtExchange(const std::string &ssExchange, const Convert::EnCvtType enDstType);

  void AddUserInfo(const std::string &ssSessionKey, SharedPtr<STUserInfo> spStuFixInfo);

  void UpdateSymbolType(const std::string &ssSessionKey, Convert::EnCvtType enSymbolType);

  Convert::EnCvtType GetSymbolType(const std::string &ssSessionKey);

  void UpsertLoginStatus(const std::string ssSessionID, bool bStatus);

  bool GetLoginStatus(const std::string ssSessionID);

  bool IsQuoteSupported();

  bool IsTradeSupported();

protected:
  bool InitSQLConnect();

  bool InitConvert();

  bool InitSgit();

  bool InitSgitQuote();

  bool InitSgitTrade();

  bool NeedRun(const std::string ssSectionName);

  SharedPtr<CSgitTdSpi> CreateTdSpi(STUTdParam &stuTdParam, EnTdSpiRole enTdSpiRole);

  SharedPtr<CSgitTdSpi> CreateTdSpi(const std::string &ssSessionID, EnTdSpiRole enTdSpiRole);

  void CreateMdSpi(const std::string &ssFlowPath, const std::string &ssMdServerAddr, const std::string &ssTradeId, const std::string &ssPassword);

  bool LinkSessionID2TdSpi(const std::string &ssSessionID, SharedPtr<CSgitTdSpi> spTdSpi);

private:
  std::string                                         m_ssSgitCfgPath;
  Convert                                             m_oConvert;
  std::string                                         m_ssCvtCfgPath;

	AutoPtr<IniFileConfiguration>					              m_apSgitConf;
  std::string                                         m_ssFlowPath;
  std::string                                         m_ssDataPath;
  std::string                                         m_ssTdServerAddr;

  std::string                                         m_ssTradingDay;

  SharedPtr<CSgitMdSpi>                               m_spMdSpi;

  //fix用户(SessionID+onBehalfOfCompID)->所用代码类型等信息 --用于交易行情推送
  std::map<std::string, SharedPtr<STUserInfo> >        m_mapFixUser2Info;
  RWLock                                              m_rwFixUser2Info;

  //fix会话->登录状态 （登录时才进行行情推送）
  std::map<std::string, bool>                         m_mapFisSessionID2LoginStatus;
  RWLock                                              m_rwFisSessionID2LoginStatus;

  //SessionID->TdSpi实例
  std::map<std::string, SharedPtr<CSgitTdSpi> >        m_mapSessionID2TdSpi;
  RWLock                                              m_rwSessionID2TdSpi;

  bool                                                m_bQuoteSupported;
  bool                                                m_bTradeSupported;

  SharedPtr<Session>                                  m_spSQLiteSession;
  //Session                                             m_oSQLiteSession;
};
#endif // __SGITAPIMANAGER_H__
