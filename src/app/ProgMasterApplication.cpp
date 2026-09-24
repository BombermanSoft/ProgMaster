#include "app/ProgMasterApplication.h"

#include "app/MainWindow.h"
#include "core/Log.h"
#include "core/Settings.h"
#include "playlist/PlaylistLocator.h"

void ProgMasterApplication::initialise(const juce::String& /*commandLine*/)
{
    Log::init();

    m_settings = std::make_unique<Settings>();
    m_locator = std::make_unique<PlaylistLocator>(*m_settings);
    m_locator->tryLoadSavedPath();

    m_mainWindow = std::make_unique<app::MainWindow>(*m_locator);
    m_mainWindow->ensurePlaylistPath();
}

void ProgMasterApplication::shutdown()
{
    m_mainWindow.reset();
    m_locator.reset();
    m_settings.reset();
}

void ProgMasterApplication::systemRequestedQuit()
{
    // A janela trata as alterações não salvas; só sai quando confirmado.
    if (m_mainWindow != nullptr) {
        m_mainWindow->requestCloseWithConfirmation();
    } else {
        quit();
    }
}

START_JUCE_APPLICATION(ProgMasterApplication)