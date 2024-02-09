#pragma once

#include <stdint.h>

class App
{
public:
	App(uint32_t w, uint32_t h);
	int run();

private:
	void init_vulkan();
};
