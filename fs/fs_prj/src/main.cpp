#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786)
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
#include "Const.h"

#include "Poco/Foundation.h"
#include "Poco/Path.h"
#include "Poco/File.h"
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

#include "SgitContext.h"

#include "Toolkit.h"
//#include "Poco/FileStream.h"
#include "Poco/ExpireCache.h"

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


bool checkCfgPath(AutoPtr<Poco::Util::IniFileConfiguration> apCfg, const std::string ssKey, std::string &ssValue)
{
	CToolkit::GetStrinIfSet(apCfg, G_CONFIG_GLOBAL_SECTION + "." + ssKey, ssValue);
	if (!CToolkit::IsExist(ssValue))
	{
		std::cout << "Failed to find log cfg file of:" << ssKey << ", path:" << ssValue << std::endl;
		return false;
	}
	return true;
}

struct STUtest{
  int m_i;

  void upate(int i)
  {
    m_i = i;
  }
};

void TestExpireCache()
{
  ExpireCache<int, STUtest> cache;
  STUtest oStutest;
  memset(&oStutest, 0, sizeof(STUtest));
  cache.add(1, oStutest);
  cache.add(2, oStutest);

  cache.get(1)->upate(20);
  SharedPtr<STUtest> spTest = cache.get(3);
  cout << "isNull:" << spTest.isNull() << endl;

  cout << "TestExpireCache 1:" << cache.get(1)->m_i << endl;
  cout << "TestExpireCache 2:" << cache.get(2)->m_i << endl;
}

int main( int argc, char** argv )
{
	std::string ssConfigPath = "";
	if(argc < 2)
	{
		std::cout << "Have not indicate the cfg path, will use the default path:" << G_CONFIG_PATH << std::endl;
		ssConfigPath = G_CONFIG_PATH;
	}
	else
	{
		ssConfigPath = argv[1];
	}
	try
	{

		if (!CToolkit::IsExist(ssConfigPath))
		{
			std::cout << "Failed to find global cfg file, path:" << ssConfigPath << std::endl;
			return 0;
		}

		std::string ssLogCfgPath = "";
		std::string ssSgitCfgPath = "";
		std::string ssFixCfgPath = "";
    std::string ssDictCfgPath = "";

		AutoPtr<IniFileConfiguration> apGlobalConf = new IniFileConfiguration(ssConfigPath);
		if (!checkCfgPath(apGlobalConf, G_CONFIG_LOG, ssLogCfgPath)) return 0;
		if (!checkCfgPath(apGlobalConf, G_CONFIG_SGIT, ssSgitCfgPath)) return 0;
		if (!checkCfgPath(apGlobalConf, G_CONFIG_FIX, ssFixCfgPath)) return 0;
    if (!checkCfgPath(apGlobalConf, G_CONFIG_DICT, ssDictCfgPath)) return 0;

		//初始化日志
		initialize ();
		ConfigureAndWatchThread configureThread(ssLogCfgPath, 5 * 1000);

		LOG(INFO_LOG_LEVEL, "Fsfix is preparing initialization, Version[%s], Build[%s:%s]", G_VERSION.c_str(), __DATE__, __TIME__);
		LOG(INFO_LOG_LEVEL, "path current:%s", Path::current().c_str());

		/*
		1. 一个交易员账号对应一个api实例，初始化时读取配置文件中的交易员账号和密码信息创建对应的api实例
		2. fix application 中 onMessage收到消息后，解析消息，根据  XXX 找到对应的 api实例提交请求
		3. api实例回调后，组包，找到对应的fix sessionID 发送回去
		*/
		SharedPtr<CSgitContext> apSigtCtx = new CSgitContext(ssSgitCfgPath, ssDictCfgPath);
		if(!apSigtCtx->Init())
		{
			LOG(FATAL_LOG_LEVEL, "Failed to Init CSgitApiManager");
		}

		//创建fix的相关服务（带上fs的api指针给app，app收到订单的请求解析对应的字段后通过飞鼠的api向fs系统发送请求）
		FIX::SessionSettings settings( ssFixCfgPath );

		Application application(apSigtCtx);
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


/*
遗留问题：
1. 上期所平今平昨
2. 执行回报成交价格
3. 
*/