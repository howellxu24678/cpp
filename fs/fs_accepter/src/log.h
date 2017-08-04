#ifndef __LOG_H__
#define __LOG_H__


#include <log4cplus/logger.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/helpers/fileinfo.h>


using namespace std;
using namespace log4cplus;
using namespace log4cplus::helpers;

/*
FATAL_LOG_LEVEL,ERROR_LOG_LEVEL,WARN_LOG_LEVEL,INFO_LOG_LEVEL,DEBUG_LOG_LEVEL,TRACE_LOG_LEVEL
*/
#if !defined(LOG)
#define LOG(logLevel, ...)                               \
  LOG4CPLUS_MACRO_FMT_BODY (Logger::getRoot(), logLevel, __VA_ARGS__)
#endif


log4cplus::tstring
  getPropertiesFileArgument (int argc, char * argv[])
{
  if (argc >= 3)
  {
    char const * arg = argv[2];
    log4cplus::tstring file = LOG4CPLUS_C_STR_TO_TSTRING (arg);
    log4cplus::helpers::FileInfo fi;
    if (getFileInfo (&fi, file) == 0)
      return file;
  }

  return LOG4CPLUS_TEXT ("log4cplus.properties");
}


#endif // __LOG_H__
