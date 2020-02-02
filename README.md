Toying with vulkan

# Generate solution
mkdir Build
cd Build
cmake ..



# Compile shader
glslangValidator.exe -V triangle.vert.glsl
glslangValidator.exe -V triangle.frag.glsl