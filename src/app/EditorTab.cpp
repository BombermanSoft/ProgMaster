#include "app/EditorTab.h"

#include <ctime>

#include "app/JuceHelpers.h"
#include "app/PlaylistConfigController.h"
#include "core/Log.h"
#include "playlist/PlaylistInstallation.h"
#include "readconf/PlaylistFileLocator.h"
#include "readconf/ReadingConfiguration.h"

namespace {

// Dia da semana corrente na grafia dos arquivos do Playlist ("Seg".."Dom").
std::wstring currentWeekdayName()
{
    const std::time_t now = std::time(nullptr);
    std::tm t{};
#if defined(_MSC_VER)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    const wchar_t* names[] = { L"Dom", L"Seg", L"Ter", L"Qua", L"Qui", L"Sex",
                               L"Sab" };
    return names[t.tm_wday];
}

// Arquivo do relógio a abrir: segue a configuração atual (Único ->
// Relogio.txt; Semanal -> Relogio<dia de hoje>.txt). Se a seção não existir
// ou estiver desconhecida, abre o arquivo único.
std::filesystem::path resolvedRelogioPath(
    const app::PlaylistConfigController& controller,
    bool comercial, const std::filesystem::path& installRoot)
{
    const readconf::ConfigScope scope =
        comercial ? readconf::ConfigScope::RelogioComercial
                  : readconf::ConfigScope::RelogioMusical;
    const readconf::ScopeSnapshot snap =
        readconf::readScope(controller.document(), scope, installRoot);

    const std::filesystem::path folder =
        readconf::folderFor(scope, installRoot);

    std::wstring name = L"Relogio.txt";
    if (snap.option == readconf::FormatOption::Weekly) {
        name = L"Relogio" + currentWeekdayName() + L".txt";
    }
    return folder / name;
}

} // namespace

EditorTab::EditorTab(app::PlaylistConfigController& controller,
                     PlaylistInstallation& installation)
    : m_controller(controller), m_installation(installation)
{
    m_editor.setMultiLine(true);
    m_editor.setScrollbarsShown(true);
    m_editor.setCaretVisible(true);
    m_editor.setPopupMenuEnabled(true);
    m_editor.setFont(juce::Font(juce::FontOptions(
        juce::Font::getDefaultMonospacedFontName(), juce::Font::getDefaultStyle(),
        15.0f)));
    m_editor.onTextChange = [this] {
        if (!m_updatingUi) {
            setDirty(true);
        }
        updateStatus();
    };
    addAndMakeVisible(m_editor);

    m_saveButton.onClick = [this] { saveCurrentFile(); };
    m_undoButton.onClick = [this] { m_editor.undo(); };
    m_redoButton.onClick = [this] { m_editor.redo(); };
    addAndMakeVisible(m_saveButton);
    addAndMakeVisible(m_undoButton);
    addAndMakeVisible(m_redoButton);

    m_fileLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_fileLabel);
    m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_statusLabel);
}

void EditorTab::openFile(FileKind file)
{
    m_current = file;
    m_updatingUi = true;
    loadIntoEditor();
    m_updatingUi = false;
    setDirty(false);
    updateButtons();
    updateStatus();
}

void EditorTab::reopenFromDisk()
{
    if (m_current == FileKind::PlaylistIni) {
        // Playlist.ini vive no controlador: descartar = recarregar do disco.
        m_controller.load();
    }
    m_updatingUi = true;
    loadIntoEditor();
    m_updatingUi = false;
    setDirty(false);
    updateButtons();
    updateStatus();
}

void EditorTab::loadIntoEditor()
{
    m_hasFile = false;
    std::wstring text;
    std::wstring display;

    switch (m_current) {
    case FileKind::PlaylistIni:
        text = m_controller.currentText();
        display = L"playlist.ini (em memória — Salvar grava via Configuração)";
        m_hasFile = true;
        break;
    case FileKind::MapasComercial: {
        std::wstring msg;
        std::string err;
        m_mapas.setPath(m_installation.mapasTxtPath());
        m_mapas.setDisplayName(L"Mapa Comercial");
        m_hasFile = m_mapas.load(text, msg, err);
        if (!m_hasFile) {
            text = msg.empty() ? L"" : msg;
        }
        display = L"Mapa Comercial (mapas\\Mapas.txt)";
        break;
    }
    case FileKind::GradesMusicais: {
        std::wstring msg;
        std::string err;
        m_grades.setPath(m_installation.gradesTxtPath());
        m_grades.setDisplayName(L"Grades Musicais");
        m_hasFile = m_grades.load(text, msg, err);
        if (!m_hasFile) {
            text = msg.empty() ? L"" : msg;
        }
        display = L"Grades Musicais (grades\\Grades.txt)";
        break;
    }
    case FileKind::RelogioComercial:
    case FileKind::RelogioMusical: {
        const bool comercial = (m_current == FileKind::RelogioComercial);
        PlaylistIni& service = comercial ? m_relogioComercial : m_relogioMusical;
        service.setPath(resolvedRelogioPath(
            m_controller, comercial,
            m_controller.path().empty() ? std::filesystem::path()
                                        : m_controller.path().parent_path()));
        service.setDisplayName(comercial ? L"Relógio Comercial"
                                         : L"Relógio Musical");
        std::wstring msg;
        std::string err;
        m_hasFile = service.load(text, msg, err);
        if (!m_hasFile) {
            text = msg.empty() ? L"" : msg;
        }
        display = comercial ? L"Relógio Comercial (Relogio.txt)"
                            : L"Relógio Musical (Relogio.txt)";
        break;
    }
    }

    m_updatingUi = true;
    m_editor.setText(app::jstr(text), false);
    m_updatingUi = false;
    m_fileLabel.setText(app::jstr(display), juce::dontSendNotification);
}

bool EditorTab::saveCurrentFile()
{
    if (!m_hasFile) {
        return false;
    }

    const std::wstring text = app::wstr(m_editor.getText());
    std::wstring msg;
    std::string err;
    bool ok = true;

    switch (m_current) {
    case FileKind::PlaylistIni:
        ok = m_controller.save(msg, err);
        break;
    case FileKind::MapasComercial:
        ok = m_mapas.save(text, msg, err);
        break;
    case FileKind::GradesMusicais:
        ok = m_grades.save(text, msg, err);
        break;
    case FileKind::RelogioComercial:
        ok = m_relogioComercial.save(text, msg, err);
        break;
    case FileKind::RelogioMusical:
        ok = m_relogioMusical.save(text, msg, err);
        break;
    }

    if (ok) {
        setDirty(false);
    }
    juce::AlertWindow::showMessageBoxAsync(
        ok ? juce::MessageBoxIconType::InfoIcon
           : juce::MessageBoxIconType::WarningIcon,
        ok ? L"Salvo" : L"Não foi possível salvar",
        ok ? app::jstr(L"O arquivo foi gravado.") : app::jstr(msg));
    return ok;
}

bool EditorTab::hasUnsavedChanges() const
{
    return m_dirty;
}

std::wstring EditorTab::currentFileName() const
{
    switch (m_current) {
    case FileKind::PlaylistIni:      return L"playlist.ini";
    case FileKind::MapasComercial:   return L"Mapas.txt";
    case FileKind::GradesMusicais:   return L"Grades.txt";
    case FileKind::RelogioComercial: return L"Relógio Comercial";
    case FileKind::RelogioMusical:   return L"Relógio Musical";
    }
    return L"";
}

void EditorTab::setDirty(bool dirty)
{
    m_dirty = dirty;
    updateButtons();
    updateStatus();
}

void EditorTab::updateButtons()
{
    m_saveButton.setEnabled(m_hasFile);
    m_undoButton.setEnabled(true);
    m_redoButton.setEnabled(true);
}

void EditorTab::updateStatus()
{
    std::wstring status;
    if (m_dirty) {
        status = L"Alterações não salvas";
    }
    if (!m_hasFile) {
        status += status.empty() ? L"" : L"  |  ";
        status += L"Arquivo não encontrado no disco (pode ser criado ao salvar)";
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

    m_editor.setBounds(area.getX() + margin, y + topH + 4,
                       area.getWidth() - 2 * margin,
                       juce::jmax(0, area.getBottom() - (y + topH + 4) - margin));
}

void EditorTab::visibilityChanged()
{
    juce::Component::visibilityChanged();

    if (m_current != FileKind::PlaylistIni) {
        return;
    }

    if (isVisible()) {
        // Voltou para a aba: puxa o estado mais recente do controlador.
        m_updatingUi = true;
        m_editor.setText(app::jstr(m_controller.currentText()), false);
        m_updatingUi = false;
        setDirty(false);
    } else {
        // Sendo ocultada: devolve o texto ao controlador (sincronização).
        if (!m_updatingUi) {
            const std::wstring text = app::wstr(m_editor.getText());
            if (text != m_controller.currentText()) {
                m_controller.setTextFromEditor(text);
            }
            setDirty(false);
        }
    }
}