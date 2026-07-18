#include "Platform.hpp"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

namespace {
bool s_glfwAlive = false;

void glfwErrorCallback(int code, const char* description) {
    spdlog::error("GLFW error {:#x}: {}", code, description);
}
}

GlfwInitialization::GlfwInitialization() {
    if (s_glfwAlive) {
        throw std::logic_error(
            "GlfwInitialization: one is already alive - GLFW's global "
            "init/terminate lifecycle admits a single owner at a time");
    }
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        throw std::runtime_error(
            "glfwInit failed (see logged GLFW error) - no usable display?");
    }
    s_glfwAlive = true;
}

GlfwInitialization::~GlfwInitialization() {
    glfwTerminate();
    s_glfwAlive = false;
}

std::pair<int, int> getPrimaryMonitorResolution(Proof<const GlfwInitialization>) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (!mode) {
        throw std::runtime_error("Cannot query GLFW for primary monitor resolution");
    }
    return {mode->width, mode->height};
}

namespace {
std::pair<int, int> getVirtualScreenSize_AI(Proof<const GlfwInitialization>) {
    Display* display_AI = glfwGetX11Display();
    const int screen_AI = DefaultScreen(display_AI);
    return {DisplayWidth(display_AI, screen_AI), DisplayHeight(display_AI, screen_AI)};
}

GlfwWindow* windowOf(GLFWwindow* w) {
    return static_cast<GlfwWindow*>(glfwGetWindowUserPointer(w));
}

void cbKey(GLFWwindow* w, int key, int scancode, int action, int mods) {
    windowOf(w)->pendingEvents.push_back(GlfwKeyEvent{key, scancode, action, mods});
}
void cbMouseButton(GLFWwindow* w, int button, int action, int mods) {
    windowOf(w)->pendingEvents.push_back(GlfwMouseButtonEvent{button, action, mods});
}
void cbCursorPos(GLFWwindow* w, double x, double y) {
    windowOf(w)->pendingEvents.push_back(GlfwMouseMoveEvent{x, y});
}
void cbScroll(GLFWwindow* w, double dx, double dy) {
    windowOf(w)->pendingEvents.push_back(GlfwScrollEvent{dx, dy});
}
void cbFramebufferSize(GLFWwindow* w, int width, int height) {
    windowOf(w)->pendingEvents.push_back(GlfwFramebufferResizeEvent{width, height});
}
}

GlfwWindow::GlfwWindow(Proof<const GlfwInitialization> glfw, WindowInitConfig initConfig)  // _AI
    : glfw(std::move(glfw)) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    const std::pair<int, int> screenPx_AI = getVirtualScreenSize_AI(glfw);
    const int width  = static_cast<int>(initConfig.windowSizeFraction.first * screenPx_AI.first);
    const int height = static_cast<int>(initConfig.windowSizeFraction.second * screenPx_AI.second);
    windowHandle = glfwCreateWindow(width, height, initConfig.title, nullptr, nullptr);
    if (!windowHandle) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowPos(windowHandle,
                     (screenPx_AI.first - width) / 2,
                     (screenPx_AI.second - height) / 2);

    glfwSetWindowUserPointer(windowHandle, this);
    glfwSetKeyCallback(windowHandle, cbKey);
    glfwSetMouseButtonCallback(windowHandle, cbMouseButton);
    glfwSetCursorPosCallback(windowHandle, cbCursorPos);
    glfwSetScrollCallback(windowHandle, cbScroll);
    glfwSetFramebufferSizeCallback(windowHandle, cbFramebufferSize);

    glfwShowWindow(windowHandle);
}

GlfwWindow::~GlfwWindow() {
    if (windowHandle) {
        glfwDestroyWindow(windowHandle);
    }
}

bool windowShouldClose(const GlfwWindow& window) {
    return glfwWindowShouldClose(window.windowHandle);
}

void windowSetTitle(GlfwWindow& window, const char* title) {
    glfwSetWindowTitle(window.windowHandle, title);
}

void windowSetCursorEnabled(GlfwWindow& window, bool enabled) {
    glfwSetInputMode(window.windowHandle, GLFW_CURSOR,
                     enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

void windowSetResizeEnabled(GlfwWindow& window, bool enabled) {
    glfwSetWindowAttrib(window.windowHandle, GLFW_RESIZABLE,
                        enabled ? GLFW_TRUE : GLFW_FALSE);
}

void windowSetFullscreen(GlfwWindow& window, bool fullscreen) {
    if (fullscreen == window.preFullscreen.has_value()) return;  // already in mode

    if (!fullscreen) {
        glfwSetWindowMonitor(window.windowHandle, nullptr,
                             window.preFullscreen->x, window.preFullscreen->y,
                             window.preFullscreen->width, window.preFullscreen->height, 0);
        window.preFullscreen.reset();
        return;
    }

    GlfwWindow::WindowedGeometry restore;
    glfwGetWindowPos(window.windowHandle, &restore.x, &restore.y);
    glfwGetWindowSize(window.windowHandle, &restore.width, &restore.height);
    window.preFullscreen = restore;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(window.windowHandle, monitor, 0, 0,
                         mode->width, mode->height, mode->refreshRate);
}

std::vector<GlfwEvent> windowGetEvents(GlfwWindow& window)
{
    glfwPollEvents();
    std::vector<GlfwEvent> newPendingEvents {};
    newPendingEvents.reserve(window.pendingEvents.size());
    return std::exchange(window.pendingEvents, newPendingEvents);
}