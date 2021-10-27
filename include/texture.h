#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "config.h"

class Texture
{
public:
	Texture() = default;
	Texture(const std::string& p)
	{
		int nrComponents;
		data = stbi_load(p.c_str(), &width, &height, &nrComponents, 0);
		if (!data)
		{
			std::cout << "Texture failed to load at path: " << p << std::endl;
			stbi_image_free(data);
		}
	}

	unsigned int id;
	int width;
	int height;
	unsigned char *data;
};


#endif // TEXTURE_H