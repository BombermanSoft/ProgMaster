#include "app/ConfiguratorTab.h"

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
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

        m_removeButton.setButtonText("Remover");
        m_removeButton.setTooltip(
            L"Apaga a seção ['" + jstr(readconf::sectionNameFor(m_scope)) +
            "'] inteira (cabeçalho e linhas).");
        m_removeButton.onClick = [this] { m_host.onRemoveScope(m_scope); };
        addAndMakeVisible(m_removeButton);

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
? L"Configuração presente (não reconhecida —"
                                    L" preservada no salvamento)"
                                  : L"Configurado",
                             juce::dontSendNotification);
        } else {
            m_status.setColour(juce::Label::textColourId, juce::Colours::tomato);
            m_status.setText(L"Não configurado (seção ausente)", juce::dontSendNotification);
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

    // Altura do cartão conforme o conteúdo. O texto explicativo é resumido
    // (listas de arquivos foram substituídas por contagem), então o cartão
    // fica menor do que quando listava todos os arquivos.
    int preferredHeight() const
    {
        size_t lines = 0;
        if (m_snap.present && m_snap.option != readconf::FormatOption::Unknown) {
            lines = 2 + (m_snap.files.empty() ? 1 : 2);
        } else if (m_snap.present) {
            lines = 3; // mensagem "não reconhecida" pode ocupar 2-3 linhas
        } else {
            lines = 3; // mensagem "A execução usa..." + eventual linha ARQUIVO
        }
        const int filesTop = m_showToken ? 128 : 108;
        const int filesH = juce::jmax(24, static_cast<int>(lines) * 14 + 6);
        return juce::jmax(190, filesTop + filesH + 6);
    }

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
            m_removeButton.setVisible(true);
            m_formato.setBounds(margin, 18 + statusH + 6 + rowH,
                                w - 2 * margin - 124, 24);
            m_removeButton.setBounds(w - margin - 116, 18 + statusH + 6 + rowH,
                                     116, 24);
        } else {
            m_formato.setVisible(false);
            m_addButton.setVisible(true);
            m_removeButton.setVisible(false);
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
            textOut = optionExplanation(m_snap.option, folderHint);
            int found = 0;
            for (const auto& f : m_snap.files) {
                if (f.exists) {
                    ++found;
                }
            }
            // Em vez da lista longa de expectativas, mostra somente um resumo
            // dos arquivos compatíveis encontrados no disco + exemplos.
            if (found > 0) {
                textOut += L"\nArquivos compatíveis encontrados: " +
                           std::to_wstring(found);
                if (m_snap.files.size() > 1) {
                    textOut += L" de " + std::to_wstring(m_snap.files.size());
                }
                textOut += L".";
            } else {
                textOut +=
                    L"\nAinda não há arquivos compatíveis nesta " + folderHint +
                    L" — eles são criados pelo gerador diário.";
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

    // Frase curta que explica o que a opção faz (usada no lugar da lista de
    // arquivos esperados, que ocupava espaço na tela).
    static std::wstring optionExplanation(readconf::FormatOption option,
                                          const std::wstring& folderHint)
    {
        switch (option) {
        case readconf::FormatOption::Auto:
            return L"Formato automático: o Playlist escolhe o arquivo de hoje "
                   L"automaticamente a cada dia.";
        case readconf::FormatOption::Single:
            return L"Um único arquivo contém toda a programação. Fica na " +
                   folderHint + L", com o nome fixo definido no ARQUIVO.";
        case readconf::FormatOption::Weekly:
            return L"Um arquivo por dia da semana (Seg a Dom), todos na " +
                   folderHint + L", com o nome no formato definido no ARQUIVO.";
        case readconf::FormatOption::CommercialDay:
            return L"Um arquivo por dia, nomeado com o dia do mês (DD), na " +
                   folderHint + L". A cada dia ele vira o arquivo corrente.";
        case readconf::FormatOption::CommercialDate:
            return L"Um arquivo por data (DD-MM-AAAA), na " + folderHint +
                   L", preservando o histórico diário completo.";
        case readconf::FormatOption::Planner:
            return L"Planner: o arquivo é gerado a partir da data corrente "
                   L"(DD-MM-AAAA), na " + folderHint + L".";
        case readconf::FormatOption::Maker:
            return L"Maker: o arquivo é gerado a partir da data corrente "
                   L"(DD-MM-AAAA), na " + folderHint + L".";
        case readconf::FormatOption::Unknown:
            break;
        }
        return L"";
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
    juce::TextButton m_removeButton;
    std::vector<std::pair<int, readconf::FormatOption>> m_optIds;
};

// ============================================================================
// Linha de afiliada (endereço | porta | ativa | remover)
// ============================================================================
class AfiliadaRow final : public juce::Component {
public:
    AfiliadaRow(ConfiguratorTab& host, size_t position,
                const std::wstring& name, const std::wstring& address,
                const std::wstring& port, bool active)
        : m_host(host), m_position(position)
    {
        // O documento usa "AFILIADA" como NOME (chave da linha) para linhas
        // novas ainda sem nome; aqui exibimos o campo em branco com um texto
        // PLACEHOLDER visual (mais claro que a escrita normal): a digitação
        // é direta, sem precisar apagar nada. O nome real só é gravado quando
        // o usuário digita.
        const bool placeholderName = (name == L"AFILIADA");

        m_name.setTooltip("Nome da afiliada (a CHAVE da linha, ex.: TESTE).");
        m_name.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3c3c3c));
        m_name.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        m_name.setText(placeholderName ? juce::String() : jstr(name),
                       juce::dontSendNotification);
        m_name.setTextToShowWhenEmpty(juce::String("AFILIADA"), juce::Colours::grey);
        m_name.onTextChange = [this] { m_host.onRowChanged(m_position); };
        addAndMakeVisible(m_name);

        m_address.setTooltip(L"Endereço da afiliada (ex.: 192.168.0.50).");
        m_address.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3c3c3c));
        m_address.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        m_address.setText(jstr(address), juce::dontSendNotification);
        m_address.setTextToShowWhenEmpty(juce::String(L"ENDEREÇO"), juce::Colours::grey);
        m_address.onTextChange = [this] { m_host.onRowChanged(m_position); };
        addAndMakeVisible(m_address);

        m_port.setTooltip("Porta (1–65535); validada ao Salvar.");
        m_port.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3c3c3c));
        m_port.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        m_port.setText(jstr(port), juce::dontSendNotification);
        m_port.setTextToShowWhenEmpty(juce::String("PORTA"), juce::Colours::grey);
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
        const int gap = 8;
        const int nameW = 110;

        m_remove.setBounds(getWidth() - removeW - margin, 0, removeW, h);
        m_ativa.setBounds(getWidth() - removeW - margin - ativaW - gap, 0, ativaW, h);
        m_port.setBounds(getWidth() - removeW - margin - ativaW - gap - portW - gap, 0,
                         portW, h);
        m_address.setBounds(margin + nameW + gap, 0,
                            getWidth() - removeW - margin - ativaW - gap - portW - gap
                                - (margin + nameW + gap) - gap,
                            h);
        m_name.setBounds(margin, 0, nameW, h);
    }

    juce::String nameText() const { return m_name.getText(); }
    juce::String addressText() const { return m_address.getText(); }
    juce::String portText() const { return m_port.getText(); }
    bool active() const { return m_ativa.getToggleState(); }

private:
    ConfiguratorTab& m_host;
    size_t m_position = 0;
    juce::TextEditor m_name;
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
                    m_host, i, list[i].name, list[i].address, list[i].portText,
                    !list[i].disabled);
                addAndMakeVisible(*row);
                m_rows.push_back(std::move(row));
            }
        } else {
            m_info.setColour(juce::Label::textColourId, juce::Colours::orange);
            m_info.setText(L"A seção [AFILIADAS] não existe no PLAYLIST.ini.", juce::dontSendNotification);
            m_addSection.setVisible(true);
            m_addRow.setVisible(false);
        }
        resized();
    }

    int preferredHeight() const
    {
        // info (18+20) -> botão (40+24) -> linhas a partir de y=68.
        return 68 + static_cast<int>(m_rows.size()) * (28 + 4);
    }

    void resized() override
    {
        const int margin = 8;
        const int rowH = 28;
        m_info.setBounds(margin, 18, getWidth() - 2 * margin, 20);
        m_addSection.setBounds(60, 40, 180, 24);
        m_addRow.setBounds(margin, 40, 180, 24);

        int y = 68;
        for (auto& row : m_rows) {
            row->setBounds(margin, y, getWidth() - 2 * margin, rowH);
            y += rowH + 4;
        }
        // A altura deve caber todas as linhas DENTRO do cartão (antes a última
        // linha/seus botões ficavam para fora da borda inferior do grupo).
        setSize(getWidth(), 68 + static_cast<int>(m_rows.size()) * (rowH + 4));
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

    m_textModeButton.setButtonText(L"\u270E  Visualizar como texto");
    m_textModeButton.setTooltip(
        L"Grava as alterações em disco e abre o PLAYLIST.ini no Bloco de "
        L"Notas do Windows para edição manual. Ao voltar a esta guia, o "
        L"arquivo é lido do disco novamente.");
    m_textModeButton.onClick = [this] { openInNotepad(); };
    addAndMakeVisible(m_textModeButton);

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
    m_reloadFromDiskOnVisible = false;
    rebuildAll();
    updateDirtyLabel();
    resized();
}

void ConfiguratorTab::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (isVisible() && m_reloadFromDiskOnVisible) {
        // O usuário voltou do Bloco de Notas: relê o arquivo do disco para ver
        // as edições externas (inclusive seções removidas/adicionadas por lá).
        m_reloadFromDiskOnVisible = false;
        m_controller.load();
        rebuildAll();
    } else if (isVisible()) {
        // Documento pode ter mudado enquanto a guia estava oculta. Sincroniza
        // os cartões com o estado em memória (sem reler o disco).
        rebuildAll();
    }
    updateDirtyLabel();
}

void ConfiguratorTab::openInNotepad()
{
    // Grava o estado atual em disco SEM validação para que o Bloco de Notas
    // mostre exatamente o que está em memória (inclusive alterações ainda não
    // clicadas em "Salvar playlist.ini").
    std::wstring userMessage;
    std::string technical;
    if (m_controller.flushPendingToDisk(userMessage, technical)) {
        m_reloadFromDiskOnVisible = true;
    } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            L"Não foi possível abrir o Bloco de Notas", jstr(userMessage));
        return;
    }

#if JUCE_WINDOWS
    const std::filesystem::path p = m_controller.path();
    if (p.empty() || !std::filesystem::exists(p)) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            L"Não foi possível abrir o Bloco de Notas",
            L"O PLAYLIST.ini ainda não existe no disco.");
        m_reloadFromDiskOnVisible = false;
        return;
    }
    const std::wstring file = p.wstring();
    HINSTANCE h = ShellExecuteW(nullptr, L"open", L"notepad.exe", file.c_str(),
                                nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(h) <= 32) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            L"Não foi possível abrir o Bloco de Notas",
            jstr(L"Falha ao iniciar notepad.exe (código " +
                 std::to_wstring(reinterpret_cast<intptr_t>(h)) + L")."));
        m_reloadFromDiskOnVisible = false;
    }
#endif
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

void ConfiguratorTab::onRemoveScope(readconf::ConfigScope scope)
{
    m_controller.removeScope(scope);
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
    m_controller.updateAfiliada(position,
                                wstr(row->nameText()),
                                wstr(row->addressText()),
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
    m_controller.addAfiliada(L"", L"", L"", false);
    refreshAfiliadasOnly();
    updateDirtyLabel();
}

void ConfiguratorTab::onAddAfiliadaSection()
{
    m_controller.addAfiliada(L"", L"", L"", false);
    refreshAfiliadasOnly();
    updateDirtyLabel();
}

void ConfiguratorTab::savePlaylistIni()
{
    std::wstring userMessage;
    std::string technical;
    if (m_controller.save(userMessage, technical)) {
        m_controller.markClean();
        updateDirtyLabel();
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon, L"Salvo",
            L"PLAYLIST.ini foi gravado com sucesso.");
    } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon, L"Não foi possível salvar",
            jstr(userMessage));
    }
}

void ConfiguratorTab::discardChanges()
{
    m_controller.load();
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
    const int margin = 6;
    const int contentWidth = juce::jmax(320, m_visualArea.getWidth() - 2 * margin);
    int y = margin;

    for (auto& card : m_cards) {
        int cardH = 190;
        if (card.get() == static_cast<juce::Component*>(m_afiliadasCard)) {
            cardH = preferredAfiliadasHeight();
        } else {
            for (ScopeCard* sc : m_scopeCards) {
                if (sc == card.get()) {
                    cardH = sc->preferredHeight();
                    break;
                }
            }
        }
        card->setBounds(margin, y, contentWidth - 2 * margin, cardH);
        y += cardH + margin;
    }
    m_content.setSize(juce::jmax(320, m_visualArea.getWidth()), juce::jmax(margin, y));
    m_visualArea.setScrollBarsShown(true, false, false, false);
}

void ConfiguratorTab::updateDirtyLabel()
{
    if (m_controller.isDirty()) {
        m_dirtyLabel.setText(L"alterações não salvas", juce::dontSendNotification);
        m_dirtyLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    } else {
        m_dirtyLabel.setText("documento em dia", juce::dontSendNotification);
        m_dirtyLabel.setColour(juce::Label::textColourId, juce::Colours::lime);
    }
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

    m_visualArea.setBounds(area.getX() + margin, top,
                           area.getWidth() - 2 * margin, bottom - top);

    m_discardButton.setBounds(area.getX() + margin, area.getBottom() - bottomH,
                              160, 28);
    m_saveButton.setBounds(area.getRight() - 170, area.getBottom() - bottomH,
                           164, 28);

    layoutContent();
}

} // namespace app