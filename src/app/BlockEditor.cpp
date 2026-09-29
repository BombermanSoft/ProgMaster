#include "app/BlockEditor.h"

#include <algorithm>
#include <cwctype>

#include "app/JuceHelpers.h"

namespace {
constexpr int kRowHeight = 28;
constexpr int kMargin = 8;
constexpr int kTimeWidth = 62;
constexpr int kChipGap = 4;
constexpr int kChipPadding = 7;
constexpr int kSelW = 20;
constexpr int kSliderW = 10;
constexpr int kSliderGap = 3;

juce::Font monoFont(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(
        juce::Font::getDefaultMonospacedFontName(),
        bold ? juce::String("Bold") : juce::String("Regular"), size));
}

void trimWs(std::wstring& s)
{
    while (!s.empty() && (s.front() == L' ' || s.front() == L'\t')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == L' ' || s.back() == L'\t')) {
        s.pop_back();
    }
}

class TimeFieldFilter final : public juce::TextEditor::InputFilter {
public:
    juce::String filterNewText(juce::TextEditor&, const juce::String& newInput) override
    {
        juce::String out;
        for (int i = 0; i < newInput.length(); ++i) {
            const auto c = newInput[i];
            if ((c >= L'0' && c <= L'9') || c == L':') {
                out += c;
            }
        }
        return out;
    }
};

struct FillSpec {
    bool ok = false;
    std::wstring inicio;
    int intervalo = 0;
    std::wstring error;
};

FillSpec askFill(juce::Component& parent)
{
    juce::AlertWindow window(
        L"Preencher horários",
        L"Preenche o bloco a partir do Início, incrementando de Intervalo em "
        L"Intervalo até o fim do dia.",
        juce::MessageBoxIconType::NoIcon, &parent);
    window.addTextEditor(L"inicio", juce::String(L"00:00"), L"Início (HH:MM)");
    window.addTextEditor(L"intervalo", juce::String(L"30"), L"Intervalo (minutos)");
    window.addButton(L"Preencher", 1,
                     juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    window.addButton(L"Cancelar", 0,
                     juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));
    window.enterModalState(true);
    if (window.runModalLoop() != 1) {
        return {};
    }

    FillSpec spec;
    spec.inicio = app::wstr(window.getTextEditorContents(L"inicio"));
    std::wstring intervalo = app::wstr(window.getTextEditorContents(L"intervalo"));
    trimWs(spec.inicio);
    trimWs(intervalo);

    if (spec.inicio.empty() || blocos::BlockDocument::parseTime(spec.inicio) < 0) {
        spec.error = L"Início inválido — use o formato HH:MM.";
        return spec;
    }
    if (intervalo.empty()) {
        spec.error = L"Intervalo inválido — digite os minutos.";
        return spec;
    }
    int mins = 0;
    for (const wchar_t c : intervalo) {
        if (c < L'0' || c > L'9') {
            spec.error = L"Intervalo inválido — use apenas números (minutos).";
            return spec;
        }
        mins = mins * 10 + (c - L'0');
        if (mins > 720) {
            spec.error = L"Intervalo muito grande (máx. 720 minutos).";
            return spec;
        }
    }
    if (mins <= 0) {
        spec.error = L"Intervalo deve ser maior que zero.";
        return spec;
    }
    spec.intervalo = mins;
    spec.ok = true;
    return spec;
}
} // namespace

namespace app {

BlockEditor::BlockEditor(blocos::BlockDocument& doc,
                         blocos::CodeCatalogue& catalogue,
                         std::vector<BlockClip>& clipboard)
    : m_doc(doc), m_catalogue(catalogue), m_clipboard(clipboard),
      m_codePanel(catalogue)
{
    m_codePanel.onCatalogueChanged = [this] {
        rebuildCombo();
        refreshButtons();
    };
    m_codePanel.onArmed = [this](const std::wstring&) {
        // O "código grudado" precisa atualizar a dica e o chip fantasma.
        refreshButtons();
        repaint();
    };
    m_codePanel.onRowsChanged = [this](int) {
        // O usuário redimensionou o painel: a altura abaixo muda.
        resized();
    };
    addAndMakeVisible(m_codePanel);

    m_hintLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_hintLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    m_hintLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_hintLabel);

    m_timeField.setInputFilter(new TimeFieldFilter(), true);
    m_timeField.setFont(monoFont(13.0f));
    m_timeField.setTextToShowWhenEmpty(L"Adicionar Horário", juce::Colours::grey);
    m_timeField.setTooltip(L"Horário a adicionar no formato HH:MM.");
    m_timeField.onReturnKey = [this] { addFromField(); };
    addAndMakeVisible(m_timeField);

    m_addTimeBtn.onClick = [this] { addFromField(); };
    m_addTimeBtn.setTooltip(L"OK — confirma a adição do horário digitado (HH:MM).");
    addAndMakeVisible(m_addTimeBtn);

    m_fillBtn.onClick = [this] { fillTimes(); };
    m_fillBtn.setTooltip(L"Preencher Horários: define o Início e o Intervalo — "
                         L"ex.: 00:00 a cada 30 minutos gera 00:00, 00:30, "
                         L"01:00, ... até o fim do dia.");
    addAndMakeVisible(m_fillBtn);

    m_removeBtn.onClick = [this] { removeSelected(); };
    m_removeBtn.setTooltip(L"Remove, dos horários selecionados, os horários "
                           L"inteiros ou apenas os códigos.");
    addAndMakeVisible(m_removeBtn);

    m_selectBtn.onClick = [this] { onSelectCheckClick(); };
    m_selectBtn.setTooltip(
        L"Ativa a seleção de vários horários (clique numa linha para marcar/"
        L"desmarcar). Ativado, esse mesmo quadrado marca TODOS; se todos já "
        L"estiverem marcados, limpa a seleção.");
    addAndMakeVisible(m_selectBtn);

    m_codeCombo.setText(juce::String(L"Código"), juce::dontSendNotification);
    m_codeCombo.setTooltip(L"Código: escolha o código a enviar para todos os "
                           L"horários selecionados.");
    addAndMakeVisible(m_codeCombo);

    m_applyBtn.onClick = [this] { applyCodeToSelected(); };
    m_applyBtn.setTooltip(L"Aplica o código (do seletor) a TODOS os horários "
                          L"selecionados.");
    addAndMakeVisible(m_applyBtn);

    m_copyBtn.onClick = [this] { copySelected(); };
    m_pasteBtn.onClick = [this] { pasteSelected(); };
    m_copyBtn.setTooltip(L"Copiar os horários + códigos dos selecionados "
                         L"(Ctrl+C) — compartilhado entre os arquivos.");
    m_pasteBtn.setTooltip(L"Colar (Ctrl+V) os horários copiados: cria os que "
                          L"faltam e acrescenta os códigos aos existentes.");
    addAndMakeVisible(m_copyBtn);
    addAndMakeVisible(m_pasteBtn);

    m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_statusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    m_statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_statusLabel);

    setWantsKeyboardFocus(true);
    rebuildCombo();
    rebuildRows();
    refreshButtons();
}

BlockEditor::~BlockEditor() = default;

void BlockEditor::rebuildCombo()
{
    const juce::String previous = m_codeCombo.getText();
    m_codeCombo.clear();
    const auto& entries = m_catalogue.entries();
    for (size_t i = 0; i < entries.size(); ++i) {
        const std::wstring label = entries[i].title.empty()
                                       ? entries[i].code
                                       : entries[i].code + L" — " + entries[i].title;
        m_codeCombo.addItem(app::jstr(label), static_cast<int>(i) + 1);
    }
    m_codeCombo.setText(entries.empty() ? juce::String(L"Código") : previous,
                        juce::dontSendNotification);
    if (m_codeCombo.getNumItems() > 0 && m_codeCombo.getSelectedId() <= 0) {
        m_codeCombo.setSelectedId(1, juce::dontSendNotification);
    }
}

void BlockEditor::rebuild()
{
    rebuildRows();
    repaint();
}

void BlockEditor::rebuildRows()
{
    const std::vector<bool> previousSelection = m_rowSelected;

    m_rows.clear();
    m_rows.reserve(m_doc.lines().size());
    const juce::Font chipFont = monoFont(13.0f);
    for (size_t i = 0; i < m_doc.lines().size(); ++i) {
        const blocos::BlockDocument::Line& line = m_doc.lines()[i];
        if (line.kind != blocos::BlockDocument::Line::Kind::Horario) {
            continue;
        }
        Row r;
        r.lineIndex = static_cast<int>(i);
        r.time = line.horario.time;
        r.timeStr = app::jstr(r.time);
        r.trailing = line.horario.trailing;
        r.trailingStr = app::jstr(r.trailing);
        for (const std::wstring& code : line.horario.codes) {
            r.colourIndex.push_back(m_catalogue.indexOf(code));
            const juce::String cs = app::jstr(code);
            r.chipStr.push_back(cs);
            r.chipW.push_back(static_cast<int>(
                juce::GlyphArrangement::getStringWidth(chipFont, cs)));
        }
        m_rows.push_back(std::move(r));
    }

    m_rowSelected.assign(m_rows.size(), false);
    const size_t keep = juce::jmin(previousSelection.size(), m_rowSelected.size());
    for (size_t k = 0; k < keep; ++k) {
        m_rowSelected[k] = previousSelection[k];
    }

    if (m_selectedRow >= static_cast<int>(m_rows.size())) {
        m_selectedRow = -1;
    }
    clampScroll();
    updateSelectCheck();
    repaint();
}

void BlockEditor::refreshRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_rows.size())) {
        return;
    }
    Row& r = m_rows[static_cast<size_t>(rowIndex)];
    const auto& lines = m_doc.lines();
    const int lineIndex = r.lineIndex;
    if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= lines.size()) {
        return;
    }
    const blocos::BlockDocument::Line& line =
        lines[static_cast<size_t>(lineIndex)];
    if (line.kind != blocos::BlockDocument::Line::Kind::Horario) {
        return;
    }

    r.time = line.horario.time;
    r.timeStr = app::jstr(r.time);
    r.trailing = line.horario.trailing;
    r.trailingStr = app::jstr(r.trailing);
    r.colourIndex.clear();
    r.chipStr.clear();
    r.chipW.clear();
    const juce::Font chipFont = monoFont(13.0f);
    for (const std::wstring& code : line.horario.codes) {
        r.colourIndex.push_back(m_catalogue.indexOf(code));
        const juce::String cs = app::jstr(code);
        r.chipStr.push_back(cs);
        r.chipW.push_back(static_cast<int>(
            juce::GlyphArrangement::getStringWidth(chipFont, cs)));
    }
    repaint(rowRect(rowIndex));
}

void BlockEditor::notifyChanged()
{
    if (onChange) {
        onChange();
    }
}

void BlockEditor::setStatus(const juce::String& text)
{
    m_statusLabel.setText(text, juce::dontSendNotification);
}

// ------------------------------------------------------------------
// Desenho
// ------------------------------------------------------------------

int BlockEditor::chipsBaseX() const
{
    return kMargin + (m_selectMode ? kSelW : 0) + kTimeWidth + kChipGap;
}

juce::Rectangle<int> BlockEditor::rowRect(int rowIndex) const
{
    return juce::Rectangle<int>(
        0, m_listRect.getY() + rowIndex * kRowHeight - m_scrollY,
        m_listRect.getWidth(), kRowHeight);
}

void BlockEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff242424));

    for (const int sx : { m_sep1X, m_sep2X }) {
        if (sx >= 0) {
            g.setColour(juce::Colour(0xff3f3f3f));
            g.fillRect(sx, m_toolArea.getY() + 3, 1,
                       juce::jmax(0, m_toolArea.getHeight() - 6));
        }
    }

    const int width = getWidth();

    if (m_rows.empty()) {
        g.setColour(juce::Colours::grey);
        g.setFont(monoFont(14.0f));
        g.drawText(L"Nenhum horário ainda — use o campo HH:MM + OK, ou "
                   L"Preencher Horários.",
                   m_listRect.reduced(16, 0), juce::Justification::centredLeft,
                   true);
        return;
    }

    const int listTop = m_listRect.getY();
    const int listHeight = m_listRect.getHeight();
    const int firstRow = m_scrollY / kRowHeight;
    const int lastRow = (m_scrollY + listHeight) / kRowHeight;

    for (int i = firstRow; i <= lastRow && i < static_cast<int>(m_rows.size());
         ++i) {
        const Row& row = m_rows[static_cast<size_t>(i)];
        const int y = listTop + i * kRowHeight - m_scrollY;
        if (y + kRowHeight < listTop || y > listTop + listHeight) {
            continue;
        }
        const juce::Rectangle<int> rowArea(0, y, width, kRowHeight);
        const bool rowMarked = m_rowSelected[static_cast<size_t>(i)];

        juce::Colour bg = juce::Colour(0xff2b2b2b);
        if (isArmed() && i == m_hoverRow) {
            bg = juce::Colour(0xff4a4a4a);
        } else if (rowMarked) {
            bg = juce::Colour(0xff3d3d3d);
        }
        g.setColour(bg);
        g.fillRect(rowArea);

        const int timeX = kMargin + (m_selectMode ? kSelW : 0);
        if (m_selectMode) {
            const juce::Rectangle<int> box(2, y + (kRowHeight - 14) / 2, 14, 14);
            g.setColour(rowMarked ? juce::Colour(0xff26a69a)
                                  : juce::Colour(0xff4a4a4a));
            g.fillRoundedRectangle(box.toFloat(), 3.0f);
            if (rowMarked) {
                g.setColour(juce::Colours::white);
                g.setFont(monoFont(12.0f, true));
                g.drawText(L"\u2713", box, juce::Justification::centred, true);
            }
        }

        g.setColour(juce::Colours::white);
        g.setFont(monoFont(14.0f, true));
        g.drawText(row.timeStr,
                   juce::Rectangle<int>(timeX, y, kTimeWidth, kRowHeight),
                   juce::Justification::centredLeft, true);

        int cx = chipsBaseX();
        const juce::Font chipFont = monoFont(13.0f);
        for (int c = 0; c < static_cast<int>(row.chipStr.size()); ++c) {
            const int tw = row.chipW[static_cast<size_t>(c)];
            const int chipW = tw + 2 * kChipPadding;
            if (cx + chipW > width - kMargin) {
                break;
            }
            const juce::Rectangle<int> chipRect(cx, y + (kRowHeight - 18) / 2,
                                                chipW, 18);
            const int ci = row.colourIndex[static_cast<size_t>(c)];
            g.setColour(ci >= 0 ? codeColour(ci) : juce::Colour(0xff9e9e9e));
            g.fillRoundedRectangle(chipRect.toFloat(), 5.0f);
            g.setColour(juce::Colours::black);
            g.setFont(chipFont);
            g.drawText(row.chipStr[static_cast<size_t>(c)], chipRect,
                       juce::Justification::centred, true);
            cx = chipRect.getRight() + kChipGap;
        }

        if (!row.trailing.empty()) {
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(cx, y, width - cx, kRowHeight));
            g.setColour(juce::Colours::lightgrey);
            g.setFont(monoFont(12.0f));
            g.drawText(row.trailingStr,
                       juce::Rectangle<int>(cx + kChipGap, y,
                                            width - cx - kChipGap, kRowHeight),
                       juce::Justification::centredLeft, true);
            g.restoreState();
        }

        g.setColour(juce::Colour(0xff333333));
        g.fillRect(0, y + kRowHeight - 1, width, 1);
    }

    const int contentH = contentHeight();
    if (contentH > listHeight) {
        const juce::Rectangle<int> thumb = sliderThumb();
        const int sx = thumb.getX();
        g.setColour(juce::Colour(0xff2f2f2f));
        g.fillRect(sx, listTop, kSliderW, listHeight);
        g.setColour(m_dragSlider ? juce::Colour(0xff6a6a6a)
                                 : juce::Colour(0xff505050));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(static_cast<float>(sx + kSliderGap),
                                   static_cast<float>(thumb.getY()),
                                   static_cast<float>(kSliderW - 2 * kSliderGap),
                                   static_cast<float>(thumb.getHeight())),
            3.0f);
    }

    if (isArmed()) {
        const juce::Rectangle<int> ghost = ghostRect(m_mousePos);
        if (!ghost.isEmpty()) {
            g.setColour(codeColour(m_catalogue.indexOf(armedCode()))
                            .withAlpha(0.85f));
            g.fillRoundedRectangle(ghost.toFloat(), 6.0f);
            g.setColour(juce::Colours::black);
            g.setFont(monoFont(13.0f, true));
            g.drawText(app::jstr(armedCode()), ghost, juce::Justification::centred,
                       true);
        }
    }
}

void BlockEditor::resized()
{
    const int bandH = 26;
    const int gap = 6;

    // Painel de códigos: altura = fileiras (2 por padrão) + dica.
    m_panelH = m_codePanel.preferredHeight();
    m_codePanel.setBounds(0, 0, getWidth(), m_panelH);

    int x = kMargin;
    const int y1 = m_panelH + 4;
    const auto placeW = [&x, &gap, &bandH](juce::Component& c, int y, int w) {
        c.setBounds(x, y, w, bandH);
        x = c.getRight() + gap;
    };

    const int hintY = y1;
    x = kMargin;
    placeW(m_selectBtn, hintY, 90);
    m_codeCombo.setBounds(x, hintY, 160, bandH);
    x = m_codeCombo.getRight() + gap;
    placeW(m_applyBtn, hintY, 74);
    placeW(m_removeBtn, hintY, 84);
    m_sep1X = x;
    x += 8;
    placeW(m_fillBtn, hintY, 132);
    m_timeField.setBounds(x, hintY, 90, bandH);
    x = m_timeField.getRight() + gap;
    placeW(m_addTimeBtn, hintY, 82);
    m_sep2X = x;
    x += 8;
    m_copyBtn.setBounds(getWidth() - kMargin - 80, hintY, 80, bandH);
    m_pasteBtn.setBounds(getWidth() - kMargin - 80 - gap - 80, hintY, 80, bandH);
    m_toolArea = juce::Rectangle<int>(0, hintY, getWidth(), bandH);

    // A dica (código armado / como usar) fica numa faixa própria logo abaixo
    // da barra de ferramentas: assim nunca disputa espaço com os botões e
    // continua visível em qualquer largura de janela.
    const int hintBandH = 18;
    const int hintTop = y1 + bandH + 2;
    m_hintLabel.setBounds(kMargin, hintTop, getWidth() - 2 * kMargin, hintBandH);

    const int statusH = 20;
    const int statusTop = getHeight() - statusH - 4;
    m_statusLabel.setBounds(kMargin, statusTop, getWidth() - 2 * kMargin, statusH);

    const int listTop = hintTop + hintBandH + gap;
    const int listBottom = statusTop - 2;
    m_listRect = juce::Rectangle<int>(0, listTop, getWidth(),
                                      juce::jmax(0, listBottom - listTop));
    clampScroll();
}

// ------------------------------------------------------------------
// Mouse / teclado
// ------------------------------------------------------------------

std::pair<int, int> BlockEditor::rowAt(const juce::Point<int>& pos) const
{
    if (!m_listRect.contains(pos) || m_rows.empty()) {
        return { -1, -1 };
    }
    const int i = (pos.getY() - m_listRect.getY() + m_scrollY) / kRowHeight;
    if (i < 0 || i >= static_cast<int>(m_rows.size())) {
        return { -1, -1 };
    }
    return { i, chipsBaseX() };
}

int BlockEditor::chipAtX(const Row& row, int x) const
{
    int cx = chipsBaseX();
    for (int c = 0; c < static_cast<int>(row.chipStr.size()); ++c) {
        const int chipW = row.chipW[static_cast<size_t>(c)] + 2 * kChipPadding;
        if (x >= cx && x <= cx + chipW) {
            return c;
        }
        cx += chipW + kChipGap;
    }
    return -1;
}

void BlockEditor::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    if (contentHeight() > m_listRect.getHeight() &&
        overSliderStrip(event.getPosition())) {
        if (event.mods.isRightButtonDown()) {
            return;
        }
        const juce::Rectangle<int> thumb = sliderThumb();
        if (thumb.contains(event.getPosition())) {
            m_dragSlider = true;
            m_dragStartThumbY = event.getPosition().getY();
            m_dragStartScrollY = m_scrollY;
            repaint();
        } else {
            const int listHeight = m_listRect.getHeight();
            const int maxScroll = juce::jmax(0, contentHeight() - listHeight);
            m_scrollY = juce::jlimit(0, maxScroll,
                                     m_scrollY + (event.getPosition().getY() < thumb.getY()
                                                      ? -listHeight
                                                      : listHeight));
            repaint();
        }
        return;
    }

    const auto [row, chipsX] = rowAt(event.getPosition());
    (void)chipsX;

    if (event.mods.isRightButtonDown()) {
        if (isArmed()) {
            m_codePanel.cancelArmed();
            repaint();
            return;
        }
        if (row < 0) {
            return;
        }
        const Row& r = m_rows[static_cast<size_t>(row)];
        const int chip = chipAtX(r, event.getPosition().getX());
        if (chip < 0) {
            selectRow(row);
            return;
        }
        m_doc.removeCode(r.lineIndex, chip);
        refreshRow(row);
        notifyChanged();
        setStatus(L"Código removido de " + r.timeStr + L".");
        return;
    }

    if (row < 0) {
        return;
    }

    if (isArmed()) {
        const std::wstring code = armedCode();
        const bool added = m_doc.addCode(m_rows[static_cast<size_t>(row)].lineIndex, code);
        refreshRow(row);
        notifyChanged();
        if (added) {
            setStatus(L"Código " + app::jstr(code) + L" adicionado a " +
                      m_rows[static_cast<size_t>(row)].timeStr + L".");
        } else {
            setStatus(L"Código " + app::jstr(code) + L" já está em " +
                      m_rows[static_cast<size_t>(row)].timeStr + L".");
        }
        return; // permanece armado
    }

    if (m_selectMode) {
        toggleSelectedRow(row);
    } else {
        selectRow(row);
    }
}

void BlockEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (!m_dragSlider) {
        return;
    }
    const int listHeight = m_listRect.getHeight();
    const int contentH = contentHeight();
    if (contentH <= listHeight) {
        m_dragSlider = false;
        return;
    }
    const int thumbH = sliderThumbHeight();
    const int travel = juce::jmax(1, listHeight - thumbH);
    const int dy = event.getPosition().getY() - m_dragStartThumbY;
    const int newScroll = m_dragStartScrollY + dy * (contentH - listHeight) / travel;
    m_scrollY = juce::jlimit(0, juce::jmax(0, contentH - listHeight), newScroll);
    repaint();
}

void BlockEditor::mouseUp(const juce::MouseEvent& event)
{
    if (m_dragSlider) {
        m_dragSlider = false;
        repaint();
    }
    juce::Component::mouseUp(event);
}

void BlockEditor::mouseMove(const juce::MouseEvent& event)
{
    const bool overList = m_listRect.contains(event.getPosition());
    const int hover = overList ? rowAt(event.getPosition()).first : -1;
    const juce::Rectangle<int> oldGhost = ghostRect(m_mousePos);
    m_mousePos = event.getPosition();
    m_mouseOverList = overList;

    if (hover != m_hoverRow) {
        if (m_hoverRow >= 0) {
            repaint(rowRect(m_hoverRow));
        }
        m_hoverRow = hover;
        if (m_hoverRow >= 0) {
            repaint(rowRect(m_hoverRow));
        }
    }
    if (isArmed()) {
        const juce::Rectangle<int> newGhost = ghostRect(m_mousePos);
        if (oldGhost.isEmpty()) {
            repaint(newGhost);
        } else if (newGhost.isEmpty()) {
            repaint(oldGhost);
        } else {
            repaint(oldGhost.getUnion(newGhost));
        }
    }
}

void BlockEditor::mouseExit(const juce::MouseEvent& event)
{
    juce::Component::mouseExit(event);
    m_mouseOverList = false;
    m_hoverRow = -1;
    repaint();
}

void BlockEditor::mouseWheelMove(const juce::MouseEvent& event,
                                 const juce::MouseWheelDetails& wheel)
{
    const int listHeight = m_listRect.getHeight();
    if (contentHeight() <= listHeight) {
        juce::Component::mouseWheelMove(event, wheel);
        return;
    }
    float dy = wheel.deltaY;
    if (wheel.isReversed) {
        dy = -dy;
    }
    const float rowsPerNotch = 3.0f * std::max(1.0f, std::abs(dy));
    int step = static_cast<int>(rowsPerNotch * kRowHeight);
    if (dy > 0.0f) {
        step = -step;
    }
    m_scrollY = juce::jlimit(0, juce::jmax(0, contentHeight() - listHeight),
                             m_scrollY + step);
    repaint();
}

bool BlockEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) {
        if (isArmed()) {
            m_codePanel.cancelArmed();
            repaint();
            return true;
        }
        return false;
    }
    if (key.getModifiers().isCtrlDown() &&
        std::towupper(static_cast<wchar_t>(key.getKeyCode())) == L'C') {
        copySelected();
        return true;
    }
    if (key.getModifiers().isCtrlDown() &&
        std::towupper(static_cast<wchar_t>(key.getKeyCode())) == L'V') {
        pasteSelected();
        return true;
    }
    return false;
}

// ------------------------------------------------------------------
// Seleção
// ------------------------------------------------------------------

void BlockEditor::selectRow(int index)
{
    m_selectedRow = (index >= 0 && index < static_cast<int>(m_rows.size()))
                        ? index
                        : -1;
    refreshButtons();
    repaint(rowRect(index));
}

bool BlockEditor::hasSelection() const
{
    if (m_selectMode) {
        for (const bool marked : m_rowSelected) {
            if (marked) {
                return true;
            }
        }
        return false;
    }
    return m_selectedRow >= 0;
}

std::vector<int> BlockEditor::selectedRowIndices() const
{
    std::vector<int> out;
    if (m_selectMode) {
        bool anyMarked = false;
        for (const bool marked : m_rowSelected) {
            if (marked) {
                anyMarked = true;
                break;
            }
        }
        if (anyMarked) {
            for (size_t k = 0; k < m_rowSelected.size(); ++k) {
                if (m_rowSelected[k]) {
                    out.push_back(static_cast<int>(k));
                }
            }
            return out;
        }
    }
    if (m_selectedRow >= 0) {
        out.push_back(m_selectedRow);
    }
    return out;
}

void BlockEditor::toggleSelectedRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_rows.size())) {
        return;
    }
    const size_t idx = static_cast<size_t>(rowIndex);
    m_rowSelected[idx] = !m_rowSelected[idx];
    m_selectedRow = rowIndex;
    refreshButtons();
    repaint(rowRect(rowIndex));
}

void BlockEditor::selectAllRows()
{
    if (m_rows.empty()) {
        return;
    }
    std::fill(m_rowSelected.begin(), m_rowSelected.end(), true);
    m_selectedRow = -1;
    refreshButtons();
    repaint();
    setStatus(L"Todos os " + juce::String(static_cast<int>(m_rows.size())) +
              L" horários selecionados.");
}

void BlockEditor::selectNoRows()
{
    std::fill(m_rowSelected.begin(), m_rowSelected.end(), false);
    refreshButtons();
    repaint();
    setStatus(L"Seleção limpa.");
}

void BlockEditor::onSelectCheckClick()
{
    if (!m_selectMode) {
        m_selectMode = true;
        updateSelectCheck();
        repaint();
        return;
    }
    bool allSelected = !m_rowSelected.empty();
    for (const bool marked : m_rowSelected) {
        if (!marked) {
            allSelected = false;
            break;
        }
    }
    if (allSelected) {
        selectNoRows();
    } else {
        selectAllRows();
    }
}

void BlockEditor::updateSelectCheck()
{
    bool allSelected = !m_rowSelected.empty();
    for (const bool marked : m_rowSelected) {
        if (!marked) {
            allSelected = false;
            break;
        }
    }
    m_selectBtn.setState(m_selectMode, allSelected);
}

void BlockEditor::SelectCheckButton::setState(bool modeOn, bool allSelected)
{
    if (m_modeOn == modeOn && m_allSelected == allSelected) {
        return;
    }
    m_modeOn = modeOn;
    m_allSelected = allSelected;
    repaint();
}

void BlockEditor::SelectCheckButton::paintButton(juce::Graphics& g, bool, bool)
{
    const juce::Rectangle<float> area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillRoundedRectangle(area.reduced(1.0f), 4.0f);

    const float size = juce::jmin(area.getHeight() - 8.0f, 18.0f);
    const juce::Rectangle<float> box(area.getX() + 5.0f,
                                     area.getCentreY() - size / 2.0f, size, size);
    if (m_modeOn && m_allSelected) {
        g.setColour(juce::Colour(0xff26a69a));
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(size * 0.8f, juce::Font::bold)));
        g.drawText(L"\u2713", box.toNearestInt(), juce::Justification::centred,
                   true);
    } else {
        g.setColour(m_modeOn ? juce::Colours::lightgrey : juce::Colour(0xff777777));
        g.drawRoundedRectangle(box, 3.0f, 1.5f);
    }

    const wchar_t* label = m_modeOn ? (m_allSelected ? L"Todos" : L"Seleção")
                                    : L"Selecionar";
    g.setColour(m_modeOn ? juce::Colours::white : juce::Colour(0xff9a9a9a));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(label,
               juce::Rectangle<int>(box.toNearestInt().getRight() + 5,
                                    getLocalBounds().getY(),
                                    getLocalBounds().getWidth(),
                                    getLocalBounds().getHeight()),
               juce::Justification::centredLeft, true);
}

// ------------------------------------------------------------------
// Rolagem
// ------------------------------------------------------------------

int BlockEditor::contentHeight() const
{
    return static_cast<int>(m_rows.size()) * kRowHeight;
}

int BlockEditor::sliderThumbHeight() const
{
    const int listHeight = m_listRect.getHeight();
    const int contentH = contentHeight();
    if (contentH <= listHeight) {
        return 0;
    }
    return juce::jmax(22, static_cast<int>(listHeight * listHeight / contentH));
}

juce::Rectangle<int> BlockEditor::sliderThumb() const
{
    const int listHeight = m_listRect.getHeight();
    const int contentH = contentHeight();
    if (contentH <= listHeight) {
        return {};
    }
    const int thumbH = sliderThumbHeight();
    const int travel = listHeight - thumbH;
    const int thumbY =
        m_listRect.getY() +
        (travel > 0 ? static_cast<int>(m_scrollY * travel / (contentH - listHeight))
                    : 0);
    return juce::Rectangle<int>(m_listRect.getRight() - kSliderW, thumbY, kSliderW,
                                thumbH);
}

bool BlockEditor::overSliderStrip(const juce::Point<int>& pos) const
{
    return m_listRect.contains(pos) &&
           pos.getX() >= m_listRect.getRight() - kSliderW;
}

void BlockEditor::clampScroll()
{
    const int maxScroll = contentHeight() - m_listRect.getHeight();
    m_scrollY = juce::jlimit(0, juce::jmax(0, maxScroll), m_scrollY);
}

void BlockEditor::refreshButtons()
{
    const bool any = hasSelection();
    m_copyBtn.setEnabled(any);
    m_pasteBtn.setEnabled(!m_clipboard.empty());
    m_removeBtn.setEnabled(any);
    m_applyBtn.setEnabled(any);
    updateSelectCheck();

    if (isArmed()) {
        m_hintLabel.setText(L"Código " + app::jstr(armedCode()) +
                                L" armado — clique num horário para adicioná-lo "
                                L"(botão direito ou ESC cancela)",
                            juce::dontSendNotification);
    } else {
        m_hintLabel.setText(
            L"Clique num código e depois num horário para adicioná-lo.",
            juce::dontSendNotification);
    }
}

juce::Rectangle<int> BlockEditor::ghostRect(const juce::Point<int>& pos) const
{
    const int width = getWidth();
    if (!isArmed() || m_mouseOverList == false || !m_listRect.contains(pos)) {
        return {};
    }
    const juce::Font f = monoFont(13.0f, true);
    const juce::String label = app::jstr(armedCode());
    const int tw = static_cast<int>(juce::GlyphArrangement::getStringWidth(f, label));
    const int w = tw + 16;
    juce::Rectangle<int> chipRect(pos.getX() + 8, pos.getY() + 12, w, 20);
    if (chipRect.getRight() > width - kMargin) {
        chipRect.setRight(kMargin + w);
    }
    if (chipRect.getBottom() > m_listRect.getBottom()) {
        chipRect.setY(m_listRect.getBottom() - 24);
    }
    return chipRect;
}

int BlockEditor::findTimeLine(const std::wstring& hhmm) const
{
    for (const Row& row : m_rows) {
        if (row.time == hhmm) {
            return row.lineIndex;
        }
    }
    return -1;
}

// ------------------------------------------------------------------
// Ações
// ------------------------------------------------------------------

void BlockEditor::addFromField()
{
    std::wstring value = app::wstr(m_timeField.getText());
    trimWs(value);
    if (value.empty()) {
        setStatus(L"Digite um horário no formato HH:MM (ex.: 12:30).");
        return;
    }
    if (blocos::BlockDocument::parseTime(value) < 0) {
        setStatus(L"Horário inválido — use o formato HH:MM.");
        return;
    }
    if (m_doc.addTime(value) >= 0) {
        rebuildRows();
        m_timeField.clear();
        notifyChanged();
        setStatus(L"Horário " + app::jstr(value) + L" adicionado.");
    } else {
        setStatus(L"Horário " + app::jstr(value) + L" já existe.");
    }
}

void BlockEditor::fillTimes()
{
    const FillSpec spec = askFill(*this);
    if (!spec.ok) {
        if (!spec.error.empty()) {
            setStatus(app::jstr(spec.error));
        }
        return;
    }
    const int start = blocos::BlockDocument::parseTime(spec.inicio);
    if (start < 0) {
        return;
    }
    std::vector<std::wstring> times;
    for (int t = start; t < 1440; t += spec.intervalo) {
        times.push_back(blocos::BlockDocument::formatTime(t));
        if (times.size() > 2048) {
            break;
        }
    }
    m_doc.reschedule(times);
    rebuildRows();
    notifyChanged();
    setStatus(L"Preencher: " + juce::String(static_cast<int>(times.size())) +
              L" horários de " + app::jstr(spec.inicio) + L" em " +
              juce::String(spec.intervalo) + L" minutos.");
}

void BlockEditor::removeSelected()
{
    const std::vector<int> targetRows = selectedRowIndices();
    if (targetRows.empty()) {
        setStatus(L"Selecione um horário para remover (clique nele).");
        return;
    }

    juce::AlertWindow window(
        L"Remover",
        L"O que você deseja remover nos horários selecionados?",
        juce::MessageBoxIconType::NoIcon, this);
    window.addButton(L"Horários", 1,
                     juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    window.addButton(L"Códigos", 2);
    window.addButton(L"Cancelar", 0,
                     juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));
    window.enterModalState(true);
    const int choice = window.runModalLoop();
    if (choice == 0) {
        return;
    }

    if (choice == 2) {
        int removed = 0;
        for (const int row : targetRows) {
            const int lineIndex = m_rows[static_cast<size_t>(row)].lineIndex;
            const auto& lines = m_doc.lines();
            if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= lines.size()) {
                continue;
            }
            while (m_doc.lines()[static_cast<size_t>(lineIndex)].horario.codes.size()
                   > 0) {
                m_doc.removeCode(lineIndex, 0);
                ++removed;
            }
            refreshRow(row);
        }
        notifyChanged();
        setStatus(juce::String(removed) + L" código(s) removido(s); os horários "
                  L"foram mantidos.");
        return;
    }

    std::vector<int> lineIndexes;
    lineIndexes.reserve(targetRows.size());
    for (const int row : targetRows) {
        lineIndexes.push_back(m_rows[static_cast<size_t>(row)].lineIndex);
    }
    std::sort(lineIndexes.begin(), lineIndexes.end(), std::greater<int>());
    int removed = 0;
    for (const int lineIndex : lineIndexes) {
        m_doc.removeLine(lineIndex);
        ++removed;
    }
    rebuildRows();
    m_selectedRow = -1;
    refreshButtons();
    notifyChanged();
    setStatus(juce::String(removed) + L" horário(s) removido(s).");
}

void BlockEditor::applyCodeToSelected()
{
    const std::vector<int> targetRows = selectedRowIndices();
    if (targetRows.empty()) {
        setStatus(L"Selecione um horário para aplicar o código.");
        return;
    }
    const int comboId = m_codeCombo.getSelectedId();
    if (comboId <= 0 || static_cast<size_t>(comboId) > m_catalogue.size()) {
        setStatus(L"Escolha um código no seletor.");
        return;
    }
    const std::wstring code = m_catalogue.entries()[static_cast<size_t>(comboId - 1)].code;

    int applied = 0;
    for (const int row : targetRows) {
        const int lineIndex = m_rows[static_cast<size_t>(row)].lineIndex;
        if (m_doc.addCode(lineIndex, code)) {
            ++applied;
            refreshRow(row);
        }
    }
    refreshButtons();
    notifyChanged();
    setStatus(L"Código " + app::jstr(code) + L" aplicado a " +
              juce::String(applied) + L" horário(s).");
}

void BlockEditor::copySelected()
{
    const std::vector<int> targetRows = selectedRowIndices();
    if (targetRows.empty()) {
        setStatus(L"Selecione um horário para copiar (clique nele).");
        return;
    }
    m_clipboard.clear();
    m_clipboard.reserve(targetRows.size());
    for (const int row : targetRows) {
        const int lineIndex = m_rows[static_cast<size_t>(row)].lineIndex;
        BlockClip entry;
        entry.time = m_rows[static_cast<size_t>(row)].time;
        entry.codes = m_doc.lines()[static_cast<size_t>(lineIndex)].horario.codes;
        m_clipboard.push_back(std::move(entry));
    }
    refreshButtons();
    setStatus(juce::String(static_cast<int>(m_clipboard.size())) +
              L" horário(s) copiado(s) — cole no mesmo ou em outro arquivo.");
}

void BlockEditor::pasteSelected()
{
    if (m_clipboard.empty()) {
        setStatus(L"Nada para colar — copie horários de outro arquivo.");
        return;
    }
    int added = 0;
    int replaced = 0;
    for (const BlockClip& entry : m_clipboard) {
        const int existing = findTimeLine(entry.time);
        int lineIndex = existing;
        if (lineIndex < 0) {
            lineIndex = m_doc.addTime(entry.time);
            if (lineIndex < 0) {
                continue;
            }
            ++added;
        } else {
            ++replaced;
        }
        // Os códigos vêm VERBATIM, com repetições: colar a primeira linha do
        // Mapa real tem de trazer o COMER cinco vezes.
        m_doc.replaceCodes(lineIndex, entry.codes);
    }
    rebuildRows();
    refreshButtons();
    notifyChanged();
    setStatus(juce::String(added) + L" horário(s) criado(s), " +
              juce::String(replaced) + L" horário(s) com códigos substituídos.");
}

} // namespace app
