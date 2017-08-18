/****************************************************************************
** Copyright (c) 2001-2014
**
** This file is part of the QuickFIX FIX Engine
**
** This file may be distributed under the terms of the quickfixengine.org
** license as defined by quickfixengine.org and appearing in the file
** LICENSE included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.quickfixengine.org/LICENSE for licensing information.
**
** Contact ask@quickfixengine.org if any conditions of this licensing are
** not clear to you.
**
****************************************************************************/

#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#else
#include "config.h"
#endif

#include "quickfix/FileStore.h"
#include "quickfix/ThreadedSocketAcceptor.h"
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


using namespace Poco;

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


int main( int argc, char** argv )
{
  initialize ();
  LogLog::getLogLog()->setInternalDebugging(true);
  ConfigureAndWatchThread configureThread(getPropertiesFileArgument(argc, argv), 5 * 1000);

  LOG(DEBUG_LOG_LEVEL, "TEST %d", 123);
  LOG(INFO_LOG_LEVEL, "TEST %d", 123);
  LOG(WARN_LOG_LEVEL, "TEST %d", 123);
  LOG(ERROR_LOG_LEVEL, "TEST %d, %s, %f", 123, "aeradfa", 234.354);

  testPoco();

  if ( argc < 2 )
  {
    std::cout << "usage: " << argv[ 0 ]
    << " FILE." << std::endl;
    return 0;
  }
  std::string file = argv[ 1 ];

  try
  {
    FIX::SessionSettings settings( file );

    Application application;
    FIX::FileStoreFactory storeFactory( settings );
    FIX::FileLogFactory logFactory( settings );
    FIX::ThreadedSocketAcceptor acceptor( application, storeFactory, settings, logFactory );

    acceptor.start();
    wait();
    acceptor.stop();
    return 0;
  }
  catch ( std::exception & e )
  {
    std::cout << e.what() << std::endl;
    return 1;
  }
}
