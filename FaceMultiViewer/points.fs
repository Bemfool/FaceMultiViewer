#version 330 core
out vec4 FragColor;

in vec3 PointColor;

void main()
{
    FragColor = vec4(PointColor, 1.0);
} 