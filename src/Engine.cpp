// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include "Engine.h"

#include "GfxResources.h"
#include "Renderer.h"
#include "Window.h"
#include "Utils.h"
#include "Timer.h"

#include "FileDirectoryWatcher.h"
#include "ResourceList.h"

#include <iostream>
#include <memory>
#include <ctime>
#include <chrono>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

Engine::Engine()
{

}

Engine::~Engine()
{

}

void Engine::init()
{
    GlobalVariables& gv = GlobalVariables::getInstance();
    gv.applicationName = "VulkanToy";
    gv.engineName = "ToyEngine";

    m_window = std::unique_ptr<Window>(new Window(gv.windowWidth, gv.windowHeight, gv.applicationName));

    m_gfxResources = std::unique_ptr<GfxResources>(new GfxResources(m_window.get()));
    m_renderer = std::unique_ptr<Renderer>(new Renderer(
        m_gfxResources.get(), m_window.get()));

    m_imageDirWatcher.reset(new FileDirectoryWatcher(
        ResourceList::getInstance().imagePath,
        ResourceList::getInstance().imageFilesForSearch,
        true));
    m_shaderDirWatcher.reset(new FileDirectoryWatcher(
        ResourceList::getInstance().shaderPath,
        ResourceList::getInstance().shaderFiles,
        false));
}

void Engine::run()
{
    while (!m_window->shouldClose())
    {
        m_window->update();
        m_timer.update();

        RendererInput rendererInput = std::move(getRendererInput());

        if (m_timer.isFpsUpdated())
        {
            m_window->updateWindowText("    " + std::to_string(round(m_timer.timeSeconds))
                + "    " + std::to_string(m_timer.fps) + " fps");
        }

        if (m_window->isResized())
        {
            m_gfxResources->resizeWindow();
            m_renderer->resizeFramebuffer();
            m_window->resizeHandled();
        }
        if (m_imageDirWatcher->checkForChanges())
        {
            const std::vector<std::string> files =
                std::move(m_imageDirWatcher->getChangedFilesAndReset());
            if (files.size() > 0)
            {
                m_renderer->updateImages(files);
            }
        }
        if (m_shaderDirWatcher->checkForChanges())
        {
            const std::vector<std::string> files =
                std::move(m_shaderDirWatcher->getChangedFilesAndReset());
            if (files.size() > 0)
            {
                m_renderer->updateShaders(files);
            }
        }

        m_renderer->render(rendererInput);
        m_frameIndex++;
    }
}

RendererInput Engine::getRendererInput()
{
    RendererInput rendererInput{};
    rendererInput.globalTime    = m_timer.timeSeconds;
    rendererInput.deltaTime     = m_timer.deltaTimeSeconds;
    rendererInput.frameIndex    = m_frameIndex;
    rendererInput.mousePos      = m_window->getMousePos();
    rendererInput.date[0]       = m_timer.year;
    rendererInput.date[1]       = m_timer.month;
    rendererInput.date[2]       = m_timer.day;
    rendererInput.date[3]       = m_timer.secs;

    return rendererInput;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
