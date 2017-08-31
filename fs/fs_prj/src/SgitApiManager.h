#ifndef __SGITAPIMANAGER_H__
#define __SGITAPIMANAGER_H__


#include "SgitTradeSpi.h"
#include "quickfix/Message.h"

class CSgitApiManager
{
public:
  CSgitApiManager(const std::string &ssSgitCfgPath);
  ~CSgitApiManager();

  void Init();

  SharedPtr<CSgitTradeSpi> CreateSpi(const std::string &ssFlowPath, const std::string &ssTradeServerAddr, const std::string ssTradeId);

  void LinkAcct2Spi(SharedPtr<CSgitTradeSpi> spTradeSpi, const std::string ssTradeId);

  SharedPtr<CSgitTradeSpi> GetApi(const FIX::Message& oMsg);
private:
  std::string                           m_ssSgitCfgPath;

	AutoPtr<IniFileConfiguration>					m_apSgitConf;

  //�˻�������TargetCompID + OnBehalfOfCompID����ʵ���˻�
  std::map<std::string, std::string>    m_mapAlias2Acct;

  //ʵ���˻�(�˻�����)->Spiʵ��
  std::map<std::string, SharedPtr<CSgitTradeSpi>>   m_mapAcct2Spi;
};
#endif // __SGITAPIMANAGER_H__
