#version 330 core
layout (location = 0) in float isBegin;

uniform vec4 EndPoint;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

float strength = 10.0f;

void main()
{
	if(isBegin == 0.0f)
	{
		gl_Position = projection * view * model * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	else
	{
		gl_Position = projection * view * model * vec4(EndPoint.x * strength, EndPoint.y * strength, EndPoint.z * strength, 1.0f);
	}
	    
}