#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "Timer.h"

#include <memory>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class FileDirectoryWatcher;
class GfxResources;
class Renderer;
class RendererInput;
class Window;

class Engine
{
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void init();
    void run();

private:
    RendererInput getRendererInput();

    std::unique_ptr<GfxResources> m_gfxResources;

    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Window> m_window;

    std::unique_ptr<FileDirectoryWatcher> m_shaderDirWatcher;
    std::unique_ptr<FileDirectoryWatcher> m_imageDirWatcher;

    Timer m_timer;
    uint32_t m_frameIndex = 0;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_ENGINE_H
