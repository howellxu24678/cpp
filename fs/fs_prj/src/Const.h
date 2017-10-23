#ifndef __CONST_H__
#define __CONST_H__

const std::string G_VERSION = "20170901.01.01";
const std::string G_CONFIG_PATH = ".//config//global.cfg";
const std::string G_CONFIG_GLOBAL_SECTION = "global";
const std::string G_CONFIG_LOG = "log";
const std::string G_CONFIG_SGIT = "sgit";
const std::string G_CONFIG_FIX = "fix";
const std::string G_CONFIG_DICT = "dict";


struct STUFixInfo{
  //收到的原始资金账号
  std::string     m_ssAcctRecv;
  FIX::Header     m_oHeader;

  FIX::SessionID  m_oSessionID;
};

#endif // __CONST_H__