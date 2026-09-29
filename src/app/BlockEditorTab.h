#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <memory>
#include <vector>

#include "app/BlockEditor.h"
#include "blocos/CodeCatalogue.h"
#include "playlist/PlaylistIni.h"
#include "readconf/FormatRules.h"

namespace app {
class PlaylistConfigController;
} // namespace app

class PlaylistInstallation;

namespace app {

// Sub-abas do tab Blocos com callback de troca de aba (como RelogioEditorTabs).
class BlockEditorTabs final : public juce::TabbedComponent {
public:
    BlockEditorTabs() : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop)
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

class BlockEditorTab;

// Uma sub-aba do tab Blocos = UM arquivo de bloco (Mapa.txt / Grade.txt ou
// MapaSeg.txt.. / GradeSeg.txt.. conforme a configuração do playlist.ini), com
// dois modos sincronizados sobre o MESMO documento em memória
// (blocos::BlockDocument): "Visual" (editor de horários com códigos) e "Texto"
// (edição textual crua). Nada é gravado até o Salvar.
//
// Os códigos vêm de um catálogo COMPARTILHADO entre todas as páginas
// (blocos::CodeCatalogue), alimentado pelo folders.xml (somente leitura).
class BlockFilePage final : public juce::Component {
public:
    enum class Mode { Visual, Texto };

    BlockFilePage(BlockEditorTab& host, std::filesystem::path path,
                  std::wstring displayName, blocos::CodeCatalogue& catalogue,
                  std::vector<BlockClip>& clipboard);
    ~BlockFilePage() override;

    void reload();
    bool save(std::wstring& userMessage, std::string& technicalError);

    void setMode(Mode mode);

    bool isDirty() const { return m_dirty; }
    bool hasFileOnDisk() const { return m_hasFileOnDisk; }
    const std::wstring& displayName() const { return m_displayName; }
    std::wstring pathString() const { return m_service.path().wstring(); }
    Mode mode() const { return m_mode; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void onDocChanged();
    void reloadDoNotNotify(std::wstring& text);

    BlockEditorTab& m_host;

    PlaylistIni m_service;
    std::wstring m_displayName;
    bool m_hasFileOnDisk = false;
    bool m_dirty = false;

    Mode m_mode = Mode::Visual;
    blocos::BlockDocument m_doc;
    BlockEditor m_editor;
    juce::TextEditor m_textEditor;
    bool m_updatingText = false;
};

// Tab "Blocos": editor VISUAL (e textual) dos arquivos de Mapas Comerciais e
// Grades Musicais, com uma sub-aba por arquivo (único ou semanal, conforme a
// configuração do playlist.ini). Os horários copiados (Ctrl+C) ficam numa área
// de transferência COMPARTILHADA entre as sub-abas.
class BlockEditorTab final : public juce::Component {
public:
    BlockEditorTab(app::PlaylistConfigController& controller,
                   PlaylistInstallation& installation);
    ~BlockEditorTab() override;

    // Abre um escopo de blocos (Comercial = Mapas, Musical = Grades).
    void openBlocos(readconf::ConfigScope scope);

    void reopenFromDisk();
    bool saveCurrentFile();
    bool hasUnsavedChanges() const;
    bool hasOpenPages() const { return !m_pages.empty(); }

    void refreshFromController();
    readconf::ConfigScope currentScope() const { return m_current; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void onPageChanged();

private:
    struct Spec {
        std::filesystem::path path;
        std::wstring displayName;
    };

    void rebuildPages();
    std::vector<Spec> resolveSpecs(readconf::ConfigScope scope) const;
    BlockFilePage* currentPage() const;
    void updateStatus();
    void setStatus(const std::wstring& text);
    void setCurrentPageMode(BlockFilePage::Mode mode);
    void updateModeIcons();
    void saveCurrentPage();
    void discardCurrentPage();
    // Relê o folders.xml (Lista de Códigos) para alimentar o catálogo.
    void reloadCatalogue();

    app::PlaylistConfigController& m_controller;
    PlaylistInstallation& m_installation;

    std::vector<std::unique_ptr<BlockFilePage>> m_pages;
    readconf::ConfigScope m_current = readconf::ConfigScope::Comercial;

    blocos::CodeCatalogue m_catalogue;
    // Aviso da última leitura do folders.xml (vazio quando deu certo).
    std::wstring m_catalogueWarning;

    // Área de transferência de horários + códigos COMPARTILHADA.
    std::vector<BlockClip> m_clipboard;

    BlockEditorTabs m_fileTabs;
    juce::TextButton m_visualBtn{ L"Visual" };
    juce::TextButton m_textBtn{ L"Texto" };
    juce::TextButton m_saveBtn{ L"Salvar" };
    juce::TextButton m_discardBtn{ L"Descartar" };
    juce::Label m_fileLabel;
    juce::Label m_statusLabel;
    juce::Rectangle<int> m_dividerBar;
};

} // namespace app
