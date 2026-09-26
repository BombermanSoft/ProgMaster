#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <memory>
#include <vector>

#include "app/RelogioEditor.h"
#include "playlist/PlaylistIni.h"
#include "readconf/FormatRules.h"
#include "relogio/RelogioDocument.h"

namespace app {
class PlaylistConfigController;
} // namespace app

class PlaylistInstallation;

namespace app {

// Sub-abas do tab Relógio com callback de troca de aba (como EditorTabTabs).
class RelogioEditorTabs final : public juce::TabbedComponent {
public:
    RelogioEditorTabs() : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop)
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

class RelogioEditorTab;

// Uma sub-aba do tab Relógio = UM arquivo de relógio (Relogio.txt ou
// RelogioSeg.txt..), com dois modos sincronizados sobre o MESMO documento em
// memória (relogio::RelogioDocument): "Visual" (editor de horários com
// parâmetros) e "Texto" (edição textual crua). Alterações na Visual são
// refletidas na Texto e vice-versa. Nada é gravado até o Salvar.
//
// Classe no namespace app (não aninhada) por quirk do MSVC (ver C.4).
class RelogioFilePage final : public juce::Component {
public:
    enum class Mode { Visual, Texto };

    RelogioFilePage(RelogioEditorTab& host, std::filesystem::path path,
                    std::wstring displayName,
                    std::vector<relogio::Param>& clipboard);
    ~RelogioFilePage() override;

    // Carrega do disco (arquivo inexistente começa vazio).
    void reload();
    bool save(std::wstring& userMessage, std::string& technicalError);

    bool isDirty() const { return m_dirty; }
    bool hasFileOnDisk() const { return m_hasFileOnDisk; }
    const std::wstring& displayName() const { return m_displayName; }
    std::wstring pathString() const { return m_service.path().wstring(); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void setMode(Mode mode);
    void onDocChanged();
    void updateModeButtons();
    void reloadDoNotNotify(std::wstring& text);

    RelogioEditorTab& m_host;

    PlaylistIni m_service;
    std::wstring m_displayName;
    bool m_hasFileOnDisk = false;
    bool m_dirty = false;

    Mode m_mode = Mode::Visual;
    relogio::RelogioDocument m_doc;
    RelogioEditor m_editor;
    juce::TextEditor m_textEditor;
    bool m_updatingText = false;

    juce::TextButton m_visualBtn{ L"Visual" };
    juce::TextButton m_textBtn{ L"Texto" };
    juce::TextButton m_saveBtn{ L"Salvar" };
    juce::TextButton m_discardBtn{ L"Descartar alterações" };
    juce::Label m_pathLabel;
};

// Tab "Relógio": editor VISUAL (e textual) dos arquivos de relógio, com uma
// sub-aba por arquivo (Único = Relogio.txt; Semanal = RelogioSeg..RelogioDom,
// conforme a configuração do playlist.ini). Os parâmetros copiados (Ctrl+C)
// ficam numa área de transferência COMPARTILHADA entre as sub-abas, permitindo
// colar (Ctrl+V) parâmetros entre horários de relógios/arquivos diferentes.
class RelogioEditorTab final : public juce::Component {
public:
    RelogioEditorTab(app::PlaylistConfigController& controller,
                     PlaylistInstallation& installation);
    ~RelogioEditorTab() override;

    // Abre um escopo de relógio (musical ou comercial), criando as sub-abas.
    void openRelogio(readconf::ConfigScope scope);

    // Recarrega os arquivos abertos do disco (Descartar alterações).
    void reopenFromDisk();
    // Salva o arquivo ativo (menu Arquivo/Salvar também).
    bool saveCurrentFile();
    bool hasUnsavedChanges() const;
    bool hasOpenPages() const { return !m_pages.empty(); }

    void refreshFromController();
    readconf::ConfigScope currentScope() const { return m_current; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Chamado pelas páginas quando o conteúdo muda (marca/no sujo + status).
    void onPageChanged();

private:
    struct Spec {
        std::filesystem::path path;
        std::wstring displayName;
    };

    void rebuildPages();
    std::vector<Spec> resolveSpecs(readconf::ConfigScope scope) const;
    RelogioFilePage* currentPage() const;
    void updateStatus();
    void setStatus(const std::wstring& text);

    app::PlaylistConfigController& m_controller;
    PlaylistInstallation& m_installation;

    std::vector<std::unique_ptr<RelogioFilePage>> m_pages;
    readconf::ConfigScope m_current = readconf::ConfigScope::RelogioMusical;

    // Área de transferência de parâmetros COMPARTILHADA entre as páginas.
    std::vector<relogio::Param> m_paramsClipboard;

    RelogioEditorTabs m_fileTabs;
    juce::Label m_fileLabel;
    juce::Label m_statusLabel;
};

} // namespace app