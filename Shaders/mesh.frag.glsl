#version 450

layout (location = 0) in vec2 vTexcoord;
layout (location = 1) in vec4 vColor;

layout(location = 0) out vec4 oColor0;

layout(binding = 1, set = 0) uniform sampler2D uColorMap;

void main()
{
    oColor0 = texture(uColorMap,vTexcoord.xy);
}