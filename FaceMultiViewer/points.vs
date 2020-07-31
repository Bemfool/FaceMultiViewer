#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 PointColor;

void main()
{
	gl_PointSize = 10.0;
    gl_Position = vec4(aPos.x, aPos.y, -1.0, 1.0);
	PointColor = aColor;
}