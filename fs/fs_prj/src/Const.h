#ifndef __CONST_H__
#define __CONST_H__

#include "sgit/SgitFtdcUserApiDataType.h"

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

const int G_WAIT_TIME = 10000;

const std::string G_VERSION = "20170901.01.01";
const std::string G_CONFIG_PATH = "./config/global.cfg";
const std::string G_CONFIG_GLOBAL_SECTION = "global";
const std::string G_CONFIG_LOG = "log";
const std::string G_CONFIG_SGIT = "sgit";
const std::string G_CONFIG_FIX = "fix";
const std::string G_CONFIG_DICT = "dict";


//����Fix�û��Ĺؼ���Ϣ���ظ���Ϣʱ��Ҫ�õ�
struct STUserInfo
{
  STUserInfo() 
		: m_ssOnBehalfOfCompID("")
    , m_enCvtType(Convert::Unknow)
    , m_iCloseTodayYesterdayTag(0)
    , m_iSpecHedgeTag(0)
    , m_cDefaultSpecHedge(THOST_FTDC_HF_Speculation)
  {}

  FIX::SessionID      m_oSessionID;
	std::string					m_ssOnBehalfOfCompID;
  //��������
  Convert::EnCvtType  m_enCvtType;
  //ƽ��ƽ����Զ���tag
  int                 m_iCloseTodayYesterdayTag;
  //Ͷ���ױ����Զ���tag
  int                 m_iSpecHedgeTag;
  //Ĭ��Ͷ���ױ�ֵ������Ͷ���ױ�tag����ʽָ��ʱȡ��Ĭ��ֵ�������ֵ�ת������û������Ĭ��Ͷ��
  char                m_cDefaultSpecHedge;
};

//���鴦���У���ĵĶ��Ĳ���
struct STUScrbParm
{
  STUScrbParm()
    : m_ssSessionKey("")
    , m_iDepth(0)
  {
    m_setEntryTypes.clear();
  }

  std::string         m_ssSessionKey;
  int                 m_iDepth;
  std::set<char>      m_setEntryTypes;

  bool operator==(const STUScrbParm &stuScrbParm) const
  {
    //return this->m_ssSessionKey == stuScrbParm.m_ssSessionKey && 
    //  this->m_iDepth == stuScrbParm.m_iDepth && 
    //  this->m_setEntryTypes == stuScrbParm.m_setEntryTypes;
    return this->m_ssSessionKey == stuScrbParm.m_ssSessionKey;
  }

  bool operator<(const STUScrbParm &stuScrbParm) const
  {
    if (this->m_ssSessionKey < stuScrbParm.m_ssSessionKey)
      return true;
    if (this->m_iDepth < stuScrbParm.m_iDepth)
      return true;
    if (this->m_setEntryTypes < stuScrbParm.m_setEntryTypes)
      return true;

    return false;
  }
};

#endif // __CONST_H__