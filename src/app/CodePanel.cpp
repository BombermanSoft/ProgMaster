#include "app/CodePanel.h"

#include <algorithm>

#include "app/JuceHelpers.h"

namespace {
constexpr int kRowHeight = 24;
constexpr int kGap = 5;
constexpr int kMargin = 6;
constexpr int kNavW = 26;
constexpr int kAddW = 30;
constexpr int kResizeStrip = 5;

// Larguras dos controles fixos, para o cálculo de espaço dos códigos.
constexpr int kCodeMinW = 58;
constexpr int kCodeMaxW = 150;
constexpr int kCodeTextPad = 18;

// Diálogo: cria um código de sessão.
struct NewCode {
    bool ok = false;
    std::wstring code;
    std::wstring title;
};

NewCode askNewCode(juce::Component& parent)
{
    juce::AlertWindow window(
        L"Novo código",
        L"Crie um código para usar nos horários. Ele ficará disponível apenas "
        L"nesta sessão: a Lista de Códigos (folders.xml) não é alterada, porque "
        L"o arquivo original deve permanecer intacto.",
        juce::MessageBoxIconType::NoIcon, &parent);
    window.addTextEditor(L"codigo", juce::String(), L"Código (letras e números)");
    window.addTextEditor(L"nome", juce::String(), L"Nome (opcional, só para identificar)");
    window.addButton(L"Criar", 1,
                     juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    window.addButton(L"Cancelar", 0,
                     juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));
    window.enterModalState(true);
    if (window.runModalLoop() != 1) {
        return {};
    }
    NewCode out;
    out.ok = true;
    out.code = app::wstr(window.getTextEditorContents(L"codigo"));
    out.title = app::wstr(window.getTextEditorContents(L"nome"));
    return out;
}
} // namespace

namespace app {

// Paleta fixa, escolhida para os códigos ficarem distinguíveis uns dos outros
// e legíveis com o texto preto. A cor vem do ÍNDICE no catálogo: o mesmo
// código mantém a mesma cor durante a sessão.
juce::Colour codeColour(int index)
{
    static const juce::Colour palette[] = {
        juce::Colour(0xffffa726), // laranja
        juce::Colour(0xff42a5f5), // azul
        juce::Colour(0xff66bb6a), // verde
        juce::Colour(0xffab47bc), // roxo
        juce::Colour(0xff26a69a), // verde-água
        juce::Colour(0xffff7043), // laranja escuro
        juce::Colour(0xff4db6ac), // água
        juce::Colour(0xffffd54f), // amarelo
        juce::Colour(0xff8d6e63), // castanho
        juce::Colour(0xff78909c), // cinza-azulado
        juce::Colour(0xfff06292), // rosa
        juce::Colour(0xff5c6bc0), // índigo
    };
    constexpr int count = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
    int i = index % count;
    if (i < 0) {
        i += count;
    }
    return palette[i];
}

// ============================================================================
// CodeButton
// ============================================================================

CodePanel::CodeButton::CodeButton(std::wstring codeText, std::wstring titleText,
                                 juce::Colour colour,
                                 blocos::CodeOrigin origin, bool sessionOnly)
    : juce::Button(app::jstr(codeText)), m_code(std::move(codeText)),
      m_colour(colour), m_origin(origin), m_sessionOnly(sessionOnly)
{
    // A procedência vai na dica: o usuário precisa saber por que um código está
    // na Lista mesmo sem estar no folders.xml da instalação.
    juce::String note;
    if (origin == blocos::CodeOrigin::FromBlockFile) {
        note = juce::String(L"\n(usado no arquivo de bloco, mas não consta do "
                            L"folders.xml desta instalação)");
    } else if (origin == blocos::CodeOrigin::CreatedByUser) {
        note = juce::String(L"\n(criado nesta sessão)");
    }
    if (!titleText.empty()) {
        setTooltip(app::jstr(titleText) + juce::String(L"\nCódigo: ")
                   + app::jstr(m_code) + note);
    } else {
        setTooltip(app::jstr(m_code) + note);
    }
    setClickingTogglesState(false);
}

void CodePanel::CodeButton::setArmedUi(bool armed)
{
    if (m_armed == armed) {
        return;
    }
    m_armed = armed;
    repaint();
}

void CodePanel::CodeButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const juce::Rectangle<float> area = getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour fill = m_colour;
    if (down) {
        fill = fill.darker(0.25f);
    } else if (highlighted) {
        fill = fill.brighter(0.15f);
    }
    if (!isEnabled()) {
        fill = fill.withMultipliedSaturation(0.2f).withAlpha(0.45f);
    }
    g.setColour(fill);
    g.fillRoundedRectangle(area, 5.0f);

    // Borda branca quando ARMADO (código "grudado no mouse").
    if (m_armed) {
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(area, 5.0f, 2.0f);
    } else if (m_sessionOnly) {
        // Códigos de sessão têm borda tracejada: sinaliza que não vêm do
        // folders.xml e somem ao fechar o programa.
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        const float dash = 3.0f;
        g.drawRoundedRectangle(area.reduced(1.5f), 4.0f, 1.0f);
        (void)dash;
    }

    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, true);
}

// ============================================================================
// CodePanel
// ============================================================================

CodePanel::CodePanel(blocos::CodeCatalogue& catalogue) : m_catalogue(catalogue)
{
    m_prevBtn.setTooltip(L"Mostrar os códigos anteriores.");
    m_prevBtn.onClick = [this] {
        if (m_page > 0) {
            --m_page;
            resized();
        }
    };
    m_nextBtn.setTooltip(L"Mostrar os próximos códigos.");
    m_nextBtn.onClick = [this] {
        if (m_page + 1 < m_pageCount) {
            ++m_page;
            resized();
        }
    };
    addAndMakeVisible(m_prevBtn);
    addAndMakeVisible(m_nextBtn);

    m_addBtn.setTooltip(L"Criar um novo código (fica disponível só nesta "
                        L"sessão; o folders.xml não é alterado).");
    m_addBtn.onClick = [this] { createCode(); };
    addAndMakeVisible(m_addBtn);

    m_trashBtn.setTooltip(L"Remover o código ARMADO da Lista quando ele não vem "
                          L"do folders.xml (criado com \"+\" ou adotado do "
                          L"arquivo de bloco). Códigos do folders.xml nunca são "
                          L"removidos.");
    m_trashBtn.onClick = [this] { removeArmedSessionCode(); };
    addAndMakeVisible(m_trashBtn);

    m_hint.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_hint.setFont(juce::Font(juce::FontOptions(12.0f)));
    m_hint.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_hint);

    setWantsKeyboardFocus(true);
    m_hint.setText(
        L"Códigos da Lista de Códigos (folders.xml). Arraste a borda inferior "
        L"para mostrar mais fileiras.",
        juce::dontSendNotification);
    rebuild();
}

CodePanel::~CodePanel() = default;

void CodePanel::rebuild()
{
    m_codeButtons.clear();
    m_pageButtons.clear();
    const auto& entries = m_catalogue.entries();
    m_codeButtons.reserve(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        const blocos::CodeEntry& entry = entries[i];
        auto button = std::make_unique<CodeButton>(
            entry.code, entry.title, codeColour(static_cast<int>(i)),
            entry.origin, entry.sessionOnly);
        CodeButton* raw = button.get();
        raw->onClick = [this, raw] { armFromButton(raw); };
        // Os botões SÓ aparecem e recebem cliques se forem filhos do painel.
        addAndMakeVisible(*raw);
        m_codeButtons.push_back(std::move(button));
    }
    // resized() (e não updatePagination) para posicionar os botões também
    // quando o painel ainda não tem largura — sem isso a paleta apareceria
    // vazia até o próximo clique em "+".
    resized();
    // A dica diz de onde veio a lista: o folders.xml da instalação e, quando
    // existe, os códigos que o próprio arquivo usa e que NÃO estão lá.
    int fromFile = 0;
    for (const blocos::CodeEntry& entry : m_catalogue.entries()) {
        if (entry.origin == blocos::CodeOrigin::FromBlockFile) {
            ++fromFile;
        }
    }
    juce::String hint;
    if (m_catalogue.empty()) {
        hint = juce::String(L"Nenhum código encontrado na Lista de Códigos "
                            L"(folders.xml) desta instalação.");
    } else if (fromFile > 0) {
        hint = juce::String(L"Lista de Códigos: folders.xml + ")
               + juce::String(fromFile)
               + (fromFile == 1 ? juce::String(L" código usado no arquivo e "
                                               L"ausente do folders.xml. ")
                                : juce::String(L" códigos usados no arquivo e "
                                               L"ausentes do folders.xml. "))
               + juce::String(L"Arraste a borda inferior para mostrar mais "
                              L"fileiras.");
    } else {
        hint = juce::String(L"Códigos da Lista de Códigos (folders.xml). "
                            L"Arraste a borda inferior para mostrar mais "
                            L"fileiras.");
    }
    m_hint.setText(hint, juce::dontSendNotification);
    if (m_armed.empty()) {
        for (auto& button : m_codeButtons) {
            if (button) {
                button->setArmedUi(false);
            }
        }
    } else {
        setArmed(m_armed);
    }
}

void CodePanel::armFromButton(CodeButton* button)
{
    if (button == nullptr) {
        return;
    }
    if (isArmed() && m_armed == button->code()) {
        cancelArmed(); // clicar no mesmo código solta o "grudado"
        return;
    }
    setArmed(button->code());
}

void CodePanel::setArmed(const std::wstring& code)
{
    m_armed = code;
    for (auto& button : m_codeButtons) {
        if (button) {
            button->setArmedUi(!code.empty() && button->code() == code);
        }
    }
    // A lixeira só vale para códigos de sessão.
    const int index = m_catalogue.indexOf(m_armed);
    m_trashBtn.setEnabled(index >= 0 && m_catalogue.entries()[static_cast<size_t>(index)].sessionOnly);
    repaint();
    // Notifica sempre por aqui, para que a dica e o chip fantasma do editor
    // fiquem em sincronia com o estado (inclusive ao desarmar).
    if (onArmed) {
        onArmed(m_armed);
    }
}

void CodePanel::cancelArmed()
{
    setArmed(std::wstring());
}

int CodePanel::itemWidth(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= m_codeButtons.size()) {
        return 0;
    }
    const juce::Font f(juce::FontOptions(12.5f, juce::Font::bold));
    const int text = static_cast<int>(
        juce::GlyphArrangement::getStringWidth(f, m_codeButtons[static_cast<size_t>(index)]->getButtonText()));
    return juce::jlimit(kCodeMinW, kCodeMaxW, text + kCodeTextPad);
}

int CodePanel::preferredHeight() const
{
    // Fileiras de códigos + uma fileira para a dica e a borda arrastável.
    return m_rows * (kRowHeight + kGap) + kRowHeight + 2;
}

void CodePanel::setRows(int rows)
{
    const int clamped = juce::jlimit(1, 8, rows);
    if (clamped == m_rows) {
        return;
    }
    m_rows = clamped;
    resized();
    if (onRowsChanged) {
        onRowsChanged(m_rows);
    }
}

void CodePanel::updatePagination()
{
    // Calcula quantos códigos cabem em `m_rows` fileiras dentro da largura
    // disponível, e a partir daí quantas páginas o catálogo precisa.
    // Precisa rodar DE NOVO a cada mudança de largura: na construção o painel
    // ainda tem largura 0, e sem isto nenhum código apareceria.
    const int avail = getWidth() - 2 * kMargin - kNavW * 2 - kAddW * 2
                      - kGap * 6;
    if (avail <= 0) {
        m_itemsPerPage = 0;
    } else {
        int used = 0;
        int per = 0;
        int rowsUsed = 1;
        for (size_t i = 0; i < m_codeButtons.size(); ++i) {
            const int w = itemWidth(static_cast<int>(i));
            if (per > 0 && used + w > avail) {
                // Não cabe mais nesta fileira: quebra (até o limite de fileiras).
                if (rowsUsed >= m_rows) {
                    break;
                }
                ++rowsUsed;
                used = 0;
            }
            used += w + kGap;
            ++per;
        }
        m_itemsPerPage = per;
    }
    m_pageCount = m_itemsPerPage <= 0
                      ? 1
                      : juce::jmax(1, (static_cast<int>(m_codeButtons.size())
                                       + m_itemsPerPage - 1) / m_itemsPerPage);
    m_page = juce::jlimit(0, m_pageCount - 1, m_page);

    // Seleciona os botões da página atual.
    m_pageButtons.clear();
    for (int i = 0; i < m_itemsPerPage; ++i) {
        const int index = m_page * m_itemsPerPage + i;
        if (static_cast<size_t>(index) < m_codeButtons.size()) {
            m_pageButtons.push_back(m_codeButtons[static_cast<size_t>(index)].get());
        }
    }

    for (size_t i = 0; i < m_codeButtons.size(); ++i) {
        const bool visible = std::find(m_pageButtons.begin(), m_pageButtons.end(),
                                       m_codeButtons[i].get())
                            != m_pageButtons.end();
        m_codeButtons[i]->setVisible(visible);
    }

    updateNavButtons();
}

void CodePanel::updateNavButtons()
{
    m_prevBtn.setEnabled(m_page > 0);
    m_nextBtn.setEnabled(m_page + 1 < m_pageCount);
}

void CodePanel::resized()
{
    // Recalcula a página a cada mudança de tamanho/posição dos botões.
    updatePagination();

    const int y0 = 2;
    const int right = getWidth() - kMargin;

    m_addBtn.setBounds(right - kAddW, y0, kAddW, kRowHeight);
    m_trashBtn.setBounds(right - 2 * kAddW - kGap, y0, kAddW, kRowHeight);
    m_nextBtn.setBounds(right - 2 * kAddW - kGap - kNavW - kGap, y0, kNavW, kRowHeight);
    m_prevBtn.setBounds(right - 2 * kAddW - kGap - 2 * kNavW - 2 * kGap, y0, kNavW,
                        kRowHeight);

    const int codesLeft = kMargin;
    const int codesRight = m_prevBtn.getX() - kGap;

    int x = codesLeft;
    int y = y0;
    int row = 0;
    for (CodeButton* button : m_pageButtons) {
        if (button == nullptr) {
            continue;
        }
        const int index = m_catalogue.indexOf(button->code());
        const int w = itemWidth(index >= 0 ? index : 0);
        if (x > codesLeft && x + w > codesRight) {
            x = codesLeft;
            y += kRowHeight + kGap;
            ++row;
        }
        button->setBounds(x, y, w, kRowHeight);
        x += w + kGap;
    }

    // Dica logo abaixo da última fileira de códigos, à direita do contador de
    // páginas (desenhado no paint) para não se sobrepor.
    const int usedRows = juce::jmax(1, row + 1);
    const int hintY = y0 + juce::jmin(usedRows, m_rows) * (kRowHeight + kGap);
    const int hintX = kMargin + 44;
    if (hintY + kRowHeight <= getHeight()) {
        m_hint.setBounds(hintX, hintY,
                         juce::jmax(0, getWidth() - hintX - kMargin), kRowHeight);
        m_hint.setVisible(true);
    } else {
        // Não há espaço para a dica (painel ainda pequeno): o editor mostra a
        // mesma orientação no rodapé, então a ocultamos em vez de cortar.
        m_hint.setVisible(false);
    }
}

void CodePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff242424));
    // Marca visual da borda de redimensionamento.
    if (m_overResize || m_draggingResize) {
        g.setColour(juce::Colour(0xff26a69a));
        g.fillRect(0, getHeight() - kResizeStrip, getWidth(), 2);
    }
    // Contador de páginas (canto inferior esquerdo) quando há mais de uma.
    if (m_pageCount > 1) {
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(juce::String(m_page + 1) + juce::String::fromUTF8("/")
                       + juce::String(m_pageCount),
                   juce::Rectangle<int>(kMargin, getHeight() - 18, 40, 14),
                   juce::Justification::centredLeft, true);
    }
}

bool CodePanel::overResizeStrip(int y) const
{
    return y >= getHeight() - kResizeStrip;
}

void CodePanel::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    if (overResizeStrip(event.getPosition().getY())) {
        m_draggingResize = true;
        m_dragStartY = event.getPosition().getY();
        m_dragStartRows = m_rows;
        repaint();
        return;
    }
    juce::Component::mouseDown(event);
}

void CodePanel::mouseDrag(const juce::MouseEvent& event)
{
    if (!m_draggingResize) {
        juce::Component::mouseDrag(event);
        return;
    }
    const int dy = m_dragStartY - event.getPosition().getY();
    // Arrastar para cima (dy > 0) aumenta a quantidade de fileiras.
    const int rows = m_dragStartRows + dy / (kRowHeight + kGap);
    const int clamped = juce::jlimit(1, 8, rows);
    if (clamped != m_rows) {
        m_rows = clamped;
        resized();
        if (onRowsChanged) {
            onRowsChanged(m_rows);
        }
    }
    repaint();
}

void CodePanel::mouseUp(const juce::MouseEvent& event)
{
    if (m_draggingResize) {
        m_draggingResize = false;
        repaint();
    }
    juce::Component::mouseUp(event);
}

void CodePanel::mouseMove(const juce::MouseEvent& event)
{
    const bool over = overResizeStrip(event.getPosition().getY());
    if (over != m_overResize) {
        m_overResize = over;
        repaint();
    }
}

bool CodePanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && isArmed()) {
        cancelArmed();
        return true;
    }
    if (key == juce::KeyPress::leftKey && m_page > 0) {
        --m_page;
        resized();
        return true;
    }
    if (key == juce::KeyPress::rightKey && m_page + 1 < m_pageCount) {
        ++m_page;
        resized();
        return true;
    }
    return false;
}

void CodePanel::createCode()
{
    const NewCode asked = askNewCode(*this);
    if (!asked.ok) {
        return;
    }
    std::wstring error;
    if (!m_catalogue.addSessionCode(asked.code, asked.title, error)) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon, L"Novo código",
            app::jstr(error));
        return;
    }
    rebuild();
    // O código novo já fica armado: o usuário já pode aplicá-lo a um horário.
    setArmed(asked.code);
    if (onCatalogueChanged) {
        onCatalogueChanged();
    }
    m_hint.setText(L"Código " + app::jstr(asked.code)
                       + L" criado (só nesta sessão) e armado — clique num "
                         L"horário para adicioná-lo.",
                   juce::dontSendNotification);
}

void CodePanel::removeArmedSessionCode()
{
    if (!isArmed()) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon, L"Remover código",
            L"Selecione (arme) um código primeiro.");
        return;
    }
    const std::wstring code = m_armed;
    const int index = m_catalogue.indexOf(code);
    const bool sessionOnly = index >= 0 && m_catalogue.entries()[static_cast<size_t>(index)].sessionOnly;
    if (!sessionOnly) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon, L"Remover código",
            L"Códigos que vêm da Lista de Códigos (folders.xml) não podem ser "
            L"removidos: esse arquivo é somente leitura.");
        return;
    }
    if (m_catalogue.removeSessionCode(code)) {
        cancelArmed();
        rebuild();
        if (onCatalogueChanged) {
            onCatalogueChanged();
        }
    }
}

} // namespace app
