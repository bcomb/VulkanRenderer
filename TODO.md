# Abstract Renderer

- [ ] Definir coding rules, à minima sur le nommage des fontions/classe, attribut membre/local/static
- [x] Mesh loader
- [ ] No STL (ou juste algorithm) EASTL?
- [ ] Minimal math : Vec3 / Matrix ...

## App
- [ ] Abstract mouse/keyboard event
- [ ] ...

## Vulkan Renderer
- [x] Init
- [x] Swapchain
- [x] Vertex Buffer / Index Buffer
- [x] Shader
- [ ] Texture
- [x] Submit command
- [ ] Capturer avec NSIGHT et/ou Renderdoc
- [ ] native ImGui impl dans notre renderer

## TODO list / bugs
- [x] Double buffer command in order to remove vkDeviceWaitIdle in main loop
- [ ] Double buffered the CommandPool (i forgot this)
- [x] Add stage buffer to upload Data (Vertex/IndexBuffer etc) in device memory for non unified memory device
- [x] Try UBO/SBO / descriptor set / descriptor bind
- [ ] Try ConstantBuffer (small sized buffer use for high frequency update, for ModelView matrix)
- [ ] Manage the Push descriptor extension instead of BindDescriptor
- [ ] Clean/refactor all the code in different file
- [ ] Add the possibility to use include directive in GLSL (use the GOOGLE extension at the moment)
- [ ] Add reflection
- [ ] Add texture binding
- [ ] Bench the way i manage the double buffering synchronisation by mesure with no double buffer
- [ ] Use 1.2 synchronization object, VkSemaphore, VK_KHR_timeline_semaphore comme mecanisme de synchronisation