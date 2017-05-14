#ifndef CORE_WINDOW_H
#define CORE_WINDOW_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include <cstdint>
#include <string>
#include <tuple>
#include <windows.h>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

struct MousePos
{
    bool leftButtonDown = false;

    uint32_t leftPosX = 0;
    uint32_t leftPosY = 0;
    uint32_t clickLeft = 0;
};

class Window
{
public:
    Window(const uint32_t width, const uint32_t height, const std::string& name);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void update();
    bool shouldClose() const;

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    MousePos getMousePos() const;

    HWND getHwnd() const;
    HINSTANCE getHinstance() const;

    bool isResized() const;
    void resizeHandled();

    void updateWindowText(const std::string& text);

private:
    void checkForResize();
    void updateMousePos(const uint32_t message);

    HWND m_hwnd;
    HINSTANCE m_hinstance;

    uint32_t m_width    = 0;
    uint32_t m_height   = 0;

    MousePos m_mousePos{};

    bool m_resized = false;

    std::string m_name;
    bool m_closeWindow = false;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_WINDOW_H
