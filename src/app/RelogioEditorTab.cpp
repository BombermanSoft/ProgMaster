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
                                 std::vector<relogio::Param>& clipboard)
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

    // Troca de modo: Visual e Texto são os DOIS lados do mesmo documento.
    m_visualBtn.setClickingTogglesState(true);
    m_visualBtn.setRadioGroupId(200);
    m_visualBtn.onClick = [this] { setMode(Mode::Visual); };
    m_textBtn.setClickingTogglesState(true);
    m_textBtn.setRadioGroupId(200);
    m_textBtn.onClick = [this] { setMode(Mode::Texto); };
    m_saveBtn.onClick = [this] { m_host.saveCurrentFile(); };
    m_discardBtn.onClick = [this] {
        reload();
        m_host.onPageChanged();
    };
    for (juce::TextButton* btn :
         { &m_visualBtn, &m_textBtn, &m_saveBtn, &m_discardBtn }) {
        addAndMakeVisible(btn);
    }
    m_saveBtn.setTooltip(L"Salvar este arquivo de relógio.");
    m_discardBtn.setTooltip(L"Descartar alterações deste relógio (recarregar do disco).");

    m_pathLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_pathLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    m_pathLabel.setJustificationType(juce::Justification::centredRight);
    m_pathLabel.setTooltip(jstr(pathString()));
    addAndMakeVisible(m_pathLabel);

    setMode(Mode::Visual);
    reload();
    updateModeButtons();
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
    updateModeButtons();
}

void RelogioFilePage::onDocChanged()
{
    m_dirty = true;
    m_host.onPageChanged();
}

void RelogioFilePage::updateModeButtons()
{
    m_visualBtn.setToggleState(m_mode == Mode::Visual, juce::dontSendNotification);
    m_textBtn.setToggleState(m_mode == Mode::Texto, juce::dontSendNotification);
    m_pathLabel.setText(jstr(pathString()), juce::dontSendNotification);
}

void RelogioFilePage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void RelogioFilePage::resized()
{
    const int margin = 6;
    const int topH = 26;
    const auto area = getLocalBounds();

    const int y = area.getY() + 2;
    int x = area.getX() + margin;
    const auto step = [&x, &y, &topH, margin](juce::Component& c, int w, int gap) {
        c.setBounds(x, y, w, topH);
        x += w + gap;
    };
    step(m_visualBtn, 64, 3);
    step(m_textBtn, 60, 12);
    step(m_saveBtn, 70, 3);
    step(m_discardBtn, 130, 12);
    m_pathLabel.setBounds(x, y, area.getRight() - x - margin, topH);
    m_pathLabel.setJustificationType(juce::Justification::centredLeft);

    const int contentTop = y + topH + 4;
    const juce::Rectangle<int> content(
        area.getX(), contentTop, area.getWidth(),
        juce::jmax(0, area.getBottom() - contentTop));
    m_editor.setBounds(content);
    m_textEditor.setBounds(content);
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
}

void RelogioEditorTab::setStatus(const std::wstring& text)
{
    m_statusLabel.setText(app::jstr(text), juce::dontSendNotification);
}

void RelogioEditorTab::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void RelogioEditorTab::resized()
{
    const int margin = 6;
    const int bottomH = 22;
    const auto area = getLocalBounds();

    const int tabsTop = area.getY() + 2;
    const int tabsH = juce::jmax(0, area.getHeight() - 2 - bottomH - margin);
    m_fileTabs.setBounds(area.getX() + margin, tabsTop, area.getWidth() - 2 * margin,
                         tabsH);

    const int labelW = 300;
    m_fileLabel.setBounds(area.getX() + margin,
                          area.getBottom() - bottomH - margin,
                          juce::jmax(0, area.getWidth() - 2 * margin - labelW),
                          bottomH);
    m_statusLabel.setBounds(m_fileLabel.getRight() + 6, m_fileLabel.getY(),
                            labelW - 6, bottomH);
}

} // namespace app