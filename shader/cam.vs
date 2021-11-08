#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Proj;

out vec3 FragPos;
out vec3 Normal;

void main()
{
    FragPos = aPos;
    Normal = aNormal; 
    gl_Position = Proj * View * Model * vec4(aPos * 0.1, 1.0);
}