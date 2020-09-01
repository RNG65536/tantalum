#pragma once

#include "glfw_window.h"

class DemoTantalum : public GlfwWindow
{
public:
    DemoTantalum();

    void onDraw() override;
    void onInitialize(int width, int height) override;
    void onDeinitialize() override;
    void onReshape(int width, int height) override;

protected:
    int handle(const Event& e) override;
};