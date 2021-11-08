#version 330 core
layout (location = 0) in float isBegin;

uniform vec4 EndPoint;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Proj;

float strength = 200.0f;

void main()
{
	if(isBegin == 0.0f)
	{
		gl_Position = Proj * View * Model * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	else
	{
		gl_Position = Proj * View * Model * vec4(EndPoint.x * strength, EndPoint.y * strength, EndPoint.z * strength, 1.0f);
	}
	    
}