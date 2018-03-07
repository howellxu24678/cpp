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
    , m_cDefaultSpecHedge(THOST_FTDC_ECIDT_Speculation)
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

#endif // __CONST_H__