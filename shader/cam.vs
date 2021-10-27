#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Proj;

void main()
{
    gl_Position = Proj * View * Model * vec4(aPos * 0.1, 1.0);
}