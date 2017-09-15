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

#include "SgitApiManager.h"

#include "Toolkit.h"
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


bool checkCfgPath(AutoPtr<Poco::Util::IniFileConfiguration> apCfg, const std::string ssKey, std::string &ssValue)
{
	CToolkit::getStrinIfSet(apCfg, G_CONFIG_GLOBAL_SECTION + "." + ssKey, ssValue);
	if (!CToolkit::isExist(ssValue))
	{
		std::cout << "Failed to find log cfg file of:" << ssKey << ", path:" << ssValue << std::endl;
		return false;
	}
	return true;
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

		if (!CToolkit::isExist(ssConfigPath))
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

		//��ʼ����־
		initialize ();
		ConfigureAndWatchThread configureThread(ssLogCfgPath, 5 * 1000);

		LOG(INFO_LOG_LEVEL, "Fsfix is preparing initialization, Version[%s], Build[%s:%s]", G_VERSION.c_str(), __DATE__, __TIME__);
		LOG(INFO_LOG_LEVEL, "path current:%s", Path::current().c_str());

		/*
		1. һ������Ա�˺Ŷ�Ӧһ��apiʵ������ʼ��ʱ��ȡ�����ļ��еĽ���Ա�˺ź�������Ϣ������Ӧ��apiʵ��
		2. fix application �� onMessage�յ���Ϣ�󣬽�����Ϣ������  XXX �ҵ���Ӧ�� apiʵ���ύ����
		3. apiʵ���ص���������ҵ���Ӧ��fix sessionID ���ͻ�ȥ
		*/
		CSgitContext oSigtCtx = CSgitContext(ssSgitCfgPath, ssDictCfgPath);
		if(!oSigtCtx.Init())
		{
			LOG(FATAL_LOG_LEVEL, "Failed to Init CSgitApiManager");
		}

		//����fix����ط��񣨴���fs��apiָ���app��app�յ����������������Ӧ���ֶκ�ͨ�������api��fsϵͳ��������
		FIX::SessionSettings settings( ssFixCfgPath );

		Application application(oSigtCtx);
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
