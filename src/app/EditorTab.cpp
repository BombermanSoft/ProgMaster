#include "app/EditorTab.h"

#include "app/JuceHelpers.h"
#include "app/PlaylistConfigController.h"
#include "playlist/PlaylistInstallation.h"
#include "readconf/PlaylistFileLocator.h"
#include "readconf/ReadingConfiguration.h"

namespace {

// Jogo de caracteres mono do editor (15px, como nas versões anteriores).
juce::Font editorFont()
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                        juce::Font::getDefaultStyle(), 15.0f));
}

} // namespace

// ============================================================================
// FilePage: uma sub-aba do editor = um arquivo de texto (ou o playlist.ini)
// ============================================================================
class EditorTab::FilePage final : public juce::Component {
public:
    FilePage(EditorTab& host, std::filesystem::path path, std::wstring displayName,
             bool isIni)
        : m_host(host), m_service(std::move(path)),
          m_displayName(std::move(displayName)), m_isIni(isIni)
    {
        m_service.setDisplayName(m_displayName);

        m_editor.setMultiLine(true);
        m_editor.setScrollbarsShown(true);
        m_editor.setCaretVisible(true);
        m_editor.setPopupMenuEnabled(true);
        m_editor.setReturnKeyStartsNewLine(true);
        m_editor.setTabKeyUsedAsCharacter(true);
        m_editor.setFont(editorFont());
        m_editor.onTextChange = [this] { m_host.onPageTextChanged(*this); };
        m_editor.setTooltip(m_isIni ? juce::String()
                                    : app::jstr(m_service.path().wstring()));
        addAndMakeVisible(m_editor);

        // Carrega do disco. O playlist.ini é preenchido pelo host (documento
        // em memória do controlador), então nada é lido aqui para ele.
        if (!m_isIni) {
            reload();
        } else {
            setDirty(false);
        }
    }

    // Recarrega do disco (Descartar alterações). Para arquivo inexistente,
    // deixa o editor vazio para o usuário criar o arquivo ao salvar.
    void reload()
    {
        std::wstring text;
        if (!m_isIni) {
            std::wstring msg;
            std::string err;
            m_hasFileOnDisk = m_service.load(text, msg, err);
            if (!m_hasFileOnDisk) {
                text.clear();
            }
        }
        setText(text);
        setDirty(false);
    }

    bool isIni() const { return m_isIni; }
    bool hasFileOnDisk() const { return m_isIni || m_hasFileOnDisk; }
    const std::wstring& displayName() const { return m_displayName; }
    std::wstring pathString() const
    {
        if (m_isIni) {
            return L"playlist.ini (em memória — Salvar grava via Configuração)";
        }
        return m_service.path().wstring();
    }

    juce::TextEditor& editor() { return m_editor; }
    const juce::TextEditor& editor() const { return m_editor; }

    void setText(const std::wstring& text)
    {
        m_editor.setText(app::jstr(text), false);
    }
    std::wstring text() const { return app::wstr(m_editor.getText()); }

    bool isDirty() const { return m_dirty; }
    void setDirty(bool dirty) { m_dirty = dirty; }
    void markTextChangedByUser() { m_dirty = true; }

    // Grava o conteúdo na codificação original (arquivo inexistente é criado).
    bool save(std::wstring& userMessage, std::string& technicalError)
    {
        if (m_isIni) {
            return false;
        }
        const bool ok = m_service.save(text(), userMessage, technicalError);
        if (ok) {
            m_dirty = false;
            m_hasFileOnDisk = true;
        }
        return ok;
    }

    void resized() override
    {
        m_editor.setBounds(getLocalBounds());
    }

private:
    EditorTab& m_host;
    PlaylistIni m_service;
    std::wstring m_displayName;
    bool m_isIni = false;
    bool m_hasFileOnDisk = false;
    bool m_dirty = false;
    juce::TextEditor m_editor;
};

namespace {

// Rótulo amigável de cada item do menu Editar (sem nomes de arquivo — Bug 7).
std::wstring kindDisplayName(EditorTab::FileKind file)
{
    switch (file) {
    case EditorTab::FileKind::PlaylistIni:      return L"playlist.ini";
    case EditorTab::FileKind::MapasComercial:   return L"Mapa Comercial";
    case EditorTab::FileKind::GradesMusicais:   return L"Grades Musicais";
    case EditorTab::FileKind::RelogioComercial: return L"Relógio Comercial";
    case EditorTab::FileKind::RelogioMusical:   return L"Relógio Musical";
    }
    return L"";
}

} // namespace

// ============================================================================

EditorTab::EditorTab(app::PlaylistConfigController& controller,
                     PlaylistInstallation& installation)
    : m_controller(controller), m_installation(installation)
{
    m_saveButton.onClick = [this] { saveCurrentFile(); };
    m_undoButton.onClick = [this] {
        if (FilePage* page = currentPage()) {
            page->editor().undo();
        }
    };
    m_redoButton.onClick = [this] {
        if (FilePage* page = currentPage()) {
            page->editor().redo();
        }
    };
    addAndMakeVisible(m_saveButton);
    addAndMakeVisible(m_undoButton);
    addAndMakeVisible(m_redoButton);

    m_fileTabs.onTabChanged = [this] { updateButtons(); updateStatus(); };
    addAndMakeVisible(m_fileTabs);

    m_fileLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_fileLabel);
    m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_statusLabel);
}

EditorTab::~EditorTab() = default;

void EditorTab::openFile(FileKind file)
{
    m_current = file;
    rebuildPages();

    // Abre a primeira sub-aba, já sincronizando o playlist.ini (se for o caso).
    if (!m_pages.empty()) {
        m_fileTabs.setCurrentTabIndex(0, false);
        syncIniPageFromController();
    }
    updateButtons();
    updateStatus();
}

void EditorTab::reopenFromDisk()
{
    if (m_current == FileKind::PlaylistIni) {
        m_controller.load();
    }
    for (auto& page : m_pages) {
        if (page->isIni()) {
            page->setText(m_controller.currentText());
            page->setDirty(false);
        } else {
            page->reload();
        }
    }
    updateButtons();
    updateStatus();
}

void EditorTab::rebuildPages()
{
    m_pages.clear();
    m_fileTabs.clearTabs();

    for (const Spec& spec : resolveSpecs(m_current)) {
        auto page = std::make_unique<FilePage>(*this, spec.path, spec.displayName,
                                               spec.isIni);
        m_fileTabs.addTab(app::jstr(spec.displayName),
                          juce::Colour(0xff2b2b2b), page.get(), false);
        m_pages.push_back(std::move(page));
    }
}

std::vector<EditorTab::Spec> EditorTab::resolveSpecs(FileKind file) const
{
    std::vector<Spec> out;
    const std::filesystem::path installFolder = m_controller.installationFolder();

    switch (file) {
    case FileKind::PlaylistIni:
        out.push_back({ {}, L"playlist.ini", /*isIni=*/true });
        break;

    case FileKind::MapasComercial: {
        const readconf::ScopeSnapshot snap = readconf::readScope(
            m_controller.document(), readconf::ConfigScope::Comercial, installFolder);
        if (snap.present && snap.option == readconf::FormatOption::Weekly) {
            const std::filesystem::path folder =
                readconf::folderFor(readconf::ConfigScope::Comercial, installFolder);
            for (const std::wstring& day : readconf::weekdayFileNames()) {
                const std::wstring name = L"Mapa" + day + L".txt";
                out.push_back({ folder / name, name, /*isIni=*/false });
            }
        } else {
            const std::filesystem::path p = m_installation.mapasTxtPath();
            out.push_back({ p, p.empty() ? L"Mapa.txt" : p.filename().wstring(),
                            /*isIni=*/false });
        }
        break;
    }

    case FileKind::GradesMusicais: {
        const readconf::ScopeSnapshot snap = readconf::readScope(
            m_controller.document(), readconf::ConfigScope::Musical, installFolder);
        if (snap.present && snap.option == readconf::FormatOption::Weekly) {
            const std::filesystem::path folder =
                readconf::folderFor(readconf::ConfigScope::Musical, installFolder);
            for (const std::wstring& day : readconf::weekdayFileNames()) {
                const std::wstring name = L"Grade" + day + L".txt";
                out.push_back({ folder / name, name, /*isIni=*/false });
            }
        } else {
            const std::filesystem::path p = m_installation.gradesTxtPath();
            out.push_back({ p, p.empty() ? L"Grade.txt" : p.filename().wstring(),
                            /*isIni=*/false });
        }
        break;
    }

    case FileKind::RelogioComercial:
    case FileKind::RelogioMusical: {
        const bool comercial = (file == FileKind::RelogioComercial);
        const readconf::ConfigScope scope =
            comercial ? readconf::ConfigScope::RelogioComercial
                      : readconf::ConfigScope::RelogioMusical;
        const readconf::ScopeSnapshot snap =
            readconf::readScope(m_controller.document(), scope, installFolder);
        if (snap.present && snap.option == readconf::FormatOption::Weekly) {
            const std::filesystem::path folder = readconf::folderFor(scope, installFolder);
            for (const std::wstring& day : readconf::weekdayFileNames()) {
                const std::wstring name = L"Relogio" + day + L".txt";
                out.push_back({ folder / name, name, /*isIni=*/false });
            }
        } else {
            const std::filesystem::path folder = readconf::folderFor(scope, installFolder);
            const std::wstring name = L"Relogio.txt";
            out.push_back({ folder / name, name, /*isIni=*/false });
        }
        break;
    }
    }
    return out;
}

EditorTab::FilePage* EditorTab::currentPage() const
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

EditorTab::FilePage* EditorTab::iniPage() const
{
    for (const auto& page : m_pages) {
        if (page->isIni()) {
            return page.get();
        }
    }
    return nullptr;
}

void EditorTab::syncIniPageFromController()
{
    if (FilePage* page = iniPage()) {
        page->setText(m_controller.currentText());
        page->setDirty(false);
    }
}

void EditorTab::refreshIniFromController()
{
    syncIniPageFromController();
}

bool EditorTab::saveCurrentFile()
{
    FilePage* page = currentPage();
    if (page == nullptr) {
        return false;
    }

    std::wstring msg;
    std::string err;
    bool ok = true;

    if (page->isIni()) {
        // Comunica o texto digitado ao controlador (documento) antes de salvar.
        const std::wstring text = page->text();
        if (text != m_controller.currentText()) {
            m_controller.setTextFromEditor(text);
        }
        ok = m_controller.save(msg, err);
        if (ok) {
            page->setDirty(false);
        }
    } else {
        ok = page->save(msg, err);
    }

    updateButtons();
    updateStatus();

    juce::AlertWindow::showMessageBoxAsync(
        ok ? juce::MessageBoxIconType::InfoIcon
           : juce::MessageBoxIconType::WarningIcon,
        ok ? L"Salvo" : L"Não foi possível salvar",
        ok ? app::jstr(L"O arquivo foi gravado.") : app::jstr(msg));
    return ok;
}

bool EditorTab::hasUnsavedChanges() const
{
    if (m_controller.isDirty()) {
        return true;
    }
    for (const auto& page : m_pages) {
        if (page->isDirty()) {
            return true;
        }
    }
    return false;
}

std::wstring EditorTab::currentFileName() const
{
    return kindDisplayName(m_current);
}

void EditorTab::onPageTextChanged(FilePage& page)
{
    page.markTextChangedByUser();
    // Mantém o controlador sincronizado apenas para o playlist.ini
    // (o documento é reinterpretado ao voltar para a interface).
    if (&page == currentPage()) {
        if (page.isIni()) {
            m_controller.setTextFromEditor(page.text());
        }
        updateButtons();
        updateStatus();
    }
}

void EditorTab::setDirty(bool /*dirty*/)
{
    // O estado "sujo" agora é por página; mantemos o método para compatibilidade
    // interna (o estado real é consultado em hasUnsavedChanges()).
    updateButtons();
    updateStatus();
}

void EditorTab::updateButtons()
{
    const bool enabled = currentPage() != nullptr;
    m_saveButton.setEnabled(enabled);
    m_undoButton.setEnabled(enabled);
    m_redoButton.setEnabled(enabled);
}

void EditorTab::updateStatus()
{
    if (FilePage* page = currentPage()) {
        m_fileLabel.setText(app::jstr(page->pathString()), juce::dontSendNotification);
    }

    std::wstring status;
    if (hasUnsavedChanges()) {
        status = L"Alterações não salvas";
    }
    if (FilePage* page = currentPage()) {
        if (!page->isIni() && !page->hasFileOnDisk()) {
            if (!status.empty()) {
                status += L"  |  ";
            }
            status += L"Arquivo ainda não existe no disco (será criado ao salvar)";
        }
    }
    m_statusLabel.setText(app::jstr(status), juce::dontSendNotification);
}

void EditorTab::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void EditorTab::resized()
{
    const int margin = 6;
    const int labelH = 20;
    const int topH = 30;
    const auto area = getLocalBounds();

    m_fileLabel.setBounds(area.getX() + margin, area.getY() + 2,
                          area.getWidth() - 2 * margin, labelH);

    const int y = area.getY() + labelH + 2;
    m_saveButton.setBounds(area.getX() + margin, y, 70, topH);
    m_undoButton.setBounds(m_saveButton.getRight() + 4, y, 84, topH);
    m_redoButton.setBounds(m_undoButton.getRight() + 4, y, 84, topH);
    m_statusLabel.setBounds(m_redoButton.getRight() + 12, y + 4,
                            juce::jmax(0, area.getRight() - margin - m_redoButton.getRight() - 12),
                            labelH);

    m_fileTabs.setBounds(area.getX() + margin, y + topH + 4,
                         area.getWidth() - 2 * margin,
                         juce::jmax(0, area.getBottom() - (y + topH + 4) - margin));
}

void EditorTab::visibilityChanged()
{
    juce::Component::visibilityChanged();

    if (m_current != FileKind::PlaylistIni) {
        return;
    }
    FilePage* page = iniPage();
    if (page == nullptr) {
        return;
    }

    if (isVisible()) {
        // Voltou para a aba: se o usuário editou (página suja),
        // sincroniza com o controlador; senão, puxa o estado
        // mais recente do controlador (evita perder edições).
        if (page->isDirty()) {
            m_controller.setTextFromEditor(page->text());
        } else {
            page->setText(m_controller.currentText());
        }
    } else {
        // Sendo ocultada: devolve o texto ao controlador e marca
        // a página como sincronizada.
        const std::wstring text = page->text();
        if (text != m_controller.currentText()) {
            m_controller.setTextFromEditor(text);
        }
        page->setDirty(false);
    }
    updateStatus();
}