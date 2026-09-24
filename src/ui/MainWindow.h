#pragma once

#include <windows.h>

#include <filesystem>
#include <string>

#include "playlist/PlaylistIni.h"
#include "playlist/PlaylistInstallation.h"
#include "ui/CodesPageView.h"
#include "ui/IniEditorView.h"

class Settings;
class PlaylistLocator;

// Janela principal da aplicação.
//
// A interface é dirigida por um menu (Arquivo | Editar) no topo:
//   Arquivo:
//     Configuração de pastas  -> abre o ConfigManager.exe
//     Lista de IDs            -> exibe os DBFId do folders.xml (guia Códigos)
//     Trocar localização      -> altera/consulta o caminho do Playlist.exe
//     Salvar                  -> salva o arquivo em edição
//     Descartar alterações    -> recarrega o arquivo em edição do disco
//     Sair                    -> fecha o programa
//   Editar:
//     Programação             -> playlist.ini
//     Mapa Comercial          -> mapas\Mapas.txt
//     GradesMusicais          -> Grades\Grades.txt
//
// A lógica de negócio NÃO fica aqui: a janela apenas coordena os componentes
// (PlaylistLocator, PlaylistInstallation, PlaylistIni, FoldersXml) com os
// controles visuais. As guias ficam em classes próprias (IniEditorView e
// CodesPageView).
class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    bool create(HINSTANCE hInstance, int nCmdShow);

    // Injeção dos componentes de serviço (criados pelo Application).
    void setContext(Settings* settings, PlaylistLocator* locator);

    void runMessageLoop();

    // Garante que existe um Playlist.exe válido; abre o diálogo se necessário.
    void ensurePlaylistPath();

private:
    // Arquivos de programação editáveis pelo Bloco de Notas interno.
    enum class EditorFile {
        PlaylistIni,
        MapasComercial,
        GradesMusicais,
    };

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void createMenu();
    void createControls();
    void onCommand(int id, int notifyCode);
    void onMenuCommand(int id);
    void doLayout();
    void applyCurrentTab();
    void showEditorTab();
    void showCodesTab();
    void setStatusText(const std::wstring& text);
    void logChildRects() const;

    void refreshFromLocator();
    void openLocateDialog();
    void reloadCodes();
    void openConfigManager();

    // Edição de arquivos de texto.
    void onEditFile(EditorFile file);
    std::filesystem::path filePathFor(EditorFile file) const;
    PlaylistIni* serviceFor(EditorFile file);
    const PlaylistIni* serviceFor(EditorFile file) const;
    std::wstring fileNameFor(EditorFile file) const;
    bool confirmIfDirtyBeforeSwitch();
    void loadFileIntoEditor(EditorFile file);
    bool saveCurrentFile();
    void discardCurrentFile();
    bool confirmCloseIfDirty();

    // Identificadores de controle da janela principal (as guias usam IDs
    // próprios definidos em IniEditorView/CodesPageView).
    static constexpr int IDC_TAB = 1005;
    static constexpr int IDC_STATUS_STATIC = 1006;

    // Identificadores dos comandos de menu.
    static constexpr int IDM_FILE_CONFIG_MGR = 4001;
    static constexpr int IDM_FILE_LIST_IDS = 4002;
    static constexpr int IDM_FILE_CHANGE_LOCATION = 4003;
    static constexpr int IDM_FILE_SAVE = 4004;
    static constexpr int IDM_FILE_DISCARD = 4005;
    static constexpr int IDM_FILE_EXIT = 4006;
    static constexpr int IDM_EDIT_PROGRAMACAO = 4101;
    static constexpr int IDM_EDIT_MAPA_COMERCIAL = 4102;
    static constexpr int IDM_EDIT_GRADES = 4103;

    HWND m_hWnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HFONT m_hUiFont = nullptr;
    HFONT m_hMonoFont = nullptr;
    HMENU m_hMenu = nullptr;
    HMENU m_hFileMenu = nullptr;
    HMENU m_hEditMenu = nullptr;

    HWND m_hTab = nullptr;
    HWND m_hStatusStatic = nullptr;

    // Páginas das guias (cada uma gerencia seus próprios controles).
    IniEditorView m_iniView;
    CodesPageView m_codesView;

    Settings* m_settings = nullptr;
    PlaylistLocator* m_locator = nullptr;

    // Instalação atual (derivada do caminho salvo) e os serviços de edição
    // dos arquivos de programação (playlist.ini, mapas\Mapas.txt,
    // Grades\Grades.txt).
    PlaylistInstallation m_installation;
    PlaylistIni m_ini;
    PlaylistIni m_mapas;
    PlaylistIni m_grades;
    EditorFile m_currentFile = EditorFile::PlaylistIni;

    int m_activeTab = 0;
};