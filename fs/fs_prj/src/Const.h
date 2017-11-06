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


//����Fix�û��Ĺؼ���Ϣ���ظ���Ϣʱ��Ҫ�õ�
struct STUFIXInfo
{
  STUFIXInfo() 
		: m_ssOnBehalfOfCompID("")
    //, m_enCvtType(Convert::Unknow)
  {}

  FIX::SessionID      m_oSessionID;
	std::string					m_ssOnBehalfOfCompID;
  ////��������
  //Convert::EnCvtType  m_enCvtType;
};

#endif // __CONST_H__