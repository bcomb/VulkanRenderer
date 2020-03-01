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