#include "app/RelogioEditor.h"

#include <cmath>
#include <cwctype>

#include "app/JuceHelpers.h"

namespace {
constexpr int kRowHeight = 28;
constexpr int kMargin = 8;
constexpr int kTimeWidth = 62;
constexpr int kChipGap = 4;
constexpr int kChipPadding = 7;

juce::Font monoFont(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(
        juce::Font::getDefaultMonospacedFontName(),
        bold ? juce::String("Bold") : juce::String("Regular"), size));
}

// Nome canônico de cada parâmetro (para botões da paleta e chips).
const wchar_t* paramNameWide(relogio::ParamKind kind)
{
    switch (kind) {
    case relogio::ParamKind::Fixo:     return L"FIXO";
    case relogio::ParamKind::Descarte: return L"DESCARTE";
    case relogio::ParamKind::Local:    return L"LOCAL";
    case relogio::ParamKind::Sat:      return L"SAT";
    case relogio::ParamKind::Locked:   return L"LOCKED";
    case relogio::ParamKind::Id:       return L"ID";
    case relogio::ParamKind::Dur:      return L"DUR";
    }
    return L"";
}

bool paramHasValue(relogio::ParamKind kind)
{
    return kind == relogio::ParamKind::Id || kind == relogio::ParamKind::Dur;
}

// "(FIXO)" ou "(ID=valor)".
std::wstring paramFull(relogio::ParamKind kind, const std::wstring& value)
{
    std::wstring s = L"(";
    s += paramNameWide(kind);
    if (paramHasValue(kind) && !value.empty()) {
        s += L"=" + value;
    }
    s += L")";
    return s;
}

// Diálogo modal com um campo de texto; devolve vazio se o usuário cancelar.
std::wstring askDialog(const juce::String& title, const juce::String& message,
                       const juce::String& field, juce::Component& parent)
{
    juce::AlertWindow window(title, message, juce::MessageBoxIconType::NoIcon,
                             &parent);
    window.addTextEditor(L"valor", juce::String(), field);
    window.addButton(L"OK", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    window.addButton(L"Cancelar", 0,
                     juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));
    window.enterModalState(true);
    if (window.runModalLoop() != 1) {
        return std::wstring();
    }
    return app::wstr(window.getTextEditorContents(L"valor"));
}

} // namespace

namespace app {

juce::Colour paramColour(relogio::ParamKind kind)
{
    switch (kind) {
    case relogio::ParamKind::Fixo:     return juce::Colour(0xffffa726);
    case relogio::ParamKind::Descarte: return juce::Colour(0xffe53935);
    case relogio::ParamKind::Local:    return juce::Colour(0xff42a5f5);
    case relogio::ParamKind::Sat:      return juce::Colour(0xff26a69a);
    case relogio::ParamKind::Locked:   return juce::Colour(0xffab47bc);
    case relogio::ParamKind::Id:       return juce::Colour(0xff66bb6a);
    case relogio::ParamKind::Dur:      return juce::Colour(0xff29b6f6);
    }
    return juce::Colours::grey;
}

std::wstring askParamValue(relogio::ParamKind kind, juce::Component& parent)
{
    const bool id = (kind == relogio::ParamKind::Id);
    return askDialog(id ? L"Valor do ID" : L"Valor da duração",
                     id ? L"Digite o identificador do horário (ex.: Noticia)."
                        : L"Digite a duração do horário (ex.: 3:00).",
                     id ? L"ID" : L"DURAÇÃO", parent);
}

RelogioEditor::RelogioEditor(relogio::RelogioDocument& doc,
                             std::vector<relogio::Param>& clipboard)
    : m_doc(doc), m_clipboard(clipboard)
{
    // Paleta de parâmetros ("grudado no mouse").
    m_fixoBtn.onClick = [this] { armParam(relogio::ParamKind::Fixo); };
    m_descarteBtn.onClick = [this] { armParam(relogio::ParamKind::Descarte); };
    m_localBtn.onClick = [this] { armParam(relogio::ParamKind::Local); };
    m_satBtn.onClick = [this] { armParam(relogio::ParamKind::Sat); };
    m_lockedBtn.onClick = [this] { armParam(relogio::ParamKind::Locked); };
    m_idBtn.onClick = [this] { armParam(relogio::ParamKind::Id); };
    m_durBtn.onClick = [this] { armParam(relogio::ParamKind::Dur); };

    const auto stylePalette = [this](juce::TextButton& btn, relogio::ParamKind k) {
        btn.setColour(juce::TextButton::buttonColourId, paramColour(k));
        btn.setColour(juce::TextButton::buttonOnColourId, paramColour(k).brighter(0.25f));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        btn.setTooltip(juce::String(
            L"Arma o parâmetro " +
            app::jstr(paramFull(k, paramHasValue(k) ? L"..." : L"")) +
            L" para adicioná-lo a um horário (grudado no mouse)."));
        addAndMakeVisible(btn);
    };
    stylePalette(m_fixoBtn, relogio::ParamKind::Fixo);
    stylePalette(m_descarteBtn, relogio::ParamKind::Descarte);
    stylePalette(m_localBtn, relogio::ParamKind::Local);
    stylePalette(m_satBtn, relogio::ParamKind::Sat);
    stylePalette(m_lockedBtn, relogio::ParamKind::Locked);
    stylePalette(m_idBtn, relogio::ParamKind::Id);
    stylePalette(m_durBtn, relogio::ParamKind::Dur);

    m_hintLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_hintLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    m_hintLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_hintLabel);

    // Empregos de horário.
    m_iniBtn.onClick = [this] { addInicio(); };
    m_intBtn.onClick = [this] { addIntervalo(); };
    m_avulsoBtn.onClick = [this] { addAvulso(); };
    m_copyBtn.onClick = [this] { copySelected(); };
    m_pasteBtn.onClick = [this] { pasteSelected(); };
    m_iniBtn.setTooltip(L"Adiciona o horário 00:00 (início do dia).");
    m_intBtn.setTooltip(
        L"Adiciona o horário do meio entre o horário selecionado e o seguinte.");
    m_avulsoBtn.setTooltip(L"Adiciona um horário digitado (HH:MM).");
    m_copyBtn.setTooltip(
        L"Copiar os parâmetros do horário selecionado (Ctrl+C) — "
        L"compartilhado entre os relógios.");
    m_pasteBtn.setTooltip(
        L"Colar os parâmetros copiados no horário selecionado (Ctrl+V).");
    for (juce::TextButton* btn :
         { &m_iniBtn, &m_intBtn, &m_avulsoBtn, &m_copyBtn, &m_pasteBtn }) {
        addAndMakeVisible(btn);
    }

    m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_statusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    m_statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_statusLabel);

    setWantsKeyboardFocus(true);
    rebuildRows();
    updateArmedUi();
    refreshButtons();
}

RelogioEditor::~RelogioEditor() = default;

void RelogioEditor::rebuild()
{
    rebuildRows();
    repaint();
}

void RelogioEditor::rebuildRows()
{
    m_rows.clear();
    for (size_t i = 0; i < m_doc.lines().size(); ++i) {
        const relogio::RelogioDocument::Line& line = m_doc.lines()[i];
        if (line.kind != relogio::RelogioDocument::Line::Kind::Horario) {
            continue;
        }
        Row r;
        r.lineIndex = static_cast<int>(i);
        r.time = line.horario.time;
        r.trailing = line.horario.trailing;
        for (const relogio::Param& p : line.horario.params) {
            r.kinds.push_back(p.kind);
            r.chipTexts.push_back(relogio::RelogioDocument::paramText(p));
        }
        m_rows.push_back(std::move(r));
    }
    if (m_selectedRow >= static_cast<int>(m_rows.size())) {
        m_selectedRow = -1;
    }
}

void RelogioEditor::notifyChanged()
{
    if (onChange) {
        onChange();
    }
}

void RelogioEditor::setStatus(const juce::String& text)
{
    m_statusLabel.setText(text, juce::dontSendNotification);
}

void RelogioEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff242424));

    const int width = getWidth();

    if (m_rows.empty()) {
        g.setColour(juce::Colours::grey);
        g.setFont(monoFont(14.0f));
        g.drawText(L"Nenhum horário ainda — use Início, Intervalo ou Avulso.",
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
        const juce::Rectangle<int> rowRect(0, y, width, kRowHeight);

        // Fundo da linha: selecionada, sob o mouse (com parâmetro armado) ou
        // normal.
        juce::Colour bg = juce::Colour(0xff2b2b2b);
        if (m_armed && i == m_hoverRow) {
            bg = juce::Colour(0xff4a4a4a);
        } else if (i == m_selectedRow) {
            bg = juce::Colour(0xff3d3d3d);
        }
        g.setColour(bg);
        g.fillRect(rowRect);

        // Horário.
        g.setColour(juce::Colours::white);
        g.setFont(monoFont(14.0f, true));
        g.drawText(jstr(row.time),
                   juce::Rectangle<int>(kMargin, y, kTimeWidth, kRowHeight),
                   juce::Justification::centredLeft, true);

        // Chips de parâmetros.
        int cx = kMargin + kTimeWidth + kChipGap;
        const juce::Font chipFont = monoFont(13.0f);
        for (int c = 0; c < static_cast<int>(row.kinds.size()); ++c) {
            const std::wstring text = row.chipTexts[static_cast<size_t>(c)];
            const int tw = static_cast<int>(
        juce::GlyphArrangement::getStringWidth(chipFont, jstr(text)));
            const int chipW = tw + 2 * kChipPadding;
            if (cx + chipW > width - kMargin) {
                break;
            }
            const juce::Rectangle<int> chipRect(
                cx, y + (kRowHeight - 18) / 2, chipW, 18);
            g.setColour(paramColour(row.kinds[static_cast<size_t>(c)]));
            g.fillRoundedRectangle(chipRect.toFloat(), 5.0f);
            g.setColour(juce::Colours::black);
            g.setFont(chipFont);
            g.drawText(jstr(text), chipRect, juce::Justification::centred, true);
            cx = chipRect.getRight() + kChipGap;
        }

        // Conteúdo preservado (resto da linha): cinza, cortado no fim da linha.
        if (!row.trailing.empty()) {
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(cx, y, width - cx, kRowHeight));
            g.setColour(juce::Colours::lightgrey);
            g.setFont(monoFont(12.0f));
            g.drawText(jstr(row.trailing),
                       juce::Rectangle<int>(cx + kChipGap, y,
                                            width - cx - kChipGap, kRowHeight),
                       juce::Justification::centredLeft, true);
            g.restoreState();
        }

        // Separador sutil entre linhas.
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(0, y + kRowHeight - 1, width, 1);
    }

    // Barra de rolagem (quando o conteúdo excede a área visível).
    const int contentH = static_cast<int>(m_rows.size()) * kRowHeight;
    if (contentH > listHeight) {
        const int thumbH = static_cast<int>(listHeight * listHeight / contentH);
        const int thumbY =
            listTop + static_cast<int>(m_scrollY * (listHeight - thumbH) /
                                       static_cast<float>(contentH - listHeight));
        g.setColour(juce::Colour(0xff444444));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(width - 8.0f, static_cast<float>(thumbY), 4.0f,
                                   static_cast<float>(thumbH)),
            2.0f);
    }

    // Chip fantasma do parâmetro armado (segue o mouse).
    if (m_armed && m_mouseOverList && m_listRect.contains(m_mousePos)) {
        const std::wstring label = paramFull(m_armedKind, m_armedValue);
        const juce::Font f = monoFont(13.0f, true);
        const int tw = static_cast<int>(
        juce::GlyphArrangement::getStringWidth(f, jstr(label)));
        const int w = tw + 16;
        juce::Rectangle<int> chipRect(m_mousePos.getX() + 8,
                                      m_mousePos.getY() + 12, w, 20);
        if (chipRect.getRight() > width - kMargin) {
            chipRect.setRight(kMargin + w);
        }
        if (chipRect.getBottom() > m_listRect.getBottom()) {
            chipRect.setY(m_listRect.getBottom() - 24);
        }
        g.setColour(paramColour(m_armedKind).withAlpha(0.85f));
        g.fillRoundedRectangle(chipRect.toFloat(), 6.0f);
        g.setColour(juce::Colours::black);
        g.setFont(f);
        g.drawText(jstr(label), chipRect, juce::Justification::centred, true);
    }
    (void)width;
}

void RelogioEditor::resized()
{
    const int margin = 8;
    const int bandH = 26;
    const int gap = 6;

    int x = margin;
    const auto place = [&x, &gap, &bandH, margin](juce::Component& c, int y) {
        c.setBounds(x, y, 90, bandH);
        x = c.getRight() + gap;
        (void)margin;
    };

    const int y1 = 6;
    x = margin;
    place(m_fixoBtn, y1);
    place(m_descarteBtn, y1);
    place(m_localBtn, y1);
    place(m_satBtn, y1);
    place(m_lockedBtn, y1);
    place(m_idBtn, y1);
    place(m_durBtn, y1);
    m_hintLabel.setBounds(x, y1, juce::jmax(0, getWidth() - x - margin), bandH);

    const int y2 = y1 + bandH + gap;
    x = margin;
    place(m_iniBtn, y2);
    place(m_intBtn, y2);
    place(m_avulsoBtn, y2);
    x = getWidth() - margin;
    for (juce::Component* btn :
         { static_cast<juce::Component*>(&m_copyBtn),
           static_cast<juce::Component*>(&m_pasteBtn) }) {
        btn->setBounds(x - 90, y2, 90, bandH);
        x -= 90 + gap;
    }

    const int statusH = 20;
    const int statusTop = getHeight() - statusH - 4;
    m_statusLabel.setBounds(margin, statusTop, getWidth() - 2 * margin, statusH);

    const int listTop = y2 + bandH + gap;
    const int listBottom = statusTop - 2;
    m_listRect = juce::Rectangle<int>(0, listTop, getWidth(),
                                      juce::jmax(0, listBottom - listTop));
}

std::pair<int, int> RelogioEditor::rowAt(const juce::Point<int>& pos) const
{
    if (!m_listRect.contains(pos) || m_rows.empty()) {
        return { -1, -1 };
    }
    const int i = (pos.getY() - m_listRect.getY() + m_scrollY) / kRowHeight;
    if (i < 0 || i >= static_cast<int>(m_rows.size())) {
        return { -1, -1 };
    }
    return { i, m_listRect.getX() + kMargin + kTimeWidth + kChipGap };
}

int RelogioEditor::chipAtX(const Row& row, int x) const
{
    const juce::Font chipFont = monoFont(13.0f);
    int cx = kMargin + kTimeWidth + kChipGap;
    for (int c = 0; c < static_cast<int>(row.kinds.size()); ++c) {
        const std::wstring text = row.chipTexts[static_cast<size_t>(c)];
        const int chipW =
            static_cast<int>(
            juce::GlyphArrangement::getStringWidth(chipFont, jstr(text))) +
        2 * kChipPadding;
        if (x >= cx && x <= cx + chipW) {
            return c;
        }
        cx += chipW + kChipGap;
    }
    return -1;
}

void RelogioEditor::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    const auto [row, chipsX] = rowAt(event.getPosition());
    (void)chipsX;

    if (event.mods.isRightButtonDown()) {
        // Botão direito: cancela o parâmetro armado; senão, remove o chip em
        // que clicou (ou seleciona a linha).
        if (m_armed) {
            cancelArmed();
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
        m_doc.removeParam(r.lineIndex, chip);
        rebuildRows();
        selectRow(row);
        notifyChanged();
        setStatus(L"Parâmetro removido de " + jstr(r.time) + L".");
        return;
    }

    if (row < 0) {
        return;
    }

    if (m_armed) {
        const relogio::ParamKind kind = m_armedKind;
        const std::wstring value = m_armedValue;
        m_doc.addParam(m_rows[static_cast<size_t>(row)].lineIndex, kind, value);
        rebuildRows();
        notifyChanged();
        setStatus(L"Parâmetro " + jstr(paramFull(kind, value)) + L" adicionado a " +
                  jstr(m_rows[static_cast<size_t>(row)].time) + L".");
        // Permanece armado para adicionar a outros horários (grudado).
        return;
    }

    selectRow(row);
}

void RelogioEditor::mouseMove(const juce::MouseEvent& event)
{
    const bool overList = m_listRect.contains(event.getPosition());
    const int hover = (overList)
                          ? rowAt(event.getPosition()).first
                          : -1;
    m_mousePos = event.getPosition();
    m_mouseOverList = overList;
    if (hover != m_hoverRow) {
        m_hoverRow = hover;
        repaint();
    } else if (m_armed) {
        repaint(); // o chip fantasma segue o cursor
    }
}

void RelogioEditor::mouseExit(const juce::MouseEvent& event)
{
    juce::Component::mouseExit(event);
    m_mouseOverList = false;
    m_hoverRow = -1;
    repaint();
}

void RelogioEditor::mouseWheelMove(const juce::MouseEvent& event,
                                   const juce::MouseWheelDetails& wheel)
{
    const int contentH = static_cast<int>(m_rows.size()) * kRowHeight;
    const int listHeight = m_listRect.getHeight();
    if (contentH <= listHeight) {
        juce::Component::mouseWheelMove(event, wheel);
        return;
    }
    const int step = (wheel.isReversed ? 1 : -1) *
                     static_cast<int>(std::abs(wheel.deltaY) * 40.0 + 0.5);
    m_scrollY = juce::jlimit(0, contentH - listHeight, m_scrollY + step);
    repaint();
}

bool RelogioEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) {
        if (m_armed) {
            cancelArmed();
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

void RelogioEditor::selectRow(int index)
{
    m_selectedRow = (index >= 0 && index < static_cast<int>(m_rows.size()))
                        ? index
                        : -1;
    refreshButtons();
    repaint();
}

void RelogioEditor::refreshButtons()
{
    m_copyBtn.setEnabled(m_selectedRow >= 0);
    m_pasteBtn.setEnabled(!m_clipboard.empty() && m_selectedRow >= 0);
}

void RelogioEditor::armParam(relogio::ParamKind kind)
{
    if (m_armed && m_armedKind == kind) {
        cancelArmed(); // clicar no mesmo chip cancela (grudado solto)
        return;
    }

    std::wstring value;
    if (paramHasValue(kind)) {
        value = askParamValue(kind, *this);
        if (value.empty()) {
            setStatus(L"Cancelado.");
            cancelArmed();
            return;
        }
    }

    m_armed = true;
    m_armedKind = kind;
    m_armedValue = value;
    updateArmedUi();
    setStatus(juce::String());
    repaint();
}

void RelogioEditor::cancelArmed()
{
    m_armed = false;
    m_armedKind = relogio::ParamKind::Fixo;
    m_armedValue.clear();
    m_hoverRow = -1;
    updateArmedUi();
    repaint();
}

void RelogioEditor::updateArmedUi()
{
    m_fixoBtn.setToggleState(m_armed && m_armedKind == relogio::ParamKind::Fixo,
                             juce::dontSendNotification);
    m_descarteBtn.setToggleState(
        m_armed && m_armedKind == relogio::ParamKind::Descarte,
        juce::dontSendNotification);
    m_localBtn.setToggleState(m_armed && m_armedKind == relogio::ParamKind::Local,
                              juce::dontSendNotification);
    m_satBtn.setToggleState(m_armed && m_armedKind == relogio::ParamKind::Sat,
                            juce::dontSendNotification);
    m_lockedBtn.setToggleState(
        m_armed && m_armedKind == relogio::ParamKind::Locked,
        juce::dontSendNotification);
    m_idBtn.setToggleState(m_armed && m_armedKind == relogio::ParamKind::Id,
                           juce::dontSendNotification);
    m_durBtn.setToggleState(m_armed && m_armedKind == relogio::ParamKind::Dur,
                            juce::dontSendNotification);

    if (m_armed) {
        m_hintLabel.setText(
            L"Parâmetro " + jstr(paramFull(m_armedKind, m_armedValue)) +
                L" armado — clique num horário para adicionar (botão direito "
                L"ou ESC cancela)",
            juce::dontSendNotification);
    } else {
        m_hintLabel.setText(
            L"Clique num parâmetro e depois num horário para adicioná-lo.",
            juce::dontSendNotification);
    }
}

void RelogioEditor::addInicio()
{
    if (m_doc.addTime(L"00:00") >= 0) {
        rebuildRows();
        notifyChanged();
        setStatus(L"Horário 00:00 adicionado.");
    } else {
        setStatus(L"00:00 já existe.");
    }
}

void RelogioEditor::addIntervalo()
{
    if (m_selectedRow < 0 ||
        m_selectedRow >= static_cast<int>(m_rows.size())) {
        setStatus(L"Selecione um horário primeiro (clique nele).");
        return;
    }
    const int a = relogio::RelogioDocument::parseTime(
        m_rows[static_cast<size_t>(m_selectedRow)].time);
    if (m_selectedRow + 1 >= static_cast<int>(m_rows.size())) {
        setStatus(L"Não há horário seguinte para completar o intervalo.");
        return;
    }
    const int b = relogio::RelogioDocument::parseTime(
        m_rows[static_cast<size_t>(m_selectedRow + 1)].time);
    if (a < 0 || b < 0 || b <= a + 1) {
        setStatus(L"Sem intervalo para preencher entre horários consecutivos.");
        return;
    }
    const int mid = (a + b) / 2;
    if (m_doc.addTime(relogio::RelogioDocument::formatTime(mid)) >= 0) {
        rebuildRows();
        notifyChanged();
        setStatus(L"Horário intermediário " +
                  jstr(relogio::RelogioDocument::formatTime(mid)) +
                  L" adicionado.");
    } else {
        setStatus(L"Não foi possível adicionar o horário intermediário.");
    }
}

void RelogioEditor::addAvulso()
{
    std::wstring value = askDialog(L"Horário avulso",
                                   L"Digite o horário no formato HH:MM (ex.: 12:30).",
                                   L"HH:MM", *this);
    while (!value.empty() && (value.front() == L' ' || value.front() == L'\t')) {
        value.erase(value.begin());
    }
    while (!value.empty() &&
           (value.back() == L' ' || value.back() == L'\t')) {
        value.pop_back();
    }
    if (value.empty()) {
        return;
    }
    if (relogio::RelogioDocument::parseTime(value) < 0) {
        setStatus(L"Horário inválido — use o formato HH:MM.");
        return;
    }
    if (m_doc.addTime(value) >= 0) {
        rebuildRows();
        notifyChanged();
        setStatus(L"Horário " + jstr(value) + L" adicionado.");
    } else {
        setStatus(L"Horário " + jstr(value) + L" já existe.");
    }
}

void RelogioEditor::copySelected()
{
    if (m_selectedRow < 0 ||
        m_selectedRow >= static_cast<int>(m_rows.size())) {
        setStatus(L"Selecione um horário primeiro (clique nele).");
        return;
    }
    const size_t idx = static_cast<size_t>(m_selectedRow);
    const int lineIndex = m_rows[idx].lineIndex;
    m_clipboard = m_doc.lines()[static_cast<size_t>(lineIndex)].horario.params;
    refreshButtons();
    setStatus(L"Parâmetros de " + jstr(m_rows[idx].time) +
              L" copiados (cole no mesmo ou em outro relógio).");
}

void RelogioEditor::pasteSelected()
{
    if (m_selectedRow < 0 ||
        m_selectedRow >= static_cast<int>(m_rows.size())) {
        setStatus(L"Selecione um horário primeiro (clique nele).");
        return;
    }
    if (m_clipboard.empty()) {
        setStatus(L"Nada para colar — copie os parâmetros de um horário.");
        return;
    }
    const size_t idx = static_cast<size_t>(m_selectedRow);
    m_doc.replaceParams(m_rows[idx].lineIndex, m_clipboard);
    rebuildRows();
    refreshButtons();
    notifyChanged();
    setStatus(L"Parâmetros colados em " + jstr(m_rows[idx].time) + L".");
}

} // namespace app