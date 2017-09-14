#ifndef __CONVERT_H__
#define __CONVERT_H__

#include <string>

class Convert
{
public:
	Convert(const std::string ssCfgPath);
	~Convert();

	bool Init();

private:
	std::string		m_ssCfgPath;
};

#endif // __CONVERT_H__
