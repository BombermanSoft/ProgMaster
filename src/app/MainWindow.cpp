#include "app/MainWindow.h"

#include <Windows.h>
#include <shellapi.h>

#include <filesystem>

#include "app/JuceHelpers.h"
#include "app/LocatePlaylistDialog.h"
#include "playlist/PlaylistLocator.h"

// Recurso embutido (ícone da janela). O uso é geral (setIcon não depende de OS).
#include <BinaryData.h>

namespace app {

MainWindow::ContentPane::ContentPane(juce::TabbedComponent& tabs,
                                     juce::Label& status)
    : m_tabs(tabs), m_status(status)
{
    addAndMakeVisible(m_tabs);
    addAndMakeVisible(m_status);
    m_status.setColour(juce::Label::textColourId, juce::Colours::grey);
}

void MainWindow::ContentPane::resized()
{
    const int statusH = 22;
    const auto area = getLocalBounds();
    m_tabs.setBounds(area.getX(), area.getY(), area.getWidth(),
                     area.getHeight() - statusH);
    m_status.setBounds(area.getX() + 6, area.getBottom() - statusH,
                       area.getWidth() - 12, statusH);
}

// ============================================================================

MainWindow::MainWindow(PlaylistLocator& locator)
    : juce::DocumentWindow("ProgMaster",
                           juce::Colour(0xff2b2b2b),
                           juce::DocumentWindow::minimiseButton |
                               juce::DocumentWindow::maximiseButton |
                               juce::DocumentWindow::closeButton,
                           true),
      m_locator(locator),
      m_controller(m_installation.playlistIniPath()),
      m_editorTab(m_controller, m_installation),
      m_codesTab(m_installation),
      m_configTab(m_controller),
      m_tabs(juce::TabbedButtonBar::TabsAtTop),
      m_contentPane(m_tabs, m_statusLabel)
{
    m_tabs.addTab("Editor", juce::Colour(0xff2b2b2b), &m_editorTab, false);
    m_tabs.addTab("Configuração", juce::Colour(0xff2b2b2b), &m_configTab, false);
    m_tabs.addTab("Códigos", juce::Colour(0xff2b2b2b), &m_codesTab, false);
    m_tabs.setCurrentTabIndex(TAB_EDITOR);
    m_tabs.setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colours::lightgrey);
    m_tabs.setColour(juce::TabbedButtonBar::frontTextColourId, juce::Colours::white);

    juce::DocumentWindow::setUsingNativeTitleBar(true);
    juce::DocumentWindow::setResizable(true, false);
    juce::DocumentWindow::setMenuBar(this);
    juce::DocumentWindow::setContentOwned(&m_contentPane, false);
    juce::DocumentWindow::centreWithSize(960, 660);

    // Ícone da janela embutido no executável (recurso BinárioDados/PNG).
    const auto windowIcon = juce::ImageCache::getFromMemory(
        BinaryData::ProgMasterIcon_png, BinaryData::ProgMasterIcon_pngSize);
    if (!windowIcon.isNull()) {
        juce::DocumentWindow::setIcon(windowIcon);
    }

    setStatus(L"Escolha o Playlist.exe no menu Arquivo ▸ Trocar localização.");

    setVisible(true);
    refreshFromLocator();
}

MainWindow::~MainWindow() = default;

void MainWindow::ensurePlaylistPath()
{
    if (!m_locator.hasValidInstallation()) {
        openLocateDialog();
    }
    refreshFromLocator();
}

void MainWindow::requestCloseWithConfirmation()
{
    const bool dirty = m_controller.isDirty() || m_editorTab.hasUnsavedChanges();
    if (!dirty) {
        finishQuit();
        return;
    }

    auto options = juce::MessageBoxOptions()
                       .withIconType(juce::MessageBoxIconType::WarningIcon)
                       .withTitle("Alterações não salvas")
                       .withMessage("Existem alterações não salvas.\n"
                                    "O que deseja fazer?")
                       .withButton("Salvar")
                       .withButton("Descartar")
                       .withButton("Cancelar")
                       .withAssociatedComponent(&m_contentPane);

    juce::AlertWindow::showAsync(options, [this](int result) {
        if (result == 0) {
            saveOrDiscardCurrent(true);
            finishQuit();
        } else if (result == 1) {
            saveOrDiscardCurrent(false);
            finishQuit();
        }
        // demais resultados (incl. cancelamento) -> nada.
    });
}

juce::StringArray MainWindow::getMenuBarNames()
{
    return { "Arquivo", "Editar" };
}

juce::PopupMenu MainWindow::getMenuForIndex(int topLevelMenuIndex,
                                            const juce::String& /*menuName*/)
{
    juce::PopupMenu menu;
    switch (topLevelMenuIndex) {
    case 0: // Arquivo
        menu.addItem(IDM_FILE_CONFIG_MGR, "Configuração de pastas");
        menu.addItem(IDM_FILE_LIST_IDS, "Lista de IDs (folders.xml)");
        menu.addItem(IDM_FILE_CHANGE_LOCATION, "Trocar localização...");
        menu.addSeparator();
        menu.addItem(IDM_FILE_SAVE, "Salvar");
        menu.addItem(IDM_FILE_DISCARD, "Descartar alterações");
        menu.addSeparator();
        menu.addItem(IDM_FILE_EXIT, "Sair");
        break;
    case 1: // Editar
        menu.addItem(IDM_EDIT_PROGRAMACAO, "Programação (playlist.ini)");
        menu.addItem(IDM_EDIT_MAPA_COMERCIAL, "Mapa Comercial (mapas\\Mapas.txt)");
        menu.addItem(IDM_EDIT_GRADES, "Grades Musicais (grades\\Grades.txt)");
        menu.addSeparator();
        menu.addItem(IDM_EDIT_RELOGIO_COMERCIAL, "Relógio Comercial");
        menu.addItem(IDM_EDIT_RELOGIO_MUSICAL, "Relógio Musical");
        break;
    default:
        break;
    }
    return menu;
}

void MainWindow::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
    if (menuItemID != IDM_FILE_CONFIG_MGR &&
        menuItemID != IDM_FILE_CHANGE_LOCATION &&
        menuItemID != IDM_FILE_LIST_IDS &&
        menuItemID != IDM_EDIT_PROGRAMACAO) {
        if (!m_locator.hasValidInstallation()) {
            openLocateDialog();
            refreshFromLocator();
            if (!m_locator.hasValidInstallation()) {
                return;
            }
        }
    }

    switch (menuItemID) {
    case IDM_FILE_CONFIG_MGR:
        openConfigManager();
        break;
    case IDM_FILE_LIST_IDS:
        showTab(TAB_CODIGOS);
        m_codesTab.reload();
        break;
    case IDM_FILE_CHANGE_LOCATION:
        openLocateDialog();
        refreshFromLocator();
        break;
    case IDM_FILE_SAVE:
        saveOrDiscardCurrent(true);
        break;
    case IDM_FILE_DISCARD:
        saveOrDiscardCurrent(false);
        break;
    case IDM_FILE_EXIT:
        requestCloseWithConfirmation();
        break;
    case IDM_EDIT_PROGRAMACAO:
        showTab(TAB_EDITOR);
        m_editorTab.openFile(EditorTab::FileKind::PlaylistIni);
        break;
    case IDM_EDIT_MAPA_COMERCIAL:
        showTab(TAB_EDITOR);
        m_editorTab.openFile(EditorTab::FileKind::MapasComercial);
        break;
    case IDM_EDIT_GRADES:
        showTab(TAB_EDITOR);
        m_editorTab.openFile(EditorTab::FileKind::GradesMusicais);
        break;
    case IDM_EDIT_RELOGIO_COMERCIAL:
        showTab(TAB_EDITOR);
        m_editorTab.openFile(EditorTab::FileKind::RelogioComercial);
        break;
    case IDM_EDIT_RELOGIO_MUSICAL:
        showTab(TAB_EDITOR);
        m_editorTab.openFile(EditorTab::FileKind::RelogioMusical);
        break;
    default:
        break;
    }
}

void MainWindow::closeButtonPressed()
{
    requestCloseWithConfirmation();
}

void MainWindow::refreshFromLocator()
{
    if (!m_locator.hasValidInstallation()) {
        setStatus(L"Escolha o Playlist.exe no menu Arquivo ▸ Trocar localização.");
        return;
    }

    const std::wstring exe = m_locator.getPlaylistExePath();
    m_installation.setExecutablePath(std::filesystem::path(exe));

    // As pastas de programação (Mapas/Grades/Relógios) ficam AO LADO do
    // Playlist.exe (ex.: C:\Playlist\pgm\Mapas), e não na raiz da instalação
    // (C:\Playlist). Por isso a pasta de busca dos arquivos é a pasta do exe.
    m_controller.setInstallationFolder(m_installation.executablePath().parent_path());
    m_controller.setPlaylistIniPath(m_installation.playlistIniPath());

    m_controller.load();
    m_configTab.refreshFromController();
    m_codesTab.reload();

    std::wstring status = L"Playlist.exe: " + exe;
    if (!m_controller.path().empty()) {
        status += L"   |   playlist.ini: " + m_controller.path().wstring();
    }
    if (!m_controller.lastLoadMessage().empty()) {
        status += L"   |   " + m_controller.lastLoadMessage();
    }
    setStatus(status);
}

void MainWindow::openLocateDialog()
{
    std::wstring chosen;
    bool confirmed = false;
    locatePlaylistExe(*this, m_locator.getPlaylistExePath(), chosen, confirmed);
    if (!confirmed) {
        return;
    }

    std::wstring message;
    if (!m_locator.validatePath(chosen, message)) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Localização do Playlist",
            jstr(message));
        return;
    }

    m_locator.setPlaylistExePath(chosen, message);
}

void MainWindow::openConfigManager()
{
    const std::filesystem::path mgr = m_installation.configManagerPath();
    if (mgr.empty() || !std::filesystem::exists(mgr)) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Configuração de pastas",
            "O ConfigManager.exe não foi encontrado junto à instalação.");
        return;
    }
    ShellExecuteW(nullptr, L"open", mgr.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::showTab(int index)
{
    m_tabs.setCurrentTabIndex(index);
}

void MainWindow::saveOrDiscardCurrent(bool save)
{
    switch (m_tabs.getCurrentTabIndex()) {
    case TAB_EDITOR:
        if (save) {
            m_editorTab.saveCurrentFile();
        } else {
            m_editorTab.reopenFromDisk();
        }
        break;
    case TAB_CONFIGURACAO:
        if (save) {
            m_configTab.savePlaylistIni();
        } else {
            m_configTab.discardChanges();
        }
        break;
    default:
        // Guia Códigos: somente leitura — nada a salvar/descartar.
        break;
    }
}

void MainWindow::finishQuit()
{
    juce::JUCEApplication::getInstance()->quit();
}

void MainWindow::setStatus(const std::wstring& text)
{
    m_statusLabel.setText(jstr(text), juce::dontSendNotification);
}

} // namespace app