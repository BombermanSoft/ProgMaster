#include "app/BlockEditorTab.h"

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
// BlockFilePage — um arquivo de bloco (Visual | Texto) no mesmo documento
// ============================================================================

BlockFilePage::BlockFilePage(BlockEditorTab& host, std::filesystem::path path,
                             std::wstring displayName,
                             blocos::CodeCatalogue& catalogue,
                             std::vector<BlockClip>& clipboard)
    : m_host(host), m_service(std::move(path)),
      m_displayName(std::move(displayName)),
      m_editor(m_doc, catalogue, clipboard)
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
            m_editor.rebuild();
            onDocChanged();
        }
    };
    addAndMakeVisible(m_textEditor);

    setMode(Mode::Visual);
    reload();
}

BlockFilePage::~BlockFilePage() = default;

void BlockFilePage::reload()
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

bool BlockFilePage::save(std::wstring& userMessage, std::string& technicalError)
{
    const bool ok = m_service.save(m_doc.text(), userMessage, technicalError);
    if (ok) {
        m_dirty = false;
        m_hasFileOnDisk = true;
    }
    return ok;
}

void BlockFilePage::setMode(Mode mode)
{
    m_mode = mode;
    m_editor.setVisible(mode == Mode::Visual);
    m_textEditor.setVisible(mode == Mode::Texto);
    if (mode == Mode::Visual) {
        m_editor.rebuild();
    } else {
        m_updatingText = true;
        m_textEditor.setText(app::jstr(m_doc.text()), false);
        m_updatingText = false;
    }
}

void BlockFilePage::onDocChanged()
{
    m_dirty = true;
    // Mantém o modo Texto sincronizado com o documento em memória.
    if (m_mode == Mode::Texto) {
        m_updatingText = true;
        m_textEditor.setText(app::jstr(m_doc.text()), false);
        m_updatingText = false;
    }
    m_host.onPageChanged();
}

void BlockFilePage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void BlockFilePage::resized()
{
    m_editor.setBounds(getLocalBounds());
    m_textEditor.setBounds(getLocalBounds());
}

// ============================================================================
// BlockEditorTab — sub-abas por arquivo, catálogo de códigos compartilhado
// ============================================================================

BlockEditorTab::BlockEditorTab(app::PlaylistConfigController& controller,
                               PlaylistInstallation& installation)
    : m_controller(controller), m_installation(installation)
{
    m_fileTabs.onTabChanged = [this] { updateStatus(); };
    addAndMakeVisible(m_fileTabs);

    m_visualBtn.setClickingTogglesState(true);
    m_visualBtn.setRadioGroupId(300);
    m_visualBtn.onClick = [this] { setCurrentPageMode(BlockFilePage::Mode::Visual); };
    m_textBtn.setClickingTogglesState(true);
    m_textBtn.setRadioGroupId(300);
    m_textBtn.onClick = [this] { setCurrentPageMode(BlockFilePage::Mode::Texto); };
    m_visualBtn.setTooltip(L"Edição Visual");
    m_textBtn.setTooltip(L"Edição Textual");
    addAndMakeVisible(m_visualBtn);
    addAndMakeVisible(m_textBtn);

    m_saveBtn.onClick = [this] { saveCurrentPage(); };
    m_discardBtn.onClick = [this] { discardCurrentPage(); };
    m_saveBtn.setTooltip(L"Salvar este arquivo de bloco.");
    m_discardBtn.setTooltip(L"Descartar alterações deste arquivo (recarregar do disco).");
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

    reloadCatalogue();
}

BlockEditorTab::~BlockEditorTab() = default;

void BlockEditorTab::reloadCatalogue()
{
    const std::filesystem::path foldersPath = m_installation.foldersXmlPath();
    std::string err;
    m_catalogueWarning = m_catalogue.loadFromFoldersXml(foldersPath, err);
    // Não é um bloqueio: o editor continua funcionando e o aviso fica no
    // rodapé. O folders.xml é somente leitura — nada é gravado.
}

void BlockEditorTab::openBlocos(readconf::ConfigScope scope)
{
    m_current = scope;
    rebuildPages();
    updateStatus();
}

void BlockEditorTab::reopenFromDisk()
{
    for (auto& page : m_pages) {
        page->reload();
    }
    updateStatus();
}

bool BlockEditorTab::saveCurrentFile()
{
    BlockFilePage* page = currentPage();
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
        ok ? L"O arquivo de bloco foi gravado." : app::jstr(msg));
    return ok;
}

bool BlockEditorTab::hasUnsavedChanges() const
{
    for (const auto& page : m_pages) {
        if (page->isDirty()) {
            return true;
        }
    }
    return false;
}

void BlockEditorTab::refreshFromController()
{
    reloadCatalogue();
    if (hasOpenPages()) {
        rebuildPages();
        updateStatus();
    }
}

void BlockEditorTab::rebuildPages()
{
    m_pages.clear();
    m_fileTabs.clearTabs();
    for (const Spec& spec : resolveSpecs(m_current)) {
        auto page = std::make_unique<BlockFilePage>(
            *this, spec.path, spec.displayName, m_catalogue, m_clipboard);
        m_fileTabs.addTab(app::jstr(spec.displayName),
                          juce::Colour(0xff2b2b2b), page.get(), false);
        m_pages.push_back(std::move(page));
    }
    if (!m_pages.empty()) {
        m_fileTabs.setCurrentTabIndex(0, false);
    }
}

// Mesma resolução de arquivos do editor textual (EditorTab::resolveSpecs):
// Semanal no playlist.ini -> sete abas (MapaSeg..MapaDom / GradeSeg..GradeDom);
// caso contrário -> um arquivo único.
std::vector<BlockEditorTab::Spec> BlockEditorTab::resolveSpecs(
    readconf::ConfigScope scope) const
{
    std::vector<Spec> out;
    const std::filesystem::path installFolder = m_controller.installationFolder();
    const bool musical = (scope == readconf::ConfigScope::Musical);
    const readconf::ScopeSnapshot snap =
        readconf::readScope(m_controller.document(), scope, installFolder);

    if (snap.present && snap.option == readconf::FormatOption::Weekly) {
        const std::filesystem::path folder =
            readconf::folderFor(scope, installFolder);
        const std::wstring prefix = musical ? L"Grade" : L"Mapa";
        for (const std::wstring& day : readconf::weekdayFileNames()) {
            const std::wstring name = prefix + day + L".txt";
            out.push_back({ folder / name, name });
        }
    } else {
        const std::filesystem::path p = musical ? m_installation.gradesTxtPath()
                                                : m_installation.mapasTxtPath();
        const std::wstring fallback = musical ? L"Grade.txt" : L"Mapa.txt";
        out.push_back({ p, p.empty() ? fallback : p.filename().wstring() });
    }
    return out;
}

BlockFilePage* BlockEditorTab::currentPage() const
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

void BlockEditorTab::onPageChanged()
{
    updateStatus();
}

void BlockEditorTab::updateStatus()
{
    if (BlockFilePage* page = currentPage()) {
        m_fileLabel.setText(app::jstr(page->pathString()), juce::dontSendNotification);
    } else {
        m_fileLabel.setText(juce::String(), juce::dontSendNotification);
    }

    std::wstring status;
    if (hasUnsavedChanges()) {
        status = L"Alterações não salvas";
    }
    if (BlockFilePage* page = currentPage()) {
        if (!page->hasFileOnDisk()) {
            if (!status.empty()) {
                status += L"  |  ";
            }
            status += L"Arquivo ainda não existe no disco (será criado ao salvar)";
        }
    }
    if (!m_catalogueWarning.empty()) {
        if (!status.empty()) {
            status += L"  |  ";
        }
        status += m_catalogueWarning;
    }
    m_statusLabel.setText(app::jstr(status), juce::dontSendNotification);
    updateModeIcons();
}

void BlockEditorTab::setStatus(const std::wstring& text)
{
    m_statusLabel.setText(app::jstr(text), juce::dontSendNotification);
}

void BlockEditorTab::setCurrentPageMode(BlockFilePage::Mode mode)
{
    if (BlockFilePage* page = currentPage()) {
        page->setMode(mode);
    }
    updateModeIcons();
}

void BlockEditorTab::updateModeIcons()
{
    const BlockFilePage* page = currentPage();
    const bool visual = (page == nullptr) || page->mode() == BlockFilePage::Mode::Visual;
    m_visualBtn.setToggleState(visual, juce::dontSendNotification);
    m_textBtn.setToggleState(!visual, juce::dontSendNotification);
}

void BlockEditorTab::saveCurrentPage()
{
    saveCurrentFile();
}

void BlockEditorTab::discardCurrentPage()
{
    if (BlockFilePage* page = currentPage()) {
        page->reload();
        updateStatus();
    }
}

void BlockEditorTab::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
    if (!m_dividerBar.isEmpty()) {
        g.setColour(juce::Colour(0xff555555));
        g.fillRect(m_dividerBar);
    }
}

void BlockEditorTab::resized()
{
    const int margin = 6;
    const int bottomH = 24;
    const auto area = getLocalBounds();

    // Topo — só as abas de arquivo.
    const int tabsTop = area.getY() + 2;
    const int tabsH = juce::jmax(0, area.getHeight() - 2 - bottomH - margin);
    m_fileTabs.setBounds(margin, tabsTop, area.getWidth() - 2 * margin, tabsH);

    // Rodapé — <caminho...status> │ [Visual] [Texto] [Descartar] [Salvar]
    const int bottomY = area.getBottom() - bottomH - margin;
    const int gap = 6;
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

    const int divX = m_visualBtn.getX() - 10;
    m_dividerBar.setBounds(divX, bottomY + 2, 2, bottomH - 4);

    const int statusW = 300;
    const int statusX = divX - gap - statusW;
    m_statusLabel.setBounds(statusX, bottomY - 1, statusW, bottomH);
    m_fileLabel.setBounds(area.getX() + margin, bottomY - 1,
                          juce::jmax(0, statusX - gap - area.getX() - margin),
                          bottomH);
}

} // namespace app
