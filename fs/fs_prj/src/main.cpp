#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#else
#include "config.h"
#endif

#include "quickfix/FileStore.h"
#include "quickfix/ThreadedSocketAcceptor.h"
#include "quickfix/ThreadedSocketInitiator.h"
#include "quickfix/FileLog.h"
#include "quickfix/SessionSettings.h"
#include "Application.h"
#include <string>
#include <iostream>
#include <fstream>

#include "Log.h"

#include "Poco/Foundation.h"
//#include "Poco/LocalDateTime.h"
//#include "Poco/DateTimeFormatter.h"
//#include "Poco/DateTimeFormat.h"
//#include "Poco/Timestamp.h"
//#include "Poco/Thread.h"
//#include "Poco/Timespan.h"

#include "Poco/Util/Util.h"
//#include "Poco/Util/Timer.h"
//#include "Poco/Util/TimerTask.h"
//#include "Poco/Util/TimerTaskAdapter.h"

#include "Poco/Util/IniFileConfiguration.h"

#include "SgitApiManager.h"
//#include "Poco/FileStream.h"

using namespace Poco;
using namespace Poco::Util;

/*
The following table provides examples regarding the use of SenderCompID (49) , TargetCompID (56) , DeliverToCompID (128) , and OnBehalfOfCompID (115) when using a single point-to-point FIX session between two firms. Assumption (A=sellside, B =buyside):

                        SenderCompID (49)	OnBehalfOfCompID (115)	TargetCompID (56)	DeliverToCompID (128)
A to B directly	            A		                                        B	
B to A directly	            B		                                        A

The following table provides examples regarding the use of SenderCompID (49) , TargetCompID (56) , DeliverToCompID (128) , and OnBehalfOfCompID (115) when using a single FIX session to represent multiple firms. Assumption (A=sellside, B and C=buyside, Q=third party):

                        SenderCompID (49)	OnBehalfOfCompID (115)	TargetCompID (56)	DeliverToCompID (128)	OnBehalfOfSendingTime (370)
Send from A to B via Q
1)	A sends to Q	          A		                                        Q	                B	
2)	Q sends to B	          Q	                    A	                    B		                                A's SendingTime
B responds to A via Q
1)	B sends to Q	          B		                                        Q	                A	
2)	Q sends to A	          Q	                    B	                    A		                                B's SendingTime
*/

void wait()
{
	std::cout << "Type Ctrl-C to quit" << std::endl;
	while(true)
	{
		FIX::process_sleep(1);
	}
}

int main( int argc, char** argv )
{
	if(argc < 4)
	{
		std::cout << "usage: argv is no enough" << std::endl;
		std::cout << "you need to indicate the cfg path of log, fix and sgit..." << std::endl << std::endl;
		for (int i = 0; i < argc; i++)
		{
			std::cout << "argv[" << i << "]:" << argv[i] << std::endl;
		}
		
		return 0;
	}

	std::string ssLogCfgPath = argv[1];
	std::string ssSgitCfgPath = argv[2];
  std::string ssFixCfgPath = argv[3];

  try
  {
		//��ʼ����־
		initialize ();
		ConfigureAndWatchThread configureThread(ssLogCfgPath, 5 * 1000);

    /*
    1. һ������Ա�˺Ŷ�Ӧһ��apiʵ������ʼ��ʱ��ȡ�����ļ��еĽ���Ա�˺ź�������Ϣ������Ӧ��apiʵ��
    2. fix application �� onMessage�յ���Ϣ�󣬽�����Ϣ������  XXX �ҵ���Ӧ�� apiʵ���ύ����
    3. apiʵ���ص���������ҵ���Ӧ��fix sessionID ���ͻ�ȥ
    */
		
    
    CSgitApiManager oSigtApiMngr = CSgitApiManager(ssSgitCfgPath);

  //  //����fs��api�ͻص�����ʵ������ʼ�����������ӣ��յ�fs�Ļص���Ϣ���齨��Ӧ��fix��Ϣ�����ҵ���userId��Ӧ��session��ͨ��fixͨ�����ͻ�ȥ��
  //  AutoPtr<IniFileConfiguration> apSgitConf = new IniFileConfiguration(ssSgitCfgPath);
  //  CThostFtdcTraderApi *pTradeApi = CThostFtdcTraderApi::CreateFtdcTraderApi(
  //    apSgitConf->hasProperty("global.flowPath") ? apSgitConf->getString("global.flowPath").c_str() : "");
  //  SharedPtr<CSgitTradeSpi> spTradeSpi = new CSgitTradeSpi(pTradeApi, ssSgitCfgPath);
  //  
		//pTradeApi->IsReviveNtyCapital(false);
		//pTradeApi->RegisterSpi(spTradeSpi);
  //  pTradeApi->SubscribePublicTopic(THOST_TERT_QUICK);
  //  pTradeApi->SubscribePrivateTopic(THOST_TERT_QUICK);
		//std::string ssTradeServer = apSgitConf->getString("global.tradeServer");
  //  pTradeApi->RegisterFront(const_cast<char*>(ssTradeServer.c_str()));
  //  pTradeApi->Init();
		//LOG(INFO_LOG_LEVEL, "RegisterFront tradeServer:%s", ssTradeServer.c_str());

		//����fix����ط��񣨴���fs��apiָ���app��app�յ����������������Ӧ���ֶκ�ͨ�������api��fsϵͳ��������
    FIX::SessionSettings settings( ssFixCfgPath );

    Application application(oSigtApiMngr);
    FIX::FileStoreFactory storeFactory( settings );
    FIX::FileLogFactory logFactory( settings );

		AutoPtr<IniFileConfiguration> apFixConf = new IniFileConfiguration(ssFixCfgPath);

    FIX::ThreadedSocketAcceptor acceptor(application, storeFactory, settings, logFactory );
		acceptor.start();
		LOG(INFO_LOG_LEVEL, "ThreadedSocketAcceptor start. SocketAcceptPort:%d", apFixConf->getInt("DEFAULT.SocketAcceptPort"));

		FIX::ThreadedSocketInitiator initiator( application, storeFactory, settings, logFactory );
    initiator.start();
		LOG(INFO_LOG_LEVEL, "ThreadedSocketInitiator start. SocketConnectHost:%s, SocketConnectPort:%d", 
			apFixConf->getString("DEFAULT.SocketConnectHost").c_str(), apFixConf->getInt("DEFAULT.SocketConnectPort"));

    wait();

    initiator.stop();
		acceptor.stop();
    return 0;
  }
  catch ( std::exception & e )
  {
		std::cout << e.what() << std::endl;
    return 1;
  }
}
