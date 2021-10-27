#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

int RenderMode_ColorPicking = 1;
int RenderMode_Texture = 2;
int RenderMode_PureColor = 3;

float CarryBit;

uniform int RenderMode;
uniform float FaceIdx;
uniform float LandmarkIdx;
uniform sampler2D Tex;

uniform vec3 PureColor;

void main()
{
	if(RenderMode == RenderMode_ColorPicking)
	{
		CarryBit = floor(LandmarkIdx / 255);
		FragColor = vec4(FaceIdx / 255.0, (LandmarkIdx - 255 * CarryBit) / 255.0, CarryBit / 255.0, 1.0);
	}
	else if(RenderMode == RenderMode_Texture)
	{
		FragColor = texture(Tex, TexCoord);
	}
	else if(RenderMode == RenderMode_PureColor)
	{
		FragColor = vec4(PureColor, 1.0);
	}
	else
	{
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}
}