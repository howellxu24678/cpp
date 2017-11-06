#ifndef __CONST_H__
#define __CONST_H__

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

const std::string G_VERSION = "20170901.01.01";
const std::string G_CONFIG_PATH = ".//config//global.cfg";
const std::string G_CONFIG_GLOBAL_SECTION = "global";
const std::string G_CONFIG_LOG = "log";
const std::string G_CONFIG_SGIT = "sgit";
const std::string G_CONFIG_FIX = "fix";
const std::string G_CONFIG_DICT = "dict";


//保存Fix用户的关键信息，回复消息时需要用到
struct STUFIXInfo
{
  STUFIXInfo() 
		: m_ssOnBehalfOfCompID("")
    //, m_enCvtType(Convert::Unknow)
  {}

  FIX::SessionID      m_oSessionID;
	std::string					m_ssOnBehalfOfCompID;
  ////代码类型
  //Convert::EnCvtType  m_enCvtType;
};

#endif // __CONST_H__