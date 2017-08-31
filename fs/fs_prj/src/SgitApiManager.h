#ifndef __SGITAPIMANAGER_H__
#define __SGITAPIMANAGER_H__


#include "SgitTradeSpi.h"

class CSgitApiManager
{
public:
  CSgitApiManager(const std::string &ssSgitCfgPath);
  ~CSgitApiManager();

  void Init();

  CThostFtdcTraderApi* CreateApiSpi(const std::string &ssFlowPath, const std::string &ssTradeServer, const std::string ssTradeId);

  void LinkAcct2Api(CThostFtdcTraderApi* pTradeApi, const std::string ssTradeId);

  CThostFtdcTraderApi* GetApi(const FIX::Message& oMsg);
private:
  std::string                           m_ssSgitCfgPath;

  //�˻�������TargetCompID + OnBehalfOfCompID����ʵ���˻�
  std::map<std::string, std::string>    m_mapAlias2Acct;

  //�˻�����->Apiʵ��
  std::map<std::string,  CThostFtdcTraderApi*>  m_mapAlias2Api;

  //ʵ���˻�->Apiʵ��
  std::map<std::string, CThostFtdcTraderApi*>   m_mapAcct2Api;
};
#endif // __SGITAPIMANAGER_H__
