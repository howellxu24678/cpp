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
  struct STUFixInfo{
    //�յ���ԭʼ�ʽ��˺�
    std::string     m_ssAcctRecv;
    FIX::Header     m_oHeader;

    FIX::SessionID  m_oSessionID;
  };

public:
  CSgitContext(const std::string &ssSgitCfgPath, const std::string &ssCvtCfgPath);
  ~CSgitContext();

  bool Init();

  SharedPtr<CSgitTdSpi> GetTdSpi(const FIX::Message& oMsg);

  SharedPtr<CSgitMdSpi> GetMdSpi(const FIX::Message& oMsg);

  std::string GetRealAccont(const FIX::Message& oRecvMsg);

  char CvtDict(const int iField, const char cValue, const Convert::EnDictType enDstDictType);

  std::string CvtSymbol(const std::string &ssSymbol, const Convert::EnCvtType enDstType);

	std::string CvtExchange(const std::string &ssExchange, const Convert::EnCvtType enDstType);

  void Send(const std::string &ssAcct, FIX::Message &oMsg);

  void AddFixInfo(const FIX::Message& oMsg, const FIX::SessionID& sessionID);

protected:
  bool InitConvert();

  bool InitSgitApi();

  SharedPtr<CSgitTdSpi> CreateTdSpi(const std::string &ssFlowPath, const std::string &ssTradeServerAddr, const std::string &ssTradeId);

  SharedPtr<CSgitMdSpi> CreateMdSpi(const std::string &ssFlowPath, const std::string &ssMdServerAddr, const std::string &ssTradeId);

  void LinkAcct2TdSpi(SharedPtr<CSgitTdSpi> spTdSpi, SharedPtr<CSgitMdSpi> spMdSpi, const std::string &ssTradeId);

  SharedPtr<CSgitTdSpi> GetTdSpi(const std::string &ssKey);

	SharedPtr<CSgitMdSpi> GetMdSpi(const std::string &ssKey);

  bool GetFixInfo(const std::string &ssAcct, STUFixInfo &stuFixInfo);

  void SetFixInfo(const STUFixInfo &stuFixInfo, FIX::Message &oMsg);
private:
  std::string                           m_ssSgitCfgPath;
  Convert                               m_oConvert;
  std::string                           m_ssCvtCfgPath;

	AutoPtr<IniFileConfiguration>					m_apSgitConf;

  //�˻�������TargetCompID + OnBehalfOfCompID����ʵ���˻�
  std::map<std::string, std::string>    m_mapAlias2Acct;

  //ʵ���˻�(�˻�����)->TdSpiʵ��
  std::map<std::string, SharedPtr<CSgitTdSpi>>   m_mapAcct2TdSpi;

  //ʵ���˻�(�˻�����)->MdSpiʵ��
  std::map<std::string, SharedPtr<CSgitMdSpi>>   m_mapAcct2MdSpi;

  //�ʽ��˺�����->Fix�����Ϣ(����Ӧ�������)
  std::map<std::string, STUFixInfo>     m_mapAcct2FixInfo;
};
#endif // __SGITAPIMANAGER_H__
