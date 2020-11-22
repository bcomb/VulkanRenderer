#include "Application.h"

/******************************************************************************/
Application::Application()
{

}

/******************************************************************************/
Application::~Application()
{

}

/******************************************************************************/
void Application::init()
{
    // Create window

    // Init resources
}

/******************************************************************************/
void Application::shutdown()
{
    // Create window

    // Finalize resources
}

/******************************************************************************/
void Application::update()
{
}

/******************************************************************************/
void Application::run()
{
    init();

    bool mustStop = false;
    while
        (!mustStop)
    {
        update();
    }

    shutdown();
}