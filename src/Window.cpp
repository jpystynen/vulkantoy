// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "Window.h"

#include <string>
#include <assert.h>
#include <tuple>
#include <algorithm>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

static LRESULT windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    } break;
    default:break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

Window::Window(const uint32_t width, const uint32_t height, const std::string& name)
    : m_width(width), m_height(height), m_name(name)
{
    m_hinstance = GetModuleHandle(nullptr);

    WNDCLASSEX wcex;
    ZeroMemory(&wcex, sizeof(WNDCLASSEX));

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = windowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = m_hinstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = m_name.c_str();
    //wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    const auto res = RegisterClassEx(&wcex);
    assert(res > 0);

    DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    RECT winRect = { 0, 0, LONG(width), LONG(height) };

    const BOOL success = AdjustWindowRectEx(&winRect, dwStyle, FALSE, dwExStyle);
    assert(success);

    m_hwnd = CreateWindowEx(
        0,
        m_name.c_str(),
        m_name.c_str(),
        dwStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        winRect.right - winRect.left,
        winRect.bottom - winRect.top,
        nullptr,
        nullptr,
        m_hinstance,
        nullptr);
    assert(m_hwnd);

    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
}

Window::~Window()
{
    DestroyWindow(m_hwnd);
    UnregisterClass(m_name.c_str(), m_hinstance);
}

void Window::update()
{
    MSG msg;
    if (PeekMessage(&msg, m_hwnd, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_QUIT || msg.message == WM_CLOSE)
        {
            m_closeWindow = true;
        }

        updateMousePos(msg.message);
        checkForResize();
    }
}

bool Window::shouldClose() const
{
    return m_closeWindow;
}

uint32_t Window::getWidth() const
{
    return m_width;
}

uint32_t Window::getHeight() const
{
    return m_height;
}

MousePos Window::getMousePos() const
{
    return m_mousePos;
}

void Window::updateMousePos(const uint32_t message)
{
    if (message == WM_LBUTTONDOWN)
        m_mousePos.leftButtonDown = true;

    if (message == WM_LBUTTONUP)
        m_mousePos.leftButtonDown = false;
    
    if (m_mousePos.leftButtonDown)
    {
        POINT point;
        if (GetCursorPos(&point))
        {
            if (ScreenToClient(m_hwnd, &point))
            {
                m_mousePos.leftPosX = std::min((uint32_t)std::max(0l, point.x), m_width);
                m_mousePos.leftPosY = std::min((uint32_t)std::max(0l, point.y), m_height);
                m_mousePos.clickLeft = 1;
            }
        }
    }

    if (!m_mousePos.leftButtonDown)
    {
        m_mousePos = {};
    }
}

void Window::checkForResize()
{
    RECT rect;
    const BOOL res = GetClientRect(m_hwnd, &rect);
    if (res != 0)
    {
        const uint32_t width = rect.right - rect.left;
        const uint32_t height = rect.bottom - rect.top;
        if (m_width != width || m_height != height)
        {
            m_resized = true;
            m_width = width;
            m_height = height;
        }
    }
}

bool Window::isResized() const
{
    return m_resized;
}

void Window::resizeHandled()
{
    m_resized = false;
}

void Window::updateWindowText(const std::string& text)
{
    const std::string fullText = m_name + "  " + std::to_string(m_width)
        + "x" + std::to_string(m_height) + text;
    const BOOL res = SetWindowText(m_hwnd, fullText.c_str());
}

HWND Window::getHwnd() const
{
    return m_hwnd;
}

HINSTANCE Window::getHinstance() const
{
    return m_hinstance;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
