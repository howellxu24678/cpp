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

//#include "Poco/FileStream.h"

#include "SgitTradeSpi.h"

using namespace Poco;
using namespace Poco::Util;

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
		//初始化日志
		initialize ();
		ConfigureAndWatchThread configureThread(ssLogCfgPath, 5 * 1000);
		
    //创建fs的api和回调处理实例，初始化与飞鼠的连接（收到fs的回调信息后，组建相应的fix消息，并找到该userId对应的session，通过fix通道发送回去）
    AutoPtr<IniFileConfiguration> apSgitConf = new IniFileConfiguration(ssSgitCfgPath);
    CThostFtdcTraderApi *pTradeApi = CThostFtdcTraderApi::CreateFtdcTraderApi(
      apSgitConf->hasProperty("global.flowPath") ? apSgitConf->getString("global.flowPath").c_str() : "");
    SharedPtr<CSgitTradeSpi> spTradeSpi = new CSgitTradeSpi(pTradeApi, ssSgitCfgPath);
    
		pTradeApi->IsReviveNtyCapital(false);
		pTradeApi->RegisterSpi(spTradeSpi);
    pTradeApi->SubscribePublicTopic(THOST_TERT_QUICK);
    pTradeApi->SubscribePrivateTopic(THOST_TERT_QUICK);
		std::string ssTradeServer = apSgitConf->getString("global.tradeServer");
    pTradeApi->RegisterFront(const_cast<char*>(ssTradeServer.c_str()));
    pTradeApi->Init();
		LOG(INFO_LOG_LEVEL, "RegisterFront tradeServer:%s", ssTradeServer.c_str());

		//创建fix的相关服务（带上fs的api指针给app，app收到订单的请求解析对应的字段后通过飞鼠的api向fs系统发送请求）
    FIX::SessionSettings settings( ssFixCfgPath );

    Application application(pTradeApi);
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
