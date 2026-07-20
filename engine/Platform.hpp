#pragma once

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "Fact.hpp"

struct GLFWwindow;

struct GlfwInitialization {
    GlfwInitialization();
    ~GlfwInitialization();

    GlfwInitialization(const GlfwInitialization&)            = delete;
    GlfwInitialization(GlfwInitialization&&)                 = default;
    GlfwInitialization& operator=(const GlfwInitialization&) = delete;
    GlfwInitialization& operator=(GlfwInitialization&&)      = default;
};

struct GlfwKeyEvent {
    int key;
    int scancode;
    int action;
    int mods;
};

struct GlfwMouseButtonEvent {
    int button;
    int action;
    int mods;
};

struct GlfwMouseMoveEvent {
    double x;
    double y;
};

struct GlfwScrollEvent {
    double dx;
    double dy;
};

struct GlfwFramebufferResizeEvent {
    int width;
    int height;
};

using GlfwEvent = std::variant<
    GlfwKeyEvent,
    GlfwMouseButtonEvent,
    GlfwMouseMoveEvent,
    GlfwScrollEvent,
    GlfwFramebufferResizeEvent
>;

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::pair<int, int> getPrimaryMonitorResolution(Proof<const GlfwInitialization>);


struct WindowInitConfig
{
    std::pair<float, float> windowSizeFraction;
    const char*             title;

    static WindowInitConfig default_() {
        return WindowInitConfig {
            .windowSizeFraction = {0.8f, 0.8f},
            .title =  "Complexity Lab Application"
        };
    }
};

struct GlfwWindow
{
    Proof<const GlfwInitialization> glfw;
    GLFWwindow*            windowHandle;

    struct WindowedGeometry { int x, y, width, height; };
    std::optional<WindowedGeometry> preFullscreen;

    std::vector<GlfwEvent> pendingEvents;


    GlfwWindow(Proof<const GlfwInitialization> glfw, WindowInitConfig initConfig);
    ~GlfwWindow();

    GlfwWindow(const GlfwWindow&)            = delete;
    GlfwWindow& operator=(const GlfwWindow&) = delete;
    // Non-movable: its address is registered with GLFW (window user pointer).
    GlfwWindow(GlfwWindow&&)                 = delete;
    GlfwWindow& operator=(GlfwWindow&&)      = delete;
};

std::vector<GlfwEvent> windowGetEvents(GlfwWindow& window);

bool windowShouldClose(const GlfwWindow& window);

void windowSetTitle(GlfwWindow& window, const char* title);
void windowSetCursorEnabled(GlfwWindow& window, bool enabled);
void windowSetResizeEnabled(GlfwWindow& window, bool enabled);
void windowSetFullscreen(GlfwWindow& window, bool fullscreen);
