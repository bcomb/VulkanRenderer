#version 450

#extension GL_GOOGLE_include_directive : require

#include "mesh.h"

// Attribute
layout (location = 0) in vec3 iPosition;
layout (location = 1) in vec3 iNormal;
layout (location = 2) in vec2 iTexCoord;

// Varying
layout (location = 0) out vec2 vTexcoord;
layout (location = 1) out vec4 vColor;


layout(binding = 0) uniform UBO
//layout(binding = 0) readonly buffer UBO
{
    Object object;
};

//layout(std430, set = 0, binding = 0) buffer SBO

void main()
{
    vTexcoord = iTexCoord;
    //vColor = vec4(iNormal * 0.5 + vec3(0.5), 1.0);
    //vColor = vColor * object.color;

    if
        (object.color.r != 0 && object.color.g != 0)
    {
        // Should not happen
        vColor = vec4(0.0, 0.0, 1.0,1.0);
    }
    else
    {
        vColor = object.color;
    }
     

    //gl_Position = object.proj * object.view * object.model * vec4(iPosition, 1.0);
    gl_Position = vec4(iPosition, 1.0);
}