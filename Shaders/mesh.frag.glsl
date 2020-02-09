#version 450

layout (location = 0) in vec2 vTexcoord;
layout (location = 1) in vec4 vColor;

layout(location = 0) out vec4 oColor0;

void main()
{
    oColor0 = vColor;
}