#include "gl_context.h"

//
#include <iostream>
#include <stdexcept>

#include "glfw_window.h"
#include "imgui_impl_glfw.h"

using std::cerr;
using std::cout;
using std::endl;

GlfwWindow* win = NULL;

static void error(int errnum, const char* errmsg)
{
    std::cerr << errnum << " : " << errmsg << std::endl;
}
// void error(int errnum, const char* errmsg)
//{
//    globjects::critical() << errnum << ": " << errmsg << std::endl;
//}

static void idle(void* s)
{
#if 0
    if (win != NULL) win->redraw();
#endif
}

static void reshape(GLFWwindow* window, int w, int h);
static void key_callback(
    GLFWwindow* window, int key, int scancode, int action, int mods);
static void mouse_button_callback(GLFWwindow* window,
                                  int         button,
                                  int         action,
                                  int         mods);
static void cursor_position_callback(GLFWwindow* window, double x, double y);
static void scroll_callback(GLFWwindow* window, double x, double y);

GlfwWindow::GlfwWindow()
{
    glfwSetErrorCallback(error);

    if (!glfwInit())
    {
        std::cerr << "Cannot initialize GLFW\n";
        throw std::runtime_error("Cannot initialize GLFW");
    }

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    _width  = 1024;
    _height = 576;
    _window = glfwCreateWindow(_width, _height, "Tantalum", nullptr, nullptr);

    if (!_window)
    {
        std::cerr << "Cannot create GLFW window\n";
        glfwTerminate();
        throw std::runtime_error("Cannot create GLFW window");
    }

    glfwMakeContextCurrent(_window);
    // glfwSwapInterval(1);
    glfwSwapInterval(0);

#if USE_GLAD
    gladLoadGL(glfwGetProcAddress);
#else
    // only resolve functions that are actually used (lazy)
    glbinding::initialize(glfwGetProcAddress, false);

    // this is not so compatible with immediate mode drawing
    // because glGetError is not allowed in glBein/glEnd block
    // glbinding::aux::enableGetErrorCallback();

    // Initialize globjects (internally initializes glbinding, and registers the
    // current context)
    if (false)
    {
        globjects::init(
            [](const char* name) { return glfwGetProcAddress(name); },
            globjects::Shader::IncludeImplementation::Fallback);
    }
    else
    {
        // globjects::init(
        //    [](const char* name) { return glfwGetProcAddress(name); });
        globjects::init(glfwGetProcAddress);
    }

    if (0)
    {
        cout << "OpenGL Version:  " << glbinding::aux::ContextInfo::version()
             << endl;
        cout << "OpenGL Vendor:   " << glbinding::aux::ContextInfo::vendor()
             << endl;
        cout << "OpenGL Renderer: " << glbinding::aux::ContextInfo::renderer()
             << endl;
    }

    // stack overflow
    // glbinding::Binding::addContextSwitchCallback(
    //    [](glbinding::ContextHandle handle) { globjects::setContext(handle);
    //    });

    glbinding::useCurrentContext();
    globjects::setCurrentContext();
#endif

    ImGui_ImplGlfw_Init(_window, true);

    win = this;

    reshape(_window, _width, _height);

    glfwSetFramebufferSizeCallback(_window, reshape);
    glfwSetKeyCallback(_window, key_callback);
    glfwSetMouseButtonCallback(_window, mouse_button_callback);
    glfwSetCursorPosCallback(_window, cursor_position_callback);
    glfwSetScrollCallback(_window, scroll_callback);
}

GlfwWindow::~GlfwWindow()
{
    ImGui_ImplGlfw_Shutdown();
    glfwTerminate();
}

static int prevX, prevY;
static int cursorX, cursorY;
static int cur_botton = -1;

static void reshape(GLFWwindow* window, int w, int h)
{
    win->w(w);
    win->h(h);

    win->onReshape(w, h);
}

static void key_callback(
    GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE && mods == 0)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    auto e = std::make_shared<KeyboardEvent>();
    e->key = key;

    win->push_event(e);
}

static void mouse_button_callback(GLFWwindow* window,
                                  int         button,
                                  int         action,
                                  int         mods)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT)
        return;

    if (action == GLFW_PRESS)
    {
        auto e    = std::make_shared<PushEvent>();
        e->x      = cursorX;
        e->y      = cursorY;
        e->button = button;
        win->push_event(e);

        cur_botton = button;
    }
    else
    {
        cur_botton = -1;
    }
}

static void cursor_position_callback(GLFWwindow* window, double x, double y)
{
    cursorX = x;
    cursorY = y;

    auto e    = std::make_shared<DragEvent>();
    e->x      = cursorX;
    e->y      = cursorY;
    e->button = cur_botton;
    win->push_event(e);
}

static void scroll_callback(GLFWwindow* window, double x, double y)
{
    auto e = std::make_shared<MouseWheelEvent>();
    e->dx  = -x;
    e->dy  = -y;
    win->push_event(e);
}

int GlfwWindow::handle(const Event& e)
{
    switch (e.type)
    {
        case EventType::PushEvent:
            prevX = e.x;
            prevY = e.y;
            return 1;
        case EventType::DragEvent:
            prevX = e.x;
            prevY = e.y;

            return 1;
        case EventType::MouseWheelEvent:
            return 1;

        case EventType::KeyboardEvent:
            return 1;
        default:
            return 0;
    }
    return 0;
}

std::shared_ptr<Event> GlfwWindow::pop_event()
{
    if (events.empty()) return nullptr;

    auto ret = events.front();
    events.pop();
    return ret;
}

void GlfwWindow::push_event(const std::shared_ptr<Event>& event)
{
    events.push(event);
}

void GlfwWindow::onDraw()
{
}

void GlfwWindow::onInitialize(int width, int height)
{
}

void GlfwWindow::onDeinitialize()
{
}

void GlfwWindow::onReshape(int width, int height)
{
    w(width);
    h(height);
    gl::glViewport(0, 0, width, height);
}

void GlfwWindow::mainLoop()
{
    this->onInitialize(_width, _height);

    while (!glfwWindowShouldClose(_window))
    {
        std::shared_ptr<Event> e;
        while (e = pop_event())
        {
            handle(*e);
        }

        this->onDraw();

        glfwSwapBuffers(_window);
        glfwPollEvents();
    }

    this->onDeinitialize();
}

const int GlfwWindow::w() const
{
    return _width;
}

void GlfwWindow::w(int _w)
{
    _width = _w;
}

const int GlfwWindow::h() const
{
    return _height;
}

void GlfwWindow::h(int _h)
{
    _height = _h;
}
