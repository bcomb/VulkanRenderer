#version 450

// Attribute
layout (location = 0) in vec3 iPosition;
layout (location = 1) in vec3 iNormal;
layout (location = 2) in vec2 iTexCoord;

// Varying
layout (location = 0) out vec2 vTexcoord;
layout (location = 1) out vec4 vColor;

//  Global 'per renderpass'
struct Global
{
    mat4 proj;
    mat4 view;
};

// Struct define 'per object'
struct Object 
{
    mat4 proj;
    mat4 view;
    mat4 model;
    vec4 color;
};

// Constant buffer 'per draw' (small size 128/256 bytes)
struct Constant
{
    mat4 model;
    //mat4 view
};

// Constant (todo)
//layout(push_constant) uniform block
//{
//    Globals globals;
//};

layout(binding = 0) uniform UBO
//layout(binding = 0) readonly buffer PerObject
{
    Object object;
};

//layout(std430, set = 0, binding = 0) buffer SBO

void main()
{
    vTexcoord = iTexCoord;
    //vColor = vec4(iNormal * 0.5 + vec3(0.5), 1.0);
    //vColor = vColor * object.color;
    vColor = object.color;
    //gl_Position = object.proj * object.view * object.model * vec4(iPosition, 1.0);
    gl_Position = vec4(iPosition, 1.0);
}