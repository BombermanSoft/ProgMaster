#include "app/RelogioEditorTab.h"

#include "app/JuceHelpers.h"
#include "app/PlaylistConfigController.h"
#include "playlist/PlaylistInstallation.h"
#include "readconf/PlaylistFileLocator.h"
#include "readconf/ReadingConfiguration.h"

namespace {
juce::Font pageMonoFont()
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                        juce::Font::getDefaultStyle(), 15.0f));
}
} // namespace

namespace app {

// ============================================================================
// RelogioFilePage — um arquivo de relógio (Visual | Texto) no mesmo documento
// ============================================================================

RelogioFilePage::RelogioFilePage(RelogioEditorTab& host,
                                 std::filesystem::path path,
                                 std::wstring displayName,
                                 std::vector<ClipEntry>& clipboard)
    : m_host(host), m_service(std::move(path)),
      m_displayName(std::move(displayName)), m_editor(m_doc, clipboard)
{
    m_editor.onChange = [this] { onDocChanged(); };
    addAndMakeVisible(m_editor);

    m_textEditor.setMultiLine(true);
    m_textEditor.setScrollbarsShown(true);
    m_textEditor.setCaretVisible(true);
    m_textEditor.setPopupMenuEnabled(true);
    m_textEditor.setReturnKeyStartsNewLine(true);
    m_textEditor.setTabKeyUsedAsCharacter(true);
    m_textEditor.setFont(pageMonoFont());
    m_textEditor.onTextChange = [this] {
        if (!m_updatingText) {
            m_doc.setText(app::wstr(m_textEditor.getText()));
            onDocChanged();
        }
    };
    addAndMakeVisible(m_textEditor);

    setMode(Mode::Visual);
    reload();
}

RelogioFilePage::~RelogioFilePage() = default;

void RelogioFilePage::reload()
{
    std::wstring text;
    std::wstring msg;
    std::string err;
    m_hasFileOnDisk = m_service.load(text, msg, err);
    if (!m_hasFileOnDisk) {
        text.clear();
    }
    m_doc.setText(text);
    m_editor.rebuild();
    m_updatingText = true;
    m_textEditor.setText(app::jstr(text), false);
    m_updatingText = false;
    m_dirty = false;
}

bool RelogioFilePage::save(std::wstring& userMessage,
                           std::string& technicalError)
{
    const bool ok = m_service.save(m_doc.text(), userMessage, technicalError);
    if (ok) {
        m_dirty = false;
        m_hasFileOnDisk = true;
    }
    return ok;
}

void RelogioFilePage::setMode(Mode mode)
{
    m_mode = mode;
    m_editor.setVisible(mode == Mode::Visual);
    m_textEditor.setVisible(mode == Mode::Texto);
    if (mode == Mode::Visual) {
        m_editor.rebuild();
    } else {
        // Sincroniza o texto com o estado atual do documento.
        m_updatingText = true;
        m_textEditor.setText(app::jstr(m_doc.text()), false);
        m_updatingText = false;
    }
}

void RelogioFilePage::onDocChanged()
{
    m_dirty = true;
    m_host.onPageChanged();
}

void RelogioFilePage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void RelogioFilePage::resized()
{
    // A página é só o conteúdo: os ícones de modo ficam no topo do tab e o
    // caminho + Salvar/Descartar ficam no rodapé do tab.
    m_editor.setBounds(getLocalBounds());
    m_textEditor.setBounds(getLocalBounds());
}

// ============================================================================
// RelogioEditorTab — sub-abas por arquivo, paleta compartilhada
// ============================================================================

RelogioEditorTab::RelogioEditorTab(
    app::PlaylistConfigController& controller,
    PlaylistInstallation& installation)
    : m_controller(controller), m_installation(installation)
{
    m_fileTabs.onTabChanged = [this] { updateStatus(); };
    addAndMakeVisible(m_fileTabs);

    // Botões de modo (Visual/Texto) — compartilhados, na fileira do rodapé.
    m_visualBtn.setClickingTogglesState(true);
    m_visualBtn.setRadioGroupId(200);
    m_visualBtn.onClick = [this] {
        setCurrentPageMode(RelogioFilePage::Mode::Visual);
    };
    m_textBtn.setClickingTogglesState(true);
    m_textBtn.setRadioGroupId(200);
    m_textBtn.onClick = [this] {
        setCurrentPageMode(RelogioFilePage::Mode::Texto);
    };
    m_visualBtn.setTooltip(L"Edição Visual");
    m_textBtn.setTooltip(L"Edição Textual");
    addAndMakeVisible(m_visualBtn);
    addAndMakeVisible(m_textBtn);

    // Salvar / Descartar alterações — na linha do rodapé (com o caminho).
    m_saveBtn.onClick = [this] { saveCurrentPage(); };
    m_discardBtn.onClick = [this] { discardCurrentPage(); };
    m_saveBtn.setTooltip(L"Salvar este arquivo de relógio.");
    m_discardBtn.setTooltip(
        L"Descartar alterações deste relógio (recarregar do disco).");
    addAndMakeVisible(m_saveBtn);
    addAndMakeVisible(m_discardBtn);

    m_fileLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_fileLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    m_fileLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_fileLabel);
    m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_statusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    m_statusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_statusLabel);
}

RelogioEditorTab::~RelogioEditorTab() = default;

void RelogioEditorTab::openRelogio(readconf::ConfigScope scope)
{
    m_current = scope;
    rebuildPages();
    updateStatus();
}

void RelogioEditorTab::reopenFromDisk()
{
    for (auto& page : m_pages) {
        page->reload();
    }
    updateStatus();
}

bool RelogioEditorTab::saveCurrentFile()
{
    RelogioFilePage* page = currentPage();
    if (page == nullptr) {
        return false;
    }
    std::wstring msg;
    std::string err;
    const bool ok = page->save(msg, err);
    updateStatus();
    juce::AlertWindow::showMessageBoxAsync(
        ok ? juce::MessageBoxIconType::InfoIcon
           : juce::MessageBoxIconType::WarningIcon,
        ok ? L"Salvo" : L"Não foi possível salvar",
        ok ? app::jstr(L"O arquivo de relógio foi gravado.") : app::jstr(msg));
    return ok;
}

bool RelogioEditorTab::hasUnsavedChanges() const
{
    for (const auto& page : m_pages) {
        if (page->isDirty()) {
            return true;
        }
    }
    return false;
}

void RelogioEditorTab::refreshFromController()
{
    if (hasOpenPages()) {
        rebuildPages();
        updateStatus();
    }
}

void RelogioEditorTab::rebuildPages()
{
    m_pages.clear();
    m_fileTabs.clearTabs();
    for (const Spec& spec : resolveSpecs(m_current)) {
        auto page = std::make_unique<RelogioFilePage>(
            *this, spec.path, spec.displayName, m_paramsClipboard);
        m_fileTabs.addTab(app::jstr(spec.displayName),
                          juce::Colour(0xff2b2b2b), page.get(), false);
        m_pages.push_back(std::move(page));
    }
    if (!m_pages.empty()) {
        m_fileTabs.setCurrentTabIndex(0, false);
    }
}

std::vector<RelogioEditorTab::Spec> RelogioEditorTab::resolveSpecs(
    readconf::ConfigScope scope) const
{
    std::vector<Spec> out;
    const std::filesystem::path installFolder = m_controller.installationFolder();
    const readconf::ScopeSnapshot snap =
        readconf::readScope(m_controller.document(), scope, installFolder);
    if (snap.present && snap.option == readconf::FormatOption::Weekly) {
        const std::filesystem::path folder =
            readconf::folderFor(scope, installFolder);
        for (const std::wstring& day : readconf::weekdayFileNames()) {
            const std::wstring name = L"Relogio" + day + L".txt";
            out.push_back({ folder / name, name });
        }
    } else {
        const std::filesystem::path folder = readconf::folderFor(scope, installFolder);
        const std::wstring name = L"Relogio.txt";
        out.push_back({ folder / name, name });
    }
    return out;
}

RelogioFilePage* RelogioEditorTab::currentPage() const
{
    if (m_pages.empty()) {
        return nullptr;
    }
    const int index = m_fileTabs.getCurrentTabIndex();
    if (index < 0 || static_cast<size_t>(index) >= m_pages.size()) {
        return nullptr;
    }
    return m_pages[static_cast<size_t>(index)].get();
}

void RelogioEditorTab::onPageChanged()
{
    updateStatus();
}

void RelogioEditorTab::updateStatus()
{
    if (RelogioFilePage* page = currentPage()) {
        m_fileLabel.setText(app::jstr(page->pathString()), juce::dontSendNotification);
    } else {
        m_fileLabel.setText(juce::String(), juce::dontSendNotification);
    }

    std::wstring status;
    if (hasUnsavedChanges()) {
        status = L"Alterações não salvas";
    }
    if (RelogioFilePage* page = currentPage()) {
        if (!page->hasFileOnDisk()) {
            if (!status.empty()) {
                status += L"  |  ";
            }
            status += L"Arquivo ainda não existe no disco (será criado ao salvar)";
        }
    }
    m_statusLabel.setText(app::jstr(status), juce::dontSendNotification);
    updateModeIcons();
}

void RelogioEditorTab::setStatus(const std::wstring& text)
{
    m_statusLabel.setText(app::jstr(text), juce::dontSendNotification);
}

void RelogioEditorTab::setCurrentPageMode(RelogioFilePage::Mode mode)
{
    if (RelogioFilePage* page = currentPage()) {
        page->setMode(mode);
    }
    updateModeIcons();
}

void RelogioEditorTab::updateModeIcons()
{
    const RelogioFilePage* page = currentPage();
    const bool visual = (page == nullptr) ||
                        page->mode() == RelogioFilePage::Mode::Visual;
    m_visualBtn.setToggleState(visual, juce::dontSendNotification);
    m_textBtn.setToggleState(!visual, juce::dontSendNotification);
}

void RelogioEditorTab::saveCurrentPage()
{
    saveCurrentFile();
}

void RelogioEditorTab::discardCurrentPage()
{
    if (RelogioFilePage* page = currentPage()) {
        page->reload();
        updateStatus();
    }
}

void RelogioEditorTab::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
    if (!m_dividerBar.isEmpty()) {
        g.setColour(juce::Colour(0xff555555));
        g.fillRect(m_dividerBar);
    }
}

void RelogioEditorTab::resized()
{
    const int margin = 6;
    const int bottomH = 24;
    const auto area = getLocalBounds();

    // Topo — só as abas de relógio (RelogioSeg, RelogioTer...).
    const int tabsTop = area.getY() + 2;
    const int tabsH = juce::jmax(0, area.getHeight() - 2 - bottomH - margin);
    m_fileTabs.setBounds(margin, tabsTop, area.getWidth() - 2 * margin, tabsH);

    // Rodapé — MESMA fileira do caminho do arquivo, tudo numa linha só:
    //   <caminho...status> │ [Visual] [Texto] [Descartar] [Salvar]
    const int bottomY = area.getBottom() - bottomH - margin;
    const int gap = 6;

    // Bloco de botões NO CANTO DIREITO, grupos separados só pela divisória.
    const int visualW = 66;
    const int textW = 60;
    const int discardW = 88;
    const int saveW = 62;
    const int blockW = visualW + gap + textW + gap + discardW + gap + saveW;
    int x = area.getRight() - margin - blockW;
    m_visualBtn.setBounds(x, bottomY - 1, visualW, bottomH);
    x += visualW + gap;
    m_textBtn.setBounds(x, bottomY - 1, textW, bottomH);
    x += textW + gap;
    m_discardBtn.setBounds(x, bottomY - 1, discardW, bottomH);
    x += discardW + gap;
    m_saveBtn.setBounds(x, bottomY - 1, saveW, bottomH);

    // Barra separando os botões do restante da linha.
    const int divX = m_visualBtn.getX() - 10;
    m_dividerBar.setBounds(divX, bottomY + 2, 2, bottomH - 4);

    // Status antes da barra; caminho preenchendo o meio.
    const int statusW = 300;
    const int statusX = divX - gap - statusW;
    m_statusLabel.setBounds(statusX, bottomY - 1, statusW, bottomH);
    m_fileLabel.setBounds(area.getX() + margin, bottomY - 1,
                          juce::jmax(0, statusX - gap - area.getX() - margin),
                          bottomH);
}

} // namespace app