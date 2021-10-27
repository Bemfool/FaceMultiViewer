#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>

namespace file_utils
{
	static std::string Id2Str(unsigned int id)
	{
		if (id < 10)
			return "0000000" + std::to_string(id);
		else
			return "000000" + std::to_string(id);
	}

	static unsigned int Str2Id(std::string str)
	{
		return std::stoi(str);
	}

};

#endif