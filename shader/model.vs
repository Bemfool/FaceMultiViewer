#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aColor;

out vec3 FragPos;
out vec3 Normal;
out vec4 Color;

uniform mat4 View;
uniform mat4 Proj;

void main()
{
    FragPos = aPos;
    Normal = aNormal; 
	Color = aColor;
    gl_Position = Proj * View * vec4(aPos, 1.0);
}