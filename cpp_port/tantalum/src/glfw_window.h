#pragma once

#include <GLFW/glfw3.h>

#include <memory>
#include <queue>

#include "gl_context.h"

enum class EventType
{
    NullEvent,
    PushEvent,
    DragEvent,
    MouseWheelEvent,
    KeyboardEvent,
};

struct Event
{
    int       x, y;
    int       dx, dy;
    int       key;
    int       button;
    EventType type;
};

struct PushEvent : public Event
{
    PushEvent()
    {
        type = EventType::PushEvent;
    }
};

struct DragEvent : public Event
{
    DragEvent()
    {
        type = EventType::DragEvent;
    }
};

struct MouseWheelEvent : public Event
{
    MouseWheelEvent()
    {
        type = EventType::MouseWheelEvent;
    }
};

struct KeyboardEvent : public Event
{
    KeyboardEvent()
    {
        type = EventType::KeyboardEvent;
    }
};

class GlfwWindow
{
public:
    GlfwWindow();

    virtual ~GlfwWindow();

    virtual void onDraw();
    virtual void onInitialize(int width, int height);
    virtual void onDeinitialize();
    virtual void onReshape(int width, int height);

    void mainLoop();

    const int w() const;
    void      w(int _w);
    const int h() const;
    void      h(int _h);

    std::shared_ptr<Event> pop_event();
    void                   push_event(const std::shared_ptr<Event>& event);

protected:
    virtual int handle(const Event&);

private:
    GLFWwindow*  _window  = nullptr;
    GLFWmonitor* _monitor = nullptr;
    int          _width, _height;
    bool         _valid = false;

    std::queue<std::shared_ptr<Event>> events;
};
