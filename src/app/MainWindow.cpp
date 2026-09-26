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
    // Fonte menor para o status caber na barra mesmo com caminhos longos
    // (antes, caminho grande cortava a informação do playlist.ini na tela).
    m_status.setFont(juce::Font(juce::FontOptions(12.0f)));
    m_status.setJustificationType(juce::Justification::left);
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
      m_relogioTab(m_controller, m_installation),
      m_tabs(juce::TabbedButtonBar::TabsAtTop),
      m_contentPane(m_tabs, m_statusLabel)
{
    m_tabs.addTab("Editor", juce::Colour(0xff2b2b2b), &m_editorTab, false);
    m_tabs.addTab(L"Configuração", juce::Colour(0xff2b2b2b), &m_configTab, false);
    m_tabs.addTab(L"Códigos", juce::Colour(0xff2b2b2b), &m_codesTab, false);
    m_tabs.addTab(L"Relógio", juce::Colour(0xff2b2b2b), &m_relogioTab, false);
    m_tabs.addTab(L"Blocos", juce::Colour(0xff2b2b2b), &m_blocosTab, false);
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
    const bool dirty = m_controller.isDirty() || m_editorTab.hasUnsavedChanges() ||
                       m_relogioTab.hasUnsavedChanges();
    if (!dirty) {
        finishQuit();
        return;
    }

    juce::AlertWindow window(L"Alterações não salvas",
                             L"Existem alterações não salvas.\nO que deseja fazer?",
                             juce::MessageBoxIconType::WarningIcon,
                             &m_contentPane);
    window.addButton(L"Salvar", 1);
    window.addButton(L"Descartar alterações", 2);
    window.addButton(L"Cancelar", 0);
    window.enterModalState(false);

    switch (window.runModalLoop()) {
    case 1: // Salvar alterações e fechar.
        saveOrDiscardCurrent(true);
        finishQuit();
        break;
    case 2: // Descartar alterações: fechar sem salvar.
        finishQuit();
        break;
    case 0: // Cancelar (ou ESC/fechar a janela): cancela o fechamento.
    default:
        break;
    }
}

juce::StringArray MainWindow::getMenuBarNames()
{
    return { "Arquivo", "Editar", L"Avançado" };
}

juce::PopupMenu MainWindow::getMenuForIndex(int topLevelMenuIndex,
                                            const juce::String& /*menuName*/)
{
    juce::PopupMenu menu;
    switch (topLevelMenuIndex) {
    case 0: // Arquivo
        menu.addItem(IDM_FILE_CONFIG_MGR, L"Configuração de pastas");
        menu.addItem(IDM_FILE_LIST_IDS, "Lista de IDs");
        menu.addItem(IDM_FILE_CHANGE_LOCATION, L"Trocar localização...");
        menu.addSeparator();
        menu.addItem(IDM_FILE_SAVE, "Salvar");
        menu.addItem(IDM_FILE_DISCARD, L"Descartar alterações");
        menu.addSeparator();
        menu.addItem(IDM_FILE_EXIT, "Sair");
        break;
    case 1: // Editar (Etapa 3 — visual)
        menu.addItem(IDM_EDIT_PROGRAMACAO, L"Programação");
        menu.addItem(IDM_EDIT_BLOCOS_MUSICAIS, L"Blocos Musicais");
        menu.addItem(IDM_EDIT_BLOCOS_COMERCIAL, L"Blocos Comerciais");
        menu.addSeparator();
        menu.addItem(IDM_EDIT_RELOGIO_MUSICAL, L"Relógio Musical");
        menu.addItem(IDM_EDIT_RELOGIO_COMERCIAL, L"Relógio Comercial");
        break;
    case 2: // Avançado (textual, antigo "Editar")
        menu.addItem(IDM_ADV_PROGRAMACAO, L"Programação");
        menu.addItem(IDM_ADV_MAPA_COMERCIAL, "Mapa Comercial");
        menu.addItem(IDM_ADV_GRADES, "Grades Musicais");
        menu.addSeparator();
        menu.addItem(IDM_ADV_RELOGIO_COMERCIAL, L"Relógio Comercial");
        menu.addItem(IDM_ADV_RELOGIO_MUSICAL, L"Relógio Musical");
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
        menuItemID != IDM_EDIT_PROGRAMACAO &&
        menuItemID != IDM_EDIT_BLOCOS_MUSICAIS &&
        menuItemID != IDM_EDIT_BLOCOS_COMERCIAL &&
        menuItemID != IDM_ADV_PROGRAMACAO) {
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
    // ---- Menu "Editar" (Etapa 3 — visual) ----
    case IDM_EDIT_PROGRAMACAO:
        showTab(TAB_CONFIGURACAO);
        break;
    case IDM_EDIT_BLOCOS_MUSICAIS:
        showTab(TAB_BLOCOS);
        m_blocosTab.setMusical(true);
        break;
    case IDM_EDIT_BLOCOS_COMERCIAL:
        showTab(TAB_BLOCOS);
        m_blocosTab.setMusical(false);
        break;
    case IDM_EDIT_RELOGIO_MUSICAL:
        showTab(TAB_RELOGIO);
        m_relogioTab.openRelogio(readconf::ConfigScope::RelogioMusical);
        break;
    case IDM_EDIT_RELOGIO_COMERCIAL:
        showTab(TAB_RELOGIO);
        m_relogioTab.openRelogio(readconf::ConfigScope::RelogioComercial);
        break;
    // ---- Menu "Avançado" (textual, antigo "Editar") ----
    case IDM_ADV_PROGRAMACAO:
        showTab(TAB_EDITOR);
        m_editorTab.openFile(EditorTab::FileKind::PlaylistIni);
        break;
    case IDM_ADV_MAPA_COMERCIAL:
        showTab(TAB_EDITOR);
        m_editorTab.openFile(EditorTab::FileKind::MapasComercial);
        break;
    case IDM_ADV_GRADES:
        showTab(TAB_EDITOR);
        m_editorTab.openFile(EditorTab::FileKind::GradesMusicais);
        break;
    case IDM_ADV_RELOGIO_COMERCIAL:
        showTab(TAB_EDITOR);
        m_editorTab.openFile(EditorTab::FileKind::RelogioComercial);
        break;
    case IDM_ADV_RELOGIO_MUSICAL:
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

    // O editor começa sem nenhuma sub-aba: ao iniciar (ou ao trocar de
    // localização), abre logo o playlist.ini para o usuário ver o conteúdo
    // já preenchido no bloco de notas — em vez de uma tela vazia e a mensagem
    // "arquivo não encontrado". Se já havia abas abertas, reabre o conjunto
    // corrente com os caminhos da NOVA localização (senão as abas continuariam
    // presas à pasta antiga).
    if (!m_editorTab.hasOpenPages()) {
        m_editorTab.openFile(EditorTab::FileKind::PlaylistIni);
    } else {
        m_editorTab.openFile(m_editorTab.currentKind());
    }

    // O tab Relógio (editor visual/textual) também reabre seus arquivos com
    // os caminhos da NOVA localização.
    m_relogioTab.refreshFromController();

    // A barra de status global não mostra mais caminhos fixos — o caminho do
    // arquivo em edição fica no rodapé do próprio editor.
    if (!m_controller.lastLoadMessage().empty()) {
        setStatus(m_controller.lastLoadMessage());
    } else {
        setStatus(std::wstring());
    }
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
            L"Localização do Playlist",
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
            L"Configuração de pastas",
            L"O ConfigManager.exe não foi encontrado junto à instalação.");
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
    case TAB_RELOGIO:
        if (save) {
            m_relogioTab.saveCurrentFile();
        } else {
            m_relogioTab.reopenFromDisk();
        }
        break;
    default:
        // Guia Códigos e Blocos: somente leitura — nada a salvar/descartar.
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