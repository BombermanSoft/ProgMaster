#include "core/Application.h"

#include <memory>

#include "core/Log.h"
#include "core/Settings.h"
#include "playlist/PlaylistLocator.h"
#include "ui/MainWindow.h"

// Pimpl: esconde os tipos internos do cabeçalho e centraliza a composição.
struct Application::Impl {
    Settings settings;                    // persistência (caminho salvo)
    PlaylistLocator locator{ settings };  // localização/validação do Playlist.exe
    MainWindow mainWindow;                // janela principal
};

Application::Application() = default;

Application::~Application() = default;

int Application::run(HINSTANCE hInstance, int nCmdShow)
{
    Log::init();
    m_impl = std::make_unique<Impl>();

    // Recupera o caminho salvo em execuções anteriores (se ainda existir).
    m_impl->locator.tryLoadSavedPath();

    if (!m_impl->mainWindow.create(hInstance, nCmdShow)) {
        Log::error(L"Não foi possível criar a janela principal.");
        return 1;
    }

    m_impl->mainWindow.setContext(&m_impl->settings, &m_impl->locator);
    m_impl->mainWindow.ensurePlaylistPath();
    m_impl->mainWindow.runMessageLoop();
    return 0;
}