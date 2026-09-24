#include "app/ConfiguratorTab.h"

#include <algorithm>
#include <utility>

#include "app/JuceHelpers.h"
#include "app/PlaylistConfigController.h"

namespace app {

// ============================================================================
// Cartão de um escopo com formato ([BLOCO COMERCIAL] etc.)
// ============================================================================
class ScopeCard final : public juce::GroupComponent {
public:
    ScopeCard(ConfiguratorTab& host, readconf::ConfigScope scope)
        : m_host(host), m_scope(scope)
    {
        setText(jstr(readconf::scopeDisplayName(scope)));

        m_status.setColour(juce::Label::textColourId, juce::Colours::white);
        m_status.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        addAndMakeVisible(m_status);

        m_formatoLabel.setText("Formato:", juce::dontSendNotification);
        m_formatoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(m_formatoLabel);
        addAndMakeVisible(m_formato);

        m_tokenLabel.setColour(juce::Label::textColourId, juce::Colours::lightsteelblue);
        m_tokenLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        addAndMakeVisible(m_tokenLabel);

        m_filesLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        m_filesLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        m_filesLabel.setJustificationType(juce::Justification::topLeft);
        m_filesLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(m_filesLabel);

        m_addButton.setButtonText("+ Adicionar estilo");
        m_addButton.onClick = [this] { m_host.onAddScope(m_scope); };
        addAndMakeVisible(m_addButton);

        m_formato.onChange = [this] {
            m_host.onOptionChanged(m_scope, optionForId(m_formato.getSelectedId()));
        };
    }

    void refresh(const readconf::ScopeSnapshot& snap)
    {
        m_snap = snap;

        if (snap.present) {
            m_status.setColour(juce::Label::textColourId,
                               snap.option == readconf::FormatOption::Unknown
                                   ? juce::Colours::orange
                                   : juce::Colours::lime);
            m_status.setText(snap.option == readconf::FormatOption::Unknown
                                 ? "Configuração presente (não reconhecida —"
                                   " preservada no salvamento)"
                                 : "Configurado",
                             juce::dontSendNotification);
        } else {
            m_status.setColour(juce::Label::textColourId, juce::Colours::tomato);
            m_status.setText("Não configurado (seção ausente)", juce::dontSendNotification);
        }

        buildCombo();

        m_showToken = snap.present && !snap.arquivoAsWritten.empty();
        if (m_showToken) {
            std::wstring tokenText = L"Arquivo identificado: " + snap.arquivoAsWritten;
            if (!snap.matchedToken.empty()) {
                tokenText += L"   (token reconhecido: " + snap.matchedToken + L")";
            }
            m_tokenLabel.setText(jstr(tokenText), juce::dontSendNotification);
        } else {
            m_tokenLabel.setText("", juce::dontSendNotification);
        }

        buildFilesLabel();
        resized();
    }

    readconf::ConfigScope scope() const { return m_scope; }

    void resized() override
    {
        const int margin = 8;
        const int w = getWidth();
        const int statusH = 22;
        const int rowH = 26;

        m_status.setBounds(margin, 18, w - 2 * margin, statusH);

        m_formatoLabel.setBounds(margin, 18 + statusH + 6, w - 2 * margin, rowH);

        if (m_snap.present) {
            m_formato.setVisible(true);
            m_addButton.setVisible(false);
            m_formato.setBounds(margin, 18 + statusH + 6 + rowH, w - 2 * margin, 24);
        } else {
            m_formato.setVisible(false);
            m_addButton.setVisible(true);
            m_addButton.setBounds(90, 18 + statusH + 12, 170, 24);
        }

        if (m_showToken) {
            m_tokenLabel.setVisible(true);
            m_tokenLabel.setBounds(margin, 108, w - 2 * margin, 18);
        } else {
            m_tokenLabel.setVisible(false);
        }

        m_filesLabel.setBounds(margin, m_showToken ? 128 : 108,
                               w - 2 * margin, getHeight() - (m_showToken ? 134 : 114));
    }

    // ------------------------------------------------------------------
private:
    static int optionId(readconf::FormatOption option)
    {
        // ids positivos e estáveis para o ComboBox.
        return static_cast<int>(option) + 1;
    }

    readconf::FormatOption optionForId(int id) const
    {
        for (const auto& pair : m_optIds) {
            if (pair.first == id) {
                return pair.second;
            }
        }
        return readconf::FormatOption::Unknown;
    }

    void buildCombo()
    {
        m_optIds.clear();
        m_formato.clear(juce::dontSendNotification);
        const auto options = readconf::optionsForFormat(m_scope);
        for (readconf::FormatOption opt : options) {
            const int id = optionId(opt);
            m_formato.addItem(jstr(readconf::displayName(opt)), id);
            m_optIds.emplace_back(id, opt);
        }

        if (m_snap.present) {
            const int currentId = optionId(m_snap.option);
            m_formato.setSelectedId(currentId, juce::dontSendNotification);
        } else {
            m_formato.setSelectedId(0, juce::dontSendNotification);
        }
    }

    void buildFilesLabel()
    {
        std::wstring textOut;
        if (m_snap.present && m_snap.option != readconf::FormatOption::Unknown) {
            static const std::wstring folderHint =
                (readconf::ConfigScope::Comercial == m_scope ||
                 readconf::ConfigScope::RelogioComercial == m_scope)
                    ? L"pasta dos mapas"
                    : L"pasta das grades";
            textOut = L"Arquivos esperados (" + folderHint + L"):\n";
            if (m_snap.files.empty()) {
                textOut += L"  (nenhum arquivo esperado para esta configuração)";
            } else {
                for (const auto& f : m_snap.files) {
                    textOut += (f.exists ? L"  \u2713 " : L"  \u2717 ") + f.fileName;
                    if (!f.exists) {
                        textOut += L"  [não encontrado ainda]";
                    }
                    textOut += L"\n";
                }
            }
        } else if (m_snap.present) {
            textOut = L"Configuração atual não reconhecida: escolha um formato acima "
                      L"para normalizar (o texto original é preservado até Salvar).";
        } else {
            textOut = L"A execução usa a configuração apontada em " +
                      m_snap.arquivoAsWritten +
                      L" (seção ausente). Escolha um estilo acima.";
            if (!m_snap.arquivoAsWritten.empty()) {
                textOut += L"\n(linha ARQUIVO lida: " + m_snap.arquivoAsWritten + L")";
            }
        }
        m_filesLabel.setText(jstr(textOut), juce::dontSendNotification);
    }

    ConfiguratorTab& m_host;
    readconf::ConfigScope m_scope;
    readconf::ScopeSnapshot m_snap;
    bool m_showToken = false;

    juce::Label m_status;
    juce::Label m_formatoLabel;
    juce::ComboBox m_formato;
    juce::Label m_tokenLabel;
    juce::Label m_filesLabel;
    juce::TextButton m_addButton;
    std::vector<std::pair<int, readconf::FormatOption>> m_optIds;
};

// ============================================================================
// Linha de afiliada (endereço | porta | ativa | remover)
// ============================================================================
class AfiliadaRow final : public juce::Component {
public:
    AfiliadaRow(ConfiguratorTab& host, size_t position,
                const std::wstring& address, const std::wstring& port,
                bool active)
        : m_host(host), m_position(position)
    {
        m_address.setTooltip("Endereço da afiliada (ex.: 192.168.0.50).");
        m_address.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3c3c3c));
        m_address.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        m_address.setText(jstr(address), juce::dontSendNotification);
        m_address.onTextChange = [this] { m_host.onRowChanged(m_position); };
        addAndMakeVisible(m_address);

        m_port.setTooltip("Porta (1–65535); validada ao Salvar.");
        m_port.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3c3c3c));
        m_port.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        m_port.setText(jstr(port), juce::dontSendNotification);
        m_port.onTextChange = [this] { m_host.onRowChanged(m_position); };
        addAndMakeVisible(m_port);

        m_ativa.setButtonText("Ativa");
        m_ativa.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
        m_ativa.setToggleState(active, juce::dontSendNotification);
        m_ativa.onStateChange = [this] { m_host.onRowChanged(m_position); };
        addAndMakeVisible(m_ativa);

        m_remove.setButtonText("Remover");
        m_remove.onClick = [this] { m_host.onRemoveAfiliada(m_position); };
        addAndMakeVisible(m_remove);
    }

    void resized() override
    {
        const int margin = 4;
        const int h = getHeight();
        const int portW = 74;
        const int ativaW = 64;
        const int removeW = 86;

        m_remove.setBounds(getWidth() - removeW - margin, 0, removeW, h);
        m_ativa.setBounds(getWidth() - removeW - margin - ativaW - 8, 0, ativaW, h);
        m_port.setBounds(getWidth() - removeW - margin - ativaW - 8 - portW - 8, 0,
                         portW, h);
        m_address.setBounds(margin, 0,
                            getWidth() - removeW - margin - ativaW - 8 - portW - 8 - margin - 8,
                            h);
    }

    juce::String addressText() const { return m_address.getText(); }
    juce::String portText() const { return m_port.getText(); }
    bool active() const { return m_ativa.getToggleState(); }

private:
    ConfiguratorTab& m_host;
    size_t m_position = 0;
    juce::TextEditor m_address;
    juce::TextEditor m_port;
    juce::ToggleButton m_ativa;
    juce::TextButton m_remove;
};

// ============================================================================
// Cartão das afiliadas ([AFILIADAS])
// ============================================================================
class AfiliadasCard final : public juce::GroupComponent {
public:
    explicit AfiliadasCard(ConfiguratorTab& host)
        : m_host(host)
    {
        setText("AFILIADAS");

        m_info.setColour(juce::Label::textColourId, juce::Colours::orange);
        m_info.setFont(juce::Font(juce::FontOptions(12.0f)));
        addAndMakeVisible(m_info);

        m_addSection.setButtonText("Adicionar afiliadas");
        m_addSection.onClick = [this] { m_host.onAddAfiliadaSection(); };
        addAndMakeVisible(m_addSection);

        m_addRow.setButtonText("+ Adicionar afiliada");
        m_addRow.onClick = [this] { m_host.onAddAfiliada(); };
        addAndMakeVisible(m_addRow);
    }

    void refresh()
    {
        const auto& doc = m_host.controller().document();
        const bool hasSection =
            doc.sectionIndex(readconf::ConfigScope::Afiliadas) != -1;
        const std::vector<readconf::PlaylistIniDocument::Afiliada> list =
            m_host.controller().afiliadas();

        m_rows.clear();
        if (hasSection) {
            m_info.setColour(juce::Label::textColourId, juce::Colours::lime);
            m_info.setText(jstr(L"Seção [AFILIADAS] presente — " +
                                   std::to_wstring(list.size()) +
                                   (list.size() == 1 ? L" registro."
                                                     : L" registros.")),
                           juce::dontSendNotification);
            m_addSection.setVisible(false);
            m_addRow.setVisible(true);
            for (size_t i = 0; i < list.size(); ++i) {
                auto row = std::make_unique<AfiliadaRow>(
                    m_host, i, list[i].address, list[i].portText, !list[i].disabled);
                addAndMakeVisible(*row);
                m_rows.push_back(std::move(row));
            }
        } else {
            m_info.setColour(juce::Label::textColourId, juce::Colours::orange);
            m_info.setText("A seção [AFILIADAS] não existe no playlist.ini.", juce::dontSendNotification);
            m_addSection.setVisible(true);
            m_addRow.setVisible(false);
        }
        resized();
    }

    int preferredHeight() const
    {
        const int base = 48;
        return base + static_cast<int>(m_rows.size()) * (28 + 4) + 8;
    }

    void resized() override
    {
        const int margin = 8;
        const int rowH = 28;
        m_info.setBounds(margin, 18, getWidth() - 2 * margin, 20);
        m_addSection.setBounds(60, 40, 180, 24);
        m_addRow.setBounds(margin, 40, 180, 24);

        int y = 42 + 28 + 4;
        for (auto& row : m_rows) {
            row->setBounds(margin, y, getWidth() - 2 * margin, rowH);
            y += rowH + 4;
        }
        setSize(getWidth(), preferredHeight());
    }

    // Fonte atual das linhas (para onRowChanged sincronizar com o controlador).
    const std::vector<std::unique_ptr<AfiliadaRow>>& rows() const { return m_rows; }

private:
    ConfiguratorTab& m_host;
    juce::Label m_info;
    juce::TextButton m_addSection;
    juce::TextButton m_addRow;
    std::vector<std::unique_ptr<AfiliadaRow>> m_rows;
};

// ============================================================================

ConfiguratorTab::ConfiguratorTab(PlaylistConfigController& controller)
    : m_controller(controller)
{
    m_headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    m_headerLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(m_headerLabel);

    m_dirtyLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(m_dirtyLabel);

    m_textModeButton.setButtonText("\u270E  Visualizar como texto");
    m_textModeButton.setTooltip(
        "Alterna esta guia entre a visualização estruturada e o bloco de "
        "notas texto do playlist.ini (mesmo documento em memória).");
    m_textModeButton.onStateChange = [this] {
        const bool toText = m_textModeButton.getToggleState();
        if (toText == m_textMode) {
            return;
        }
        if (!toText) {
            // Sair do modo texto: o conteúdo do editor vira o documento.
            m_controller.setTextFromEditor(wstr(m_textEditor.getText()));
        }
        m_textMode = toText;
        layoutContent();
        resized();
        updateDirtyLabel();
    };
    addAndMakeVisible(m_textModeButton);

    m_textEditor.setMultiLine(true);
    m_textEditor.setScrollbarsShown(true);
    m_textEditor.setCaretVisible(true);
    m_textEditor.setPopupMenuEnabled(true);
    m_textEditor.setFont(juce::Font(juce::FontOptions(
        juce::Font::getDefaultMonospacedFontName(), juce::Font::getDefaultStyle(),
        14.0f)));
    m_textEditor.onTextChange = [this] { updateDirtyLabel(); };
    addAndMakeVisible(m_textEditor);

    m_visualArea.setScrollBarsShown(true, false, false, false);
    m_visualArea.setViewedComponent(&m_content, false);
    addAndMakeVisible(m_visualArea);

    m_saveButton.onClick = [this] { savePlaylistIni(); };
    addAndMakeVisible(m_saveButton);
    m_discardButton.onClick = [this] { discardChanges(); };
    addAndMakeVisible(m_discardButton);

    refreshFromController();
}

void ConfiguratorTab::refreshFromController()
{
    m_controller.load();
    m_textMode = false;
    m_textModeButton.setToggleState(false, juce::dontSendNotification);
    rebuildAll();
    updateDirtyLabel();
    resized();
}

void ConfiguratorTab::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (isVisible() && !m_textMode) {
        // Documento pode ter mudado enquanto a guia estava oculta (pelo bloco
        // de notas — playlist.ini em memória). Re-sincroniza os cartões.
        rebuildAll();
        updateDirtyLabel();
    }
}

void ConfiguratorTab::onOptionChanged(readconf::ConfigScope scope,
                                      readconf::FormatOption option)
{
    if (option == readconf::FormatOption::Unknown) {
        return;
    }
    m_controller.applyOption(scope, option);
    refreshScopeCard(scope);
    updateDirtyLabel();
}

void ConfiguratorTab::onAddScope(readconf::ConfigScope scope)
{
    m_controller.addMissingConfiguration(scope);
    refreshScopeCard(scope);
    updateDirtyLabel();
}

void ConfiguratorTab::onRowChanged(size_t position)
{
    // Chamado a cada tecla/alternância numa linha de afiliada: sincroniza o
    // documento SEM reconstruir (evita perder o foco enquanto digita).
    if (m_afiliadasCard == nullptr || position >= m_afiliadasCard->rows().size()) {
        return;
    }
    const auto& row = m_afiliadasCard->rows()[position];
    m_controller.updateAfiliada(position, wstr(row->addressText()),
                                wstr(row->portText()), !row->active());
    updateDirtyLabel();
}

void ConfiguratorTab::onRemoveAfiliada(size_t position)
{
    m_controller.removeAfiliada(position);
    refreshAfiliadasOnly();
    updateDirtyLabel();
}

void ConfiguratorTab::onAddAfiliada()
{
    m_controller.addAfiliada(L"", L"", false);
    refreshAfiliadasOnly();
    updateDirtyLabel();
}

void ConfiguratorTab::onAddAfiliadaSection()
{
    m_controller.addAfiliada(L"", L"", false);
    refreshAfiliadasOnly();
    updateDirtyLabel();
}

void ConfiguratorTab::savePlaylistIni()
{
    if (m_textMode) {
        m_controller.setTextFromEditor(wstr(m_textEditor.getText()));
    }
    std::wstring userMessage;
    std::string technical;
    if (m_controller.save(userMessage, technical)) {
        m_controller.markClean();
        updateDirtyLabel();
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon, L"Salvo",
            L"playlist.ini foi gravado com sucesso.");
    } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon, L"Não foi possível salvar",
            jstr(userMessage));
    }
}

void ConfiguratorTab::discardChanges()
{
    m_controller.load();
    if (m_textMode) {
        m_textMode = false;
        m_textModeButton.setToggleState(false, juce::dontSendNotification);
    }
    rebuildAll();
    updateDirtyLabel();
}

void ConfiguratorTab::rebuildAll()
{
    m_cards.clear();
    m_scopeCards.clear();
    m_afiliadasCard = nullptr;

    const readconf::ConfigScope scopes[] = {
        readconf::ConfigScope::Comercial,
        readconf::ConfigScope::Musical,
        readconf::ConfigScope::RelogioComercial,
        readconf::ConfigScope::RelogioMusical,
    };
    for (auto scope : scopes) {
        const readconf::ScopeSnapshot snap =
            readconf::readScope(m_controller.document(), scope,
                                m_controller.installationFolder());
        auto card = std::make_unique<ScopeCard>(*this, scope);
        card->refresh(snap);
        m_content.addAndMakeVisible(*card);
        m_scopeCards.push_back(card.get());
        m_cards.push_back(std::move(card));
    }

    auto afiliadas = std::make_unique<AfiliadasCard>(*this);
    afiliadas->refresh();
    m_content.addAndMakeVisible(*afiliadas);
    m_afiliadasCard = afiliadas.get();
    m_cards.push_back(std::move(afiliadas));

    layoutContent();
}

void ConfiguratorTab::refreshScopeCard(readconf::ConfigScope scope)
{
    for (ScopeCard* card : m_scopeCards) {
        if (card->scope() == scope) {
            card->refresh(readconf::readScope(m_controller.document(), scope,
                                              m_controller.installationFolder()));
            return;
        }
    }
}

void ConfiguratorTab::refreshAfiliadasOnly()
{
    if (m_afiliadasCard != nullptr) {
        m_afiliadasCard->refresh();
        layoutContent();
    }
}

void ConfiguratorTab::layoutContent()
{
    if (m_textMode) {
        return;
    }

    const int margin = 6;
    const int contentWidth = juce::jmax(320, m_visualArea.getWidth() - 2 * margin);
    int y = margin;

    for (auto& card : m_cards) {
        const int cardH = (card.get() == static_cast<juce::Component*>(m_afiliadasCard))
                              ? preferredAfiliadasHeight()
                              : preferredScopeCardHeight();
        card->setBounds(margin, y, contentWidth - 2 * margin, cardH);
        y += cardH + margin;
    }
    m_content.setSize(juce::jmax(320, m_visualArea.getWidth()), juce::jmax(margin, y));
    m_visualArea.setScrollBarsShown(true, false, false, false);
}

void ConfiguratorTab::updateDirtyLabel()
{
    if (m_textMode) {
        m_dirtyLabel.setText("editando no bloco de notas (memória)",
                             juce::dontSendNotification);
        m_dirtyLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        return;
    }
    if (m_controller.isDirty()) {
        m_dirtyLabel.setText("alterações não salvas", juce::dontSendNotification);
        m_dirtyLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    } else {
        m_dirtyLabel.setText("documento em dia", juce::dontSendNotification);
        m_dirtyLabel.setColour(juce::Label::textColourId, juce::Colours::lime);
    }
}

int ConfiguratorTab::preferredScopeCardHeight()
{
    return 190;
}

int ConfiguratorTab::preferredAfiliadasHeight() const
{
    return m_afiliadasCard != nullptr ? m_afiliadasCard->preferredHeight() : 120;
}

void ConfiguratorTab::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void ConfiguratorTab::resized()
{
    const int margin = 6;
    const int headerH = 24;
    const int bottomH = 34;
    const auto area = getLocalBounds();

    m_headerLabel.setBounds(area.getX() + margin, area.getY() + 3,
                            area.getWidth() - 280, headerH);
    m_dirtyLabel.setBounds(area.getRight() - 260, area.getY() + 3, 120, headerH);
    m_textModeButton.setBounds(area.getRight() - 130, area.getY() + 3, 124, 22);

    const int top = area.getY() + headerH + 4;
    const int bottom = area.getBottom() - bottomH - margin;

    if (m_textMode) {
        m_textEditor.setVisible(true);
        m_visualArea.setVisible(false);
        m_textEditor.setBounds(area.getX() + margin, top,
                               area.getWidth() - 2 * margin, bottom - top);
    } else {
        m_textEditor.setVisible(false);
        m_visualArea.setVisible(true);
        m_visualArea.setBounds(area.getX() + margin, top,
                               area.getWidth() - 2 * margin, bottom - top);
    }

    m_discardButton.setBounds(area.getX() + margin, area.getBottom() - bottomH,
                              160, 28);
    m_saveButton.setBounds(area.getRight() - 170, area.getBottom() - bottomH,
                           164, 28);

    if (!m_textMode) {
        layoutContent();
    }
}

} // namespace app