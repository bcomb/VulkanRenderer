#version 450

layout (location = 0) in vec3 iPosition;
layout (location = 1) in vec3 iNormal;
layout (location = 2) in vec2 iTexCoord;

layout (location = 0) out vec2 vTexcoord;
layout (location = 1) out vec4 vColor;

void main()
{
    vTexcoord = iTexCoord;
    vColor = vec4(iNormal * 0.5 + vec3(0.5), 1.0);

    gl_Position = vec4(iPosition, 1.0);
}