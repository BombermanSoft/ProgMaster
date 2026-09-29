#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "app/BlockEditorTab.h"
#include "app/CodesTab.h"
#include "app/ConfiguratorTab.h"
#include "app/EditorTab.h"
#include "app/PlaylistConfigController.h"
#include "app/RelogioEditorTab.h"
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
// Menu "Editar" (Etapa 3/4 — interfaces simplificadas):
//   Programação            -> guia Configuração (playlist.ini visual)
//   Blocos Musicais        -> guia Blocos (editor VISUAL das Grades)
//   Blocos Comerciais      -> guia Blocos (editor VISUAL dos Mapas)
//   Relógio Musical        -> guia Relógio (editor VISUAL do relógio)
//   Relógio Comercial      -> guia Relógio (editor VISUAL do relógio)
// Menu "Avançado" (o antigo menu "Editar" textual da Etapa 2):
//   Programação            -> guia Editor (playlist.ini no bloco de notas)
//   Mapa Comercial         -> arquivo(s) dos mapas (único ou semanais)
//   Grades Musicais        -> arquivo(s) das grades (único ou semanais)
//   Relógio Comercial      -> arquivo(s) do relógio comercial
//   Relógio Musical        -> arquivo(s) do relógio musical
//   (conforme a configuração atual: Único = 1 aba, Semanal = 7 abas)
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
        // Menu "Editar" (Etapa 3 — visual).
        IDM_EDIT_PROGRAMACAO = 4101,
        IDM_EDIT_BLOCOS_MUSICAIS,
        IDM_EDIT_BLOCOS_COMERCIAL,
        IDM_EDIT_RELOGIO_MUSICAL,
        IDM_EDIT_RELOGIO_COMERCIAL,
        // Menu "Avançado" (textual, antigo "Editar").
        IDM_ADV_PROGRAMACAO = 4201,
        IDM_ADV_MAPA_COMERCIAL,
        IDM_ADV_GRADES,
        IDM_ADV_RELOGIO_COMERCIAL,
        IDM_ADV_RELOGIO_MUSICAL,
    };

    static constexpr int TAB_EDITOR = 0;
    static constexpr int TAB_CONFIGURACAO = 1;
    static constexpr int TAB_CODIGOS = 2;
    static constexpr int TAB_RELOGIO = 3;
    static constexpr int TAB_BLOCOS = 4;

    // Conteúdo da janela: guias + barra de status (não muda com o tamanho).
    class ContentPane final : public juce::Component {
    public:
        ContentPane(juce::TabbedComponent& tabs, juce::Label& status);
        void resized() override;

    private:
        juce::TabbedComponent& m_tabs;
        juce::Label& m_status;
        // Janela de tooltips: habilitar o texto explicativo ao passar o mouse
        // sobre os controles (setTooltip).
        juce::TooltipWindow m_tooltips;
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
    RelogioEditorTab m_relogioTab;
    BlockEditorTab m_blocosTab;
    juce::TabbedComponent m_tabs;
    juce::Label m_statusLabel;
    ContentPane m_contentPane;
};

} // namespace app