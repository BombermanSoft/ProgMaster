#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <memory>
#include <vector>

#include "playlist/PlaylistIni.h"

namespace app {
class PlaylistConfigController;
} // namespace app

class PlaylistInstallation;

// Tab predefinida do editor: expõe o callback de troca de aba (a versão do
// JUCE desta base só oferece o método virtual currentTabChanged).
class EditorTabTabs final : public juce::TabbedComponent {
public:
    EditorTabTabs()
        : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop)
    {
    }
    std::function<void()> onTabChanged;
    void currentTabChanged(int newCurrentTabIndex,
                           const juce::String& newCurrentTabName) override
    {
        juce::TabbedComponent::currentTabChanged(newCurrentTabIndex,
                                                 newCurrentTabName);
        if (onTabChanged) {
            onTabChanged();
        }
    }
};

// Tab "Editor": bloco de notas textual dos arquivos de programação.
//
// Para cada item do menu Editar, resolve os arquivos seguindo a configuração
// atual do playlist.ini: Único -> UMA aba com o arquivo único
// (Mapa.txt/Grade.txt/Relogio.txt); Semanal -> SETE abas, uma por dia da
// semana (MapaSeg.txt..MapaDom.txt, etc.), permitindo criar/editá-los. O
// playlist.ini, quando aberto aqui, usa o MESMO documento em memória do
// controlador de configuração (sincronização bidirecional ao entrar/sair).
class EditorTab final : public juce::Component {
public:
    // Arquivo textual em edição nesta aba.
    enum class FileKind {
        PlaylistIni,
        MapasComercial,
        GradesMusicais,
        RelogioComercial,
        RelogioMusical,
    };

    EditorTab(app::PlaylistConfigController& controller,
              PlaylistInstallation& installation);
    ~EditorTab() override;

    // Abre um conjunto de arquivos na aba (preenche as sub-abas conforme a
    // configuração: 1 arquivo único ou os 7 arquivos semanais).
    void openFile(FileKind file);
    // Recarrega os arquivos abertos do disco (Descartar alterações).
    void reopenFromDisk();
    // Salva o arquivo ativo (menu Arquivo/Salvar também).
    bool saveCurrentFile();

    // Puxa o conteúdo atual do playlist.ini para a sub-aba (usado após o
    // controlador recarregar o documento do disco).
    void refreshIniFromController();
    // True quando nenhuma sub-aba foi aberta ainda (estado inicial).
    bool hasOpenPages() const { return !m_pages.empty(); }

    bool hasUnsavedChanges() const;
    std::wstring currentFileName() const;
    FileKind currentKind() const { return m_current; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Chamado pelo JUCE ao exibir/ocultar a aba: mantém o playlist.ini em
    // sincronia com a interface (Configuração).
    void visibilityChanged() override;

private:
    class FilePage;
    struct Spec {
        std::filesystem::path path;
        std::wstring displayName;
        bool isIni = false;
    };

    void rebuildPages();
    std::vector<Spec> resolveSpecs(FileKind file) const;
    FilePage* currentPage() const;
    FilePage* iniPage() const;
    void syncIniPageFromController();
    void onPageTextChanged(FilePage& page);
    void setDirty(bool dirty);
    void updateButtons();
    void updateStatus();

    // Serviços (o playlist.ini vive no controlador, os demais em FilePage).
    app::PlaylistConfigController& m_controller;
    PlaylistInstallation& m_installation;

    std::vector<std::unique_ptr<FilePage>> m_pages;
    FileKind m_current = FileKind::PlaylistIni;
    bool m_dirty = false;

    juce::Label m_fileLabel;
    juce::DrawableButton m_saveButton{ "Salvar", juce::DrawableButton::ImageFitted };
    juce::DrawableButton m_undoButton{ "Desfazer", juce::DrawableButton::ImageFitted };
    juce::DrawableButton m_redoButton{ "Refazer", juce::DrawableButton::ImageFitted };
    EditorTabTabs m_fileTabs;
    juce::Label m_statusLabel;
};