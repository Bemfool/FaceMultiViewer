#version 330 core
layout (location = 0) in vec3 aPos;

float sc = 0.1;

void main()
{
    gl_Position = vec4(aPos.x * 0.05, aPos.y * 0.07, aPos.z * 0.1, 1.0);
}