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

#include "log.h"

#include "Poco/Foundation.h"
#include "Poco/LocalDateTime.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/Timestamp.h"
#include "Poco/Thread.h"
#include "Poco/Timespan.h"

#include "Poco/Util/Util.h"
#include "Poco/Util/Timer.h"
#include "Poco/Util/TimerTask.h"
#include "Poco/Util/TimerTaskAdapter.h"

#include "Poco/Util/IniFileConfiguration.h"

#include "Poco/FileStream.h"

using namespace Poco;
using namespace Poco::Util;

class TimerTest
{
public: 
	TimerTest(){}
	~TimerTest(){}

	void onTimer(TimerTask& task)
	{
		cout << "onTimer" << endl;
		LOG(INFO_LOG_LEVEL, "onTimer %d", 123);
		//LOG(INFO_LOG_LEVEL, "TEST %d", 123);
	}

	void onTT()
	{
		cout << "onTT" << endl;
	}
};

void wait()
{
  std::cout << "Type Ctrl-C to quit" << std::endl;
  while(true)
  {
    FIX::process_sleep(1);
  }
}

void testPoco()
{
  LocalDateTime now;
  LOG(INFO_LOG_LEVEL, "now is %s", DateTimeFormatter::format(now, DateTimeFormat::ISO8601_FORMAT).c_str());
  LOG(INFO_LOG_LEVEL, "now is %s", DateTimeFormatter::format(now, DateTimeFormat::SORTABLE_FORMAT).c_str());

  Timestamp s;
  Thread::sleep(5000);
  Timespan sp = s.elapsed();

  LOG(INFO_LOG_LEVEL, "elapsed:%ld ms", sp.totalMilliseconds());
}

void testLogCost()
{
	Timestamp s;
	for(int i = 0; i < 1000; i++)
	{
		LOG(ERROR_LOG_LEVEL, "TEST %s, %f, %d","aeradfa", 234.354, i);
	}
	Timespan sp = s.elapsed();
	LOG(INFO_LOG_LEVEL, "elapsed:%ld ms", sp.totalMilliseconds());
}



void testTimer(Util::Timer &timer, TimerTest &tt)
{
	LOG(WARN_LOG_LEVEL, "testTimer");

	//Timestamp time;
	//time += 10;

	//Clock scheduleClock;
	//scheduleClock += 5 * 1000;

	
	TimerTask::Ptr pTask = new TimerTaskAdapter<TimerTest>(tt, &TimerTest::onTimer);

	timer.schedule(pTask, 5000, 5000);
}

void testIniFile(const std::string &file)
{
	static const std::string iniFile = 
		"; comment\n"
		"  ; comment  \n"
		"prop1=value1\n"
		"  prop2 = value2  \n"
		"[section1]\n"
		"prop1 = value3\r\n"
		"\tprop2=value4\r\n"
		";prop3=value7\r\n"
		"\n"
		"  [ section 2 ]\n"
		"prop1 = value 5\n"
		"\t   \n"
		"Prop2 = value6";

	std::istringstream istr(iniFile);	
	AutoPtr<IniFileConfiguration> pConf = new IniFileConfiguration(istr);
	LOG(INFO_LOG_LEVEL, pConf->getString("prop1").c_str());


	AutoPtr<IniFileConfiguration> pConf2 = new IniFileConfiguration(file);
	//IniFileConfiguration cfg("E:\\pan\\work\\code\\github\\cpp\\fs\bin\\config\\executor.cfg");
	LOG(INFO_LOG_LEVEL, pConf2->getString("SESSION.BeginString").c_str());
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
	std::string ssFixCfgPath = argv[2];
	std::string ssSgitCfgPath = argv[3];

  try
  {
		InitLog(ssLogCfgPath);
		
    FIX::SessionSettings settings( ssFixCfgPath );

    Application application;
    FIX::FileStoreFactory storeFactory( settings );
    FIX::FileLogFactory logFactory( settings );

		AutoPtr<IniFileConfiguration> pFixConf = new IniFileConfiguration(ssFixCfgPath);

    FIX::ThreadedSocketAcceptor acceptor(application, storeFactory, settings, logFactory );
		acceptor.start();
		LOG(INFO_LOG_LEVEL, "ThreadedSocketAcceptor start. SocketAcceptPort:%d", pFixConf->getInt("DEFAULT.SocketAcceptPort"));

		FIX::ThreadedSocketInitiator initiator( application, storeFactory, settings, logFactory );
    initiator.start();
		LOG(INFO_LOG_LEVEL, "ThreadedSocketInitiator start. SocketConnectHost:%s, SocketConnectPort:%d", 
			pFixConf->getString("DEFAULT.SocketConnectHost").c_str(), pFixConf->getInt("DEFAULT.SocketConnectPort"));

    wait();

    initiator.stop();
		acceptor.stop();
    return 0;
  }
  catch ( std::exception & e )
  {
		LOG(FATAL_LOG_LEVEL, e.what());
    return 1;
  }
}
