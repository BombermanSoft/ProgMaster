#include "ui/MainWindow.h"

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <string>

#include "core/Log.h"
#include "core/Settings.h"
#include "models/FolderEntry.h"
#include "playlist/FoldersXml.h"
#include "playlist/PlaylistLocator.h"
#include "resource.h"
#include "ui/CodesPageView.h"
#include "ui/IniEditorView.h"
#include "ui/LocatePlaylistDialog.h"

namespace {

constexpr wchar_t kMainWindowClass[] = L"ProgMasterMainWindow";
constexpr wchar_t kMainWindowTitle[] = L"ProgMaster";

// Converte um ID numérico de controle em HMENU (usado em CreateWindowExW).
// O INT_PTR evita o aviso C4312 em builds x64.
HMENU menuFromId(int id)
{
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

} // namespace

MainWindow::MainWindow() = default;

MainWindow::~MainWindow()
{
    if (m_hUiFont) {
        DeleteObject(m_hUiFont);
    }
    if (m_hMonoFont) {
        DeleteObject(m_hMonoFont);
    }
}

bool MainWindow::create(HINSTANCE hInstance, int nCmdShow)
{
    m_hInstance = hInstance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &MainWindow::wndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kMainWindowClass;
    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(0, kMainWindowClass, kMainWindowTitle,
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 920, 620,
                             nullptr, nullptr, hInstance, this);
    if (!m_hWnd) {
        Log::error(L"Falha ao criar a janela principal.");
        return false;
    }

    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    // Dispara o layout explicitamente, independentemente da ordem de entrega
    // do WM_SIZE durante a criação (garante que nenhum controle fique em
    // posição padrão sobreposto a outro).
    doLayout();
    return true;
}

void MainWindow::setContext(Settings* settings, PlaylistLocator* locator)
{
    m_settings = settings;
    m_locator = locator;
}

void MainWindow::runMessageLoop()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void MainWindow::ensurePlaylistPath()
{
    if (!m_locator->hasValidInstallation()) {
        LocatePlaylistDialog::show(m_hWnd, m_hInstance, *m_locator);
    }
    refreshFromLocator();
}

LRESULT CALLBACK MainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        self = reinterpret_cast<MainWindow*>(
            reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) {
        return self->handleMessage(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT MainWindow::handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    m_hWnd = hwnd;

    switch (msg) {
    case WM_CREATE:
        createControls();
        return 0;

    case WM_COMMAND:
        onCommand(LOWORD(wp), HIWORD(wp));
        return 0;

    case WM_NOTIFY: {
        const NMHDR* hdr = reinterpret_cast<const NMHDR*>(lp);
        if (hdr && hdr->idFrom == IDC_TAB && hdr->code == TCN_SELCHANGE) {
            m_activeTab = static_cast<int>(SendMessageW(m_hTab, TCM_GETCURSEL, 0, 0));
            applyCurrentTab();
        }
        return 0;
    }

    case WM_INITMENUPOPUP: {
        // Habilita/desabilita "Salvar" e "Descartar alterações" conforme o
        // estado atual do editor de texto.
        if (reinterpret_cast<HMENU>(wp) == m_hFileMenu) {
            const bool canSave = m_iniView.hasFile();
            const bool canDiscard = m_iniView.hasFile() &&
                                    m_iniView.hasUnsavedChanges();
            EnableMenuItem(m_hFileMenu, IDM_FILE_SAVE,
                           canSave ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(m_hFileMenu, IDM_FILE_DISCARD,
                           canDiscard ? MF_ENABLED : MF_GRAYED);
        }
        return 0;
    }

    case WM_SIZE:
        doLayout();
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        if (mmi) {
            mmi->ptMinTrackSize.x = 720;
            mmi->ptMinTrackSize.y = 500;
        }
        return 0;
    }

    case WM_CLOSE:
        if (confirmCloseIfDirty()) {
            DestroyWindow(m_hWnd);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void MainWindow::createMenu()
{
    m_hFileMenu = CreatePopupMenu();
    AppendMenuW(m_hFileMenu, MF_STRING, IDM_FILE_CONFIG_MGR, L"Configuração de pastas");
    AppendMenuW(m_hFileMenu, MF_STRING, IDM_FILE_LIST_IDS, L"Lista de IDs");
    AppendMenuW(m_hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hFileMenu, MF_STRING, IDM_FILE_CHANGE_LOCATION, L"Trocar localização");
    AppendMenuW(m_hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hFileMenu, MF_STRING, IDM_FILE_SAVE, L"Salvar");
    AppendMenuW(m_hFileMenu, MF_STRING, IDM_FILE_DISCARD, L"Descartar alterações");
    AppendMenuW(m_hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hFileMenu, MF_STRING, IDM_FILE_EXIT, L"Sair");

    m_hEditMenu = CreatePopupMenu();
    AppendMenuW(m_hEditMenu, MF_STRING, IDM_EDIT_PROGRAMACAO, L"Programação");
    AppendMenuW(m_hEditMenu, MF_STRING, IDM_EDIT_MAPA_COMERCIAL, L"Mapa Comercial");
    AppendMenuW(m_hEditMenu, MF_STRING, IDM_EDIT_GRADES, L"GradesMusicais");

    m_hMenu = CreateMenu();
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(m_hFileMenu), L"Arquivo");
    AppendMenuW(m_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(m_hEditMenu), L"Editar");
    SetMenu(m_hWnd, m_hMenu);
}

void MainWindow::createControls()
{
    m_hUiFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hMonoFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");

    createMenu();

    m_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
                             WS_CHILD | WS_VISIBLE | TCS_FOCUSNEVER,
                             0, 0, 100, 100, m_hWnd,
                             menuFromId(IDC_TAB),
                             m_hInstance, nullptr);
    m_hStatusStatic = CreateWindowExW(0, L"STATIC", L"",
                                      WS_CHILD | WS_VISIBLE | SS_SUNKEN,
                                      0, 0, 100, 16, m_hWnd,
                                      menuFromId(IDC_STATUS_STATIC),
                                      m_hInstance, nullptr);

    SendMessageW(m_hTab, WM_SETFONT, reinterpret_cast<WPARAM>(m_hUiFont), TRUE);
    SendMessageW(m_hStatusStatic, WM_SETFONT, reinterpret_cast<WPARAM>(m_hUiFont), TRUE);

    // Guias: "Editor" (Bloco de Notas interno) e "Códigos".
    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    wchar_t editorTabTitle[] = L"Editor";
    wchar_t codesTabTitle[] = L"Códigos";
    ti.pszText = editorTabTitle;
    SendMessageW(m_hTab, TCM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&ti));
    ti.pszText = codesTabTitle;
    SendMessageW(m_hTab, TCM_INSERTITEMW, 1, reinterpret_cast<LPARAM>(&ti));

    m_iniView.create(m_hWnd, m_hInstance, m_hUiFont, m_hMonoFont);
    m_codesView.create(m_hWnd, m_hInstance, m_hUiFont);

    applyCurrentTab();
}

void MainWindow::onCommand(int id, int notifyCode)
{
    // Comandos de menu usam IDs na faixa 4001+; os controles filhos usam IDs
    // abaixo disso (definidos em IniEditorView/CodesPageView).
    if (id >= IDM_FILE_CONFIG_MGR) {
        onMenuCommand(id);
        return;
    }

    switch (id) {
    case IniEditorView::IDC_INI_SAVE:
        if (notifyCode == BN_CLICKED) {
            saveCurrentFile();
        }
        break;
    case IniEditorView::IDC_INI_UNDO:
        if (notifyCode == BN_CLICKED) {
            m_iniView.onUndo();
        }
        break;
    case IniEditorView::IDC_INI_REDO:
        if (notifyCode == BN_CLICKED) {
            m_iniView.onRedo();
        }
        break;
    case IniEditorView::IDC_INI_EDIT:
        if (notifyCode == EN_CHANGE) {
            m_iniView.onContentEdited();
        }
        break;
    default:
        break;
    }
}

void MainWindow::onMenuCommand(int id)
{
    switch (id) {
    case IDM_FILE_CONFIG_MGR:
        openConfigManager();
        break;
    case IDM_FILE_LIST_IDS:
        showCodesTab();
        break;
    case IDM_FILE_CHANGE_LOCATION:
        openLocateDialog();
        break;
    case IDM_FILE_SAVE:
        saveCurrentFile();
        break;
    case IDM_FILE_DISCARD:
        discardCurrentFile();
        break;
    case IDM_FILE_EXIT:
        PostMessageW(m_hWnd, WM_CLOSE, 0, 0);
        break;
    case IDM_EDIT_PROGRAMACAO:
        onEditFile(EditorFile::PlaylistIni);
        break;
    case IDM_EDIT_MAPA_COMERCIAL:
        onEditFile(EditorFile::MapasComercial);
        break;
    case IDM_EDIT_GRADES:
        onEditFile(EditorFile::GradesMusicais);
        break;
    default:
        break;
    }
}

void MainWindow::doLayout()
{
    if (!m_hTab) {
        return;
    }

    RECT rc{};
    GetClientRect(m_hWnd, &rc);

    const int margin = 10;
    const int statusH = 20;

    // A faixa do menu já fica fora da área do cliente; a guia ocupa todo o
    // resto da janela, com a barra de status na base.
    const int tabY = margin;
    const int tabH = rc.bottom - margin - statusH - 4 - tabY;
    if (tabH > 0) {
        MoveWindow(m_hTab, margin, tabY, rc.right - 2 * margin, tabH, TRUE);
    }
    MoveWindow(m_hStatusStatic, margin, rc.bottom - margin - statusH,
               rc.right - 2 * margin, statusH, TRUE);

    // Área útil da guia (descontando a faixa das abas).
    RECT page{};
    GetClientRect(m_hTab, &page);
    // wParam=FALSE: converte o retângulo cheio do tab para a área de página
    // (abaixo da faixa de abas). TRUE expandiria, causando sobreposição.
    SendMessageW(m_hTab, TCM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&page));
    MapWindowPoints(m_hTab, m_hWnd, reinterpret_cast<POINT*>(&page), 2);

    m_iniView.layout(page);
    m_codesView.layout(page);

    logChildRects();
}

void MainWindow::applyCurrentTab()
{
    const bool showEditor = (m_activeTab == 0);
    m_iniView.setVisible(showEditor);
    m_codesView.setVisible(!showEditor);
}

void MainWindow::showEditorTab()
{
    m_activeTab = 0;
    SendMessageW(m_hTab, TCM_SETCURSEL, 0, 0);
    applyCurrentTab();
}

void MainWindow::showCodesTab()
{
    m_activeTab = 1;
    SendMessageW(m_hTab, TCM_SETCURSEL, 1, 0);
    applyCurrentTab();
}

void MainWindow::setStatusText(const std::wstring& text)
{
    if (m_hStatusStatic) {
        SetWindowTextW(m_hStatusStatic, text.c_str());
    }
}

void MainWindow::refreshFromLocator()
{
    if (!m_locator || !m_locator->hasValidInstallation()) {
        m_iniView.clear();
        m_codesView.clear();
        setStatusText(L"Aguardando a localização do Playlist.exe.");
        return;
    }

    m_installation.setExecutablePath(m_locator->getPlaylistExePath());
    Log::info(L"Instalação do Playlist reconhecida em: " +
              m_installation.installFolder().wstring());

    loadFileIntoEditor(EditorFile::PlaylistIni);
    reloadCodes();
    showEditorTab();
    setStatusText(L"Instalação do Playlist reconhecida.");
}

void MainWindow::openLocateDialog()
{
    if (!confirmIfDirtyBeforeSwitch()) {
        return;
    }

    if (LocatePlaylistDialog::show(m_hWnd, m_hInstance, *m_locator)) {
        refreshFromLocator();
    }
}

void MainWindow::onEditFile(EditorFile file)
{
    if (!m_locator || !m_locator->hasValidInstallation()) {
        setStatusText(L"Nenhuma instalação do Playlist configurada.");
        return;
    }

    // Mesmo arquivo já em edição: apenas garante que a guia Editor está ativa.
    if (file == m_currentFile && m_iniView.hasFile()) {
        showEditorTab();
        return;
    }

    if (!confirmIfDirtyBeforeSwitch()) {
        return;
    }

    loadFileIntoEditor(file);
    showEditorTab();
}

std::filesystem::path MainWindow::filePathFor(EditorFile file) const
{
    switch (file) {
    case EditorFile::PlaylistIni:
        return m_installation.playlistIniPath();
    case EditorFile::MapasComercial:
        return m_installation.mapasTxtPath();
    case EditorFile::GradesMusicais:
        return m_installation.gradesTxtPath();
    }
    return std::filesystem::path();
}

PlaylistIni* MainWindow::serviceFor(EditorFile file)
{
    switch (file) {
    case EditorFile::PlaylistIni:
        return &m_ini;
    case EditorFile::MapasComercial:
        return &m_mapas;
    case EditorFile::GradesMusicais:
        return &m_grades;
    }
    return &m_ini;
}

const PlaylistIni* MainWindow::serviceFor(EditorFile file) const
{
    return const_cast<MainWindow*>(this)->serviceFor(file);
}

std::wstring MainWindow::fileNameFor(EditorFile file) const
{
    switch (file) {
    case EditorFile::PlaylistIni:
        return L"playlist.ini";
    case EditorFile::MapasComercial:
        return L"mapa.txt";
    case EditorFile::GradesMusicais:
        return L"grade.txt";
    }
    return L"arquivo de texto";
}

bool MainWindow::confirmIfDirtyBeforeSwitch()
{
    if (!m_iniView.hasUnsavedChanges()) {
        return true;
    }

    const std::wstring fileName = fileNameFor(m_currentFile);
    const int answer = MessageBoxW(
        m_hWnd,
        (L"O arquivo \"" + fileName +
         L"\" possui alterações não salvas. Deseja salvá-las antes de continuar? "
         L"Escolher \"Não\" descarta as alterações.")
            .c_str(),
        L"Alterações não salvas", MB_YESNOCANCEL | MB_ICONWARNING);
    if (answer == IDCANCEL) {
        return false;
    }
    if (answer == IDYES && !saveCurrentFile()) {
        return false;
    }
    return true;
}

void MainWindow::loadFileIntoEditor(EditorFile file)
{
    m_currentFile = file;
    PlaylistIni* svc = serviceFor(file);

    const std::filesystem::path path = filePathFor(file);
    svc->setPath(path);

    if (path.empty()) {
        m_iniView.setErrorMessage(
            L"Nenhuma instalação do Playlist configurada.");
        setStatusText(L"Configure a localização do Playlist primeiro.");
        return;
    }

    // Nome exibido: usa o nome real do arquivo encontrado; se o arquivo
    // não existir, usa o nome canônico retornado por fileNameFor.
    const std::wstring actualName = path.filename().wstring();
    const std::wstring displayName = actualName.empty()
                                         ? fileNameFor(file)
                                         : actualName;
    svc->setDisplayName(displayName);

    Log::info(L"loadFile: " + displayName + L" -> " + path.wstring());

    if (!svc->exists()) {
        m_iniView.setContent(L"");
        m_iniView.setFileName(displayName);
        setStatusText(displayName + L" ainda não existe — clique em Salvar "
                                      L"(disquete) para criá-lo.");
        return;
    }

    std::wstring content;
    std::wstring userMessage;
    std::string technicalError;
    if (!svc->load(content, userMessage, technicalError)) {
        m_iniView.setErrorMessage(userMessage.empty()
                                      ? (L"Falha ao carregar \"" + displayName + L"\".")
                                      : userMessage);
        setStatusText(L"Falha ao carregar " + displayName + L".");
        return;
    }

    m_iniView.setContent(content);
    m_iniView.setFileName(displayName);
    setStatusText(displayName + L" carregado.");
}

bool MainWindow::saveCurrentFile()
{
    PlaylistIni* svc = serviceFor(m_currentFile);
    const std::wstring fileName = fileNameFor(m_currentFile);
    const std::wstring displayName = svc->displayName().empty()
                                         ? fileName
                                         : svc->displayName();

    if (!m_iniView.hasFile() || !svc->hasPath()) {
        MessageBoxW(m_hWnd,
                    (L"Não há um arquivo de texto carregado para salvar (\"" +
                     displayName + L"\").").c_str(),
                    L"Salvar", MB_ICONINFORMATION);
        return false;
    }

    const std::wstring content = m_iniView.getContent();
    const bool wasMissing = !svc->exists();

    // Sem alterações e arquivo já existe: não reescreve.
    if (!m_iniView.hasUnsavedChanges() && !wasMissing) {
        m_iniView.markSaved();
        return true;
    }

    // Garante que a pasta do arquivo exista (necessário para criar arquivos novos).
    if (!svc->path().parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(svc->path().parent_path(), ec);
        if (ec) {
            const std::wstring failMessage =
                L"Não foi possível criar a pasta do arquivo \"" + displayName +
                L"\". Verifique as permissões em " +
                svc->path().parent_path().wstring() + L".";
            MessageBoxW(m_hWnd, failMessage.c_str(), L"Erro ao salvar",
                        MB_ICONERROR);
            Log::error(L"create_directories falhou (" +
                       std::to_wstring(ec.value()) + L"): " +
                       svc->path().parent_path().wstring());
            return false;
        }
    }

    std::wstring userMessage;
    std::string technicalError;
    if (!svc->save(content, userMessage, technicalError)) {
        const std::wstring failMessage =
            userMessage.empty() ? (L"Não foi possível salvar \"" + displayName + L"\".")
                                : userMessage;
        MessageBoxW(m_hWnd, failMessage.c_str(), L"Erro ao salvar", MB_ICONERROR);
        if (!technicalError.empty()) {
            Log::error(technicalError);
        }
        return false;
    }

    m_iniView.markSaved();
    setStatusText(wasMissing
                      ? (displayName + L" criado com sucesso.")
                      : (L"Alterações salvas em \"" + displayName + L"\"."));
    return true;
}

void MainWindow::discardCurrentFile()
{
    if (m_iniView.hasUnsavedChanges()) {
        const int answer = MessageBoxW(
            m_hWnd,
            (L"Há alterações não salvas no arquivo \"" +
             fileNameFor(m_currentFile) +
             L"\". Deseja desconsiderá-las e recarregar o arquivo do disco?").c_str(),
            L"Cancelar alterações", MB_YESNO | MB_ICONWARNING);
        if (answer != IDYES) {
            return;
        }
    }
    loadFileIntoEditor(m_currentFile);
}

bool MainWindow::confirmCloseIfDirty()
{
    if (!m_iniView.hasUnsavedChanges()) {
        return true;
    }

    const std::wstring fileName = fileNameFor(m_currentFile);
    const int answer = MessageBoxW(
        m_hWnd,
        (L"Há alterações não salvas no arquivo \"" + fileName +
         L"\". Deseja salvá-las antes de fechar?").c_str(),
        L"Alterações não salvas", MB_YESNOCANCEL | MB_ICONWARNING);
    if (answer == IDCANCEL) {
        return false;
    }
    if (answer == IDNO) {
        return true;
    }
    return saveCurrentFile();
}

void MainWindow::reloadCodes()
{
    const std::filesystem::path xmlPath = m_installation.foldersXmlPath();
    if (xmlPath.empty()) {
        m_codesView.showMessage(
            L"folders.xml não encontrado na pasta da instalação do Playlist.");
        setStatusText(L"folders.xml não encontrado.");
        return;
    }

    std::vector<FolderEntry> entries;
    std::string technicalError;
    Log::info(L"folders.xml sendo lido de: " + xmlPath.wstring());
    const std::wstring parseMessage =
        FoldersXml::readAll(xmlPath, entries, technicalError);
    if (!parseMessage.empty()) {
        m_codesView.showMessage(parseMessage);
        if (!technicalError.empty()) {
            Log::error(technicalError);
        }
        setStatusText(L"Falha ao ler o folders.xml.");
        return;
    }

    Log::info(L"folders.xml: " + std::to_wstring(entries.size()) +
              L" registro(s) lido(s) com sucesso.");
    m_codesView.populate(entries, xmlPath);

    wchar_t debugEnv[8]{};
    if (GetEnvironmentVariableW(L"PC_UI_DEBUG", debugEnv, 8) != 0 &&
        debugEnv[0] == L'1') {
        Log::info(L"[ui-debug] ListView itens = " +
                  std::to_wstring(m_codesView.itemCount()));
    }
    setStatusText(L"folders.xml lido com sucesso.");
}

void MainWindow::openConfigManager()
{
    const std::filesystem::path configManager = m_installation.configManagerPath();
    if (configManager.empty()) {
        MessageBoxW(m_hWnd,
                    L"Não foi possível localizar o arquivo ConfigManager.exe "
                    L"na pasta Pgm da instalação do Playlist.",
                    L"ConfigManager não encontrado", MB_ICONERROR);
        Log::info(L"ConfigManager.exe não encontrado a partir de: " +
                  m_installation.installFolder().wstring());
        return;
    }

    const HINSTANCE execResult = ShellExecuteW(
        m_hWnd, L"open", configManager.c_str(), nullptr,
        configManager.parent_path().c_str(), SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(execResult) <= 32) {
        MessageBoxW(m_hWnd, L"Não foi possível abrir o ConfigManager.exe.",
                    L"Erro ao abrir", MB_ICONERROR);
        Log::error(L"ShellExecuteW falhou ao abrir: " + configManager.wstring());
    } else {
        Log::info(L"ConfigManager.exe aberto: " + configManager.wstring());
    }
}

void MainWindow::logChildRects() const
{
    // Diagnóstico opcional de layout (apenas quando PC_UI_DEBUG=1 está
    // definido no ambiente). Útil para conferir que nenhum controle fica
    // sobreposto a outro.
    wchar_t env[8]{};
    if (GetEnvironmentVariableW(L"PC_UI_DEBUG", env, 8) == 0 ||
        env[0] != L'1') {
        return;
    }

    const auto rectText = [](HWND hwnd, const wchar_t* name) {
        RECT rc{};
        if (hwnd) {
            GetWindowRect(hwnd, &rc);
        }
        return std::wstring(name) + L" = (" +
               std::to_wstring(rc.left) + L", " + std::to_wstring(rc.top) +
               L")-" + std::to_wstring(rc.right) + L", " +
               std::to_wstring(rc.bottom) + L")";
    };

    RECT client{};
    GetClientRect(m_hWnd, &client);
    Log::info(L"[ui-debug] client = (0,0)-(" +
              std::to_wstring(client.right) + L", " +
              std::to_wstring(client.bottom) + L")");
    Log::info(L"[ui-debug] " + rectText(m_hTab, L"tab"));

    // Área de página (abaixo da faixa de abas).
    RECT tabClient{};
    GetClientRect(m_hTab, &tabClient);
    SendMessageW(m_hTab, TCM_ADJUSTRECT, FALSE,
                 reinterpret_cast<LPARAM>(&tabClient));
    MapWindowPoints(m_hTab, m_hWnd,
                    reinterpret_cast<POINT*>(&tabClient), 2);
    Log::info(L"[ui-debug] page = (" +
              std::to_wstring(tabClient.left) + L", " +
              std::to_wstring(tabClient.top) + L")-(" +
              std::to_wstring(tabClient.right) + L", " +
              std::to_wstring(tabClient.bottom) + L")");

    Log::info(L"[ui-debug] " + rectText(m_hStatusStatic, L"status"));
}