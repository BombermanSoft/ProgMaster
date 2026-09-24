#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "app/CodesTab.h"
#include "app/ConfiguratorTab.h"
#include "app/EditorTab.h"
#include "app/PlaylistConfigController.h"
#include "playlist/PlaylistInstallation.h"

class PlaylistLocator;

namespace app {

// Janela principal da Etapa 2 (JUCE).
//
// Coordena os componentes de serviço (PlaylistLocator, PlaylistInstallation,
// PlaylistConfigController) com os controles visuais. A lógica de negócio
// NÃO fica aqui — a janela apenas orquestra.
//
// Menu "Arquivo":
//   Configuração de pastas -> abre o ConfigManager.exe
//   Lista de IDs           -> guia Códigos (folders.xml)
//   Trocar localização     -> diálogo do Playlist.exe
//   Salvar                 -> salva o arquivo em edição (ou a configuração)
//   Descartar alterações   -> recarrega o arquivo em edição do disco
//   Sair                   -> fecha (com aviso se houver não salvo)
// Menu "Editar":
//   Programação            -> playlist.ini (bloco de notas)
//   Mapa Comercial         -> mapas\Mapas.txt
//   Grades Musicais        -> grades\Grades.txt
//   Relógio Comercial      -> Relogio.txt (conforme configuração)
//   Relógio Musical        -> Relogio.txt (conforme configuração)
class MainWindow final : public juce::DocumentWindow,
                         public juce::MenuBarModel {
public:
    MainWindow(PlaylistLocator& locator);
    ~MainWindow() override;

    // Coisas que mudaram na instalação (path salvo) -> atualiza tudo.
    void ensurePlaylistPath();

    // Fechamento: pergunta Salvar/Descartar/Cancelar quando houver pendências.
    void requestCloseWithConfirmation();

    // --- MenuBarModel ---------------------------------------------------------
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex,
                                    const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // --- DocumentWindow -------------------------------------------------------
    void closeButtonPressed() override;

private:
    enum MenuIds {
        IDM_FILE_CONFIG_MGR = 4001,
        IDM_FILE_LIST_IDS,
        IDM_FILE_CHANGE_LOCATION,
        IDM_FILE_SAVE,
        IDM_FILE_DISCARD,
        IDM_FILE_EXIT,
        IDM_EDIT_PROGRAMACAO = 4101,
        IDM_EDIT_MAPA_COMERCIAL,
        IDM_EDIT_GRADES,
        IDM_EDIT_RELOGIO_COMERCIAL,
        IDM_EDIT_RELOGIO_MUSICAL,
    };

    static constexpr int TAB_EDITOR = 0;
    static constexpr int TAB_CONFIGURACAO = 1;
    static constexpr int TAB_CODIGOS = 2;

    // Conteúdo da janela: guias + barra de status (não muda com o tamanho).
    class ContentPane final : public juce::Component {
    public:
        ContentPane(juce::TabbedComponent& tabs, juce::Label& status);
        void resized() override;

    private:
        juce::TabbedComponent& m_tabs;
        juce::Label& m_status;
    };

    void refreshFromLocator();
    void rebuildFromNewLocation();
    void openLocateDialog();
    void openConfigManager();
    void showTab(int index);
    void saveOrDiscardCurrent(bool save);
    void finishQuit();
    void setStatus(const std::wstring& text);

    PlaylistLocator& m_locator;

    PlaylistInstallation m_installation;
    PlaylistConfigController m_controller;
    EditorTab m_editorTab;
    CodesTab m_codesTab;
    ConfiguratorTab m_configTab;
    juce::TabbedComponent m_tabs;
    juce::Label m_statusLabel;
    ContentPane m_contentPane;
};

} // namespace app