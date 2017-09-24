#ifndef __SGITAPIMANAGER_H__
#define __SGITAPIMANAGER_H__


#include "SgitTradeSpi.h"
#include "Convert.h"
#include "quickfix/Message.h"
#include "Poco/Util/JSONConfiguration.h"
#include "Poco/Util/XMLConfiguration.h"

using namespace Poco::Util;

class CSgitContext
{
  struct STUFixInfo{
    //收到的原始资金账号
    std::string     m_ssAcctRecv;
    FIX::Header     m_oHeader;

    FIX::SessionID  m_oSessionID;
  };

public:
  CSgitContext(const std::string &ssSgitCfgPath, const std::string &ssCvtCfgPath);
  ~CSgitContext();

  bool Init();

  SharedPtr<CSgitTradeSpi> GetSpi(const FIX::Message& oMsg);

  std::string GetRealAccont(const FIX::Message& oRecvMsg);

  char CvtDict(const int iField, const char cValue, const Convert::EnDictType enDstDictType);

  std::string CvtSymbol(const std::string &ssSymbol, const Convert::EnCvtType enDstType);

	std::string CvtExchange(const std::string &ssExchange, const Convert::EnCvtType enSrcType, const Convert::EnCvtType enDstType);

  void Send(const std::string &ssAcct, FIX::Message &oMsg);

  void AddFixInfo(const FIX::Message& oMsg, const FIX::SessionID& sessionID);

protected:
  bool InitConvert();

  bool InitSgitApi();

  SharedPtr<CSgitTradeSpi> CreateSpi(const std::string &ssFlowPath, const std::string &ssTradeServerAddr, const std::string &ssTradeId);

  void LinkAcct2Spi(SharedPtr<CSgitTradeSpi> spTradeSpi, const std::string &ssTradeId);

  SharedPtr<CSgitTradeSpi> GetSpi(const std::string &ssKey);

  bool GetFixInfo(const std::string &ssAcct, STUFixInfo &stuFixInfo);

  void SetFixInfo(const STUFixInfo &stuFixInfo, FIX::Message &oMsg);
private:
  std::string                           m_ssSgitCfgPath;
  Convert                               m_oConvert;
  std::string                           m_ssCvtCfgPath;

	AutoPtr<IniFileConfiguration>					m_apSgitConf;

  //账户别名（TargetCompID + OnBehalfOfCompID）对实际账户
  std::map<std::string, std::string>    m_mapAlias2Acct;

  //实际账户(账户别名)->Spi实例
  std::map<std::string, SharedPtr<CSgitTradeSpi>>   m_mapAcct2Spi;

  //资金账号真名->Fix相关信息(用于应答和推送)
  std::map<std::string, STUFixInfo>     m_mapAcct2FixInfo;
};
#endif // __SGITAPIMANAGER_H__
