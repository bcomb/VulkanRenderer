#pragma once


class Application
{
public:
    Application();
    ~Application();

    void run();

protected:

    void init();
    void shutdown();

    // Main loop
    void update();

};