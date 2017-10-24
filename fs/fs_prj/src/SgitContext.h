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

  void Deal(const FIX::Message& oMsg);

  SharedPtr<CSgitTdSpi> GetTdSpi(const FIX::Message& oMsg);

  SharedPtr<CSgitMdSpi> GetMdSpi(const FIX::Message& oMsg);

  std::string GetRealAccont(const FIX::Message& oRecvMsg);

  char CvtDict(const int iField, const char cValue, const Convert::EnDictType enDstDictType);

  std::string CvtSymbol(const std::string &ssSymbol, const Convert::EnCvtType enDstType);

	std::string CvtExchange(const std::string &ssExchange, const Convert::EnCvtType enDstType);

  void Send(const std::string &ssAcct, FIX::Message &oMsg);

  void AddFixInfo(const FIX::Message& oMsg);

protected:
  bool InitConvert();

  bool InitSgitApi();

  SharedPtr<CSgitTdSpi> CreateTdSpi(const std::string &ssFlowPath, const std::string &ssTradeServerAddr, const std::string &ssUserId, const std::string &ssPassword);

  void CreateMdSpi(const std::string &ssFlowPath, const std::string &ssMdServerAddr, const std::string &ssTradeId, const std::string &ssPassword);

  void LinkSessionID2TdSpi(const std::string &ssSessionID, SharedPtr<CSgitTdSpi> spTdSpi);

  SharedPtr<CSgitTdSpi> GetTdSpi(const std::string &ssKey);

	//SharedPtr<CSgitMdSpi> GetMdSpi(const std::string &ssKey);

  bool GetFixInfo(const std::string &ssAcct, STUFixInfo &stuFixInfo);

  void SetFixInfo(const STUFixInfo &stuFixInfo, FIX::Message &oMsg);

  void AddFixInfo(const std::string &ssKey, const STUFixInfo &stuFixInfo);

  //预先发起登录
  bool PreLogin();
private:
  std::string                           m_ssSgitCfgPath;
  Convert                               m_oConvert;
  std::string                           m_ssCvtCfgPath;

	AutoPtr<IniFileConfiguration>					m_apSgitConf;

  //账户别名（TargetCompID + OnBehalfOfCompID）对实际账户
  std::map<std::string, std::string>    m_mapAlias2Acct;

  //实际账户(账户别名)->TdSpi实例
  std::map<std::string, SharedPtr<CSgitTdSpi>>   m_mapSessionID2TdSpi;

  SharedPtr<CSgitMdSpi>                          m_spMdSpi;
  ////实际账户(账户别名)->MdSpi实例
  //std::map<std::string, SharedPtr<CSgitMdSpi>>   m_mapAcct2MdSpi;

  //资金账号真名->Fix相关信息(用于应答和推送)
  std::map<std::string, STUFixInfo>     m_mapSessionAcct2FixInfo;
};
#endif // __SGITAPIMANAGER_H__
