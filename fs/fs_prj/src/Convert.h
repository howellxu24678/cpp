#ifndef __CONVERT_H__
#define __CONVERT_H__

#include <string>

class Convert
{
public:
  enum EnDirect { Fix2Fs, Fs2Fix };

	Convert(const std::string ssCfgPath);
	~Convert();

	bool Init();

  char GetCvt(const int iField,const char cValue);

private:
	std::string		m_ssCfgPath;
};

#endif // __CONVERT_H__
