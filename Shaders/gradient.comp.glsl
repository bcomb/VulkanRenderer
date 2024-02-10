#version 460

layout (local_size_x = 16, local_size_y = 16) in;

layout(rgba8, set = 0, binding = 0) uniform image2D image;

// push constant block
layout (push_constant) uniform constants_t {
    vec4 data0;
    vec4 data1;
    vec4 data2;
    vec4 data3;
} PushConstants;


void main() 
{
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(image);

    if(texelCoord.x < size.x && texelCoord.y < size.y)
    {
        vec4 color = vec4(0.0, 0.0, 0.0, 1.0);        

        if(gl_LocalInvocationID.x != 0 && gl_LocalInvocationID.y != 0)
        {
            color.x = float(texelCoord.x)/(size.x);
            color.y = float(texelCoord.y)/(size.y);	
        }
    
        imageStore(image, texelCoord, color * PushConstants.data0);
    }
}

