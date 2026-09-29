#include "app/RelogioEditor.h"

#include <algorithm>
#include <cmath>
#include <cwctype>

#include "app/JuceHelpers.h"

namespace {
constexpr int kRowHeight = 28;
constexpr int kMargin = 8;
constexpr int kTimeWidth = 62;
constexpr int kChipGap = 4;
constexpr int kChipPadding = 7;
constexpr int kSelW = 20; // coluna de seleção (checkbox) quando modo ativo
constexpr int kSliderW = 10;
constexpr int kSliderGap = 3;

juce::Font monoFont(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(
        juce::Font::getDefaultMonospacedFontName(),
        bold ? juce::String("Bold") : juce::String("Regular"), size));
}

// Rótulo amigável (PT-BR) exibido na paleta e nos chips, SEM parênteses.
const wchar_t* paramFriendlyName(relogio::ParamKind kind)
{
    switch (kind) {
    case relogio::ParamKind::Fixo:     return L"FIXO";
    case relogio::ParamKind::Descarte: return L"DESCARTE";
    case relogio::ParamKind::Local:    return L"LOCAL";
    case relogio::ParamKind::Sat:      return L"DISPARO";
    case relogio::ParamKind::Locked:   return L"BLOQUEADO";
    case relogio::ParamKind::Id:       return L"NOMEAR BLOCO";
    case relogio::ParamKind::Dur:      return L"DURAÇÃO";
    }
    return L"";
}

bool paramHasValue(relogio::ParamKind kind)
{
    return kind == relogio::ParamKind::Id || kind == relogio::ParamKind::Dur;
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

// Remove espaços/tabulações nas pontas.
void trimWs(std::wstring& s)
{
    while (!s.empty() && (s.front() == L' ' || s.front() == L'\t')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == L' ' || s.back() == L'\t')) {
        s.pop_back();
    }
}

// Filtro do campo de horário: aceita apenas dígitos e ':'.
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

// Resultado do diálogo "Preencher".
struct FillSpec {
    bool ok = false;
    std::wstring inicio; // "HH:MM"
    int intervalo = 0;   // minutos
    std::wstring error;  // mensagem quando !ok
};

// Diálogo "Preencher": Início (HH:MM) + Intervalo (minutos).
FillSpec askFill(juce::Component& parent)
{
    juce::AlertWindow window(
        L"Preencher horários",
        L"Preenche o relógio a partir do Início, incrementando de Intervalo "
        L"em Intervalo até o fim do dia.",
        juce::MessageBoxIconType::NoIcon, &parent);
    window.addTextEditor(L"inicio", juce::String(L"00:00"), L"Início (HH:MM)");
    window.addTextEditor(L"intervalo", juce::String(L"30"),
                         L"Intervalo (minutos)");
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
    std::wstring intervalo =
        app::wstr(window.getTextEditorContents(L"intervalo"));
    trimWs(spec.inicio);
    trimWs(intervalo);

    if (spec.inicio.empty() ||
        relogio::RelogioDocument::parseTime(spec.inicio) < 0) {
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

const wchar_t* paramFriendlyName(relogio::ParamKind kind)
{
    return ::paramFriendlyName(kind);
}

std::wstring paramChipText(const relogio::Param& p)
{
    std::wstring s = ::paramFriendlyName(p.kind);
    if (paramHasValue(p.kind) && !p.value.empty()) {
        s += L": " + p.value;
    }
    return s;
}

std::wstring paramArmedText(relogio::ParamKind kind, const std::wstring& value)
{
    std::wstring s = ::paramFriendlyName(kind);
    if (paramHasValue(kind) && !value.empty()) {
        s += L": " + value;
    }
    return s;
}

RelogioEditor::RelogioEditor(relogio::RelogioDocument& doc,
                             std::vector<ClipEntry>& clipboard)
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
            L"Selecione o parâmetro " +
            app::jstr(paramArmedText(k, paramHasValue(k) ? L"..." : L"")) +
            L" para adicioná-lo a um horário."));
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
    m_timeField.setInputFilter(new TimeFieldFilter(), true);
    m_timeField.setFont(monoFont(13.0f));
    m_timeField.setTextToShowWhenEmpty(L"Adicionar Horário", juce::Colours::grey);
    m_timeField.setTooltip(L"Horário a adicionar no formato HH:MM.");
    m_timeField.onReturnKey = [this] { addFromField(); };
    addAndMakeVisible(m_timeField);

    m_addTimeBtn.onClick = [this] { addFromField(); };
    m_addTimeBtn.setTooltip(L"OK — confirma a adição do horário digitado (HH:MM).");
    addAndMakeVisible(m_addTimeBtn);

    m_fillBtn.onClick = [this] { fillClock(); };
    m_fillBtn.setTooltip(
        L"Preencher Horários: define o Início e o Intervalo — ex.: 00:00 a "
        L"cada 30 minutos gera 00:00, 00:30, 01:00, ... até o fim do dia.");
    addAndMakeVisible(m_fillBtn);

    m_removeBtn.onClick = [this] { removeSelected(); };
    m_removeBtn.setTooltip(L"Remove, dos horários selecionados, os horários "
                           L"inteiros ou apenas os parâmetros.");
    addAndMakeVisible(m_removeBtn);

    // Botão quadrado de seleção: ativa o modo; com ele ativo, alterna marcar
    // todos (como o antigo "Todos").
    m_selectBtn.onClick = [this] { onSelectCheckClick(); };
    m_selectBtn.setTooltip(
        L"Ativa a seleção de vários horários (clique numa linha para marcar/"
        L"desmarcar). Ativado, esse mesmo quadrado marca TODOS; se todos já "
        L"estiverem marcados, limpa a seleção.");
    addAndMakeVisible(m_selectBtn);

    m_paramCombo.addItem(L"FIXO", 1);
    m_paramCombo.addItem(L"DESCARTE", 2);
    m_paramCombo.addItem(L"LOCAL", 3);
    m_paramCombo.addItem(L"DISPARO", 4);
    m_paramCombo.addItem(L"BLOQUEADO", 5);
    m_paramCombo.addItem(L"NOMEAR BLOCO", 6);
    m_paramCombo.addItem(L"DURAÇÃO", 7);
    // Nenhum item pré-selecionado: o rótulo "Parâmetros" é exibido na caixa,
    // mas NÃO aparece na lista — só os parâmetros aparecem.
    m_paramCombo.setText(juce::String(L"Parâmetros"),
                         juce::dontSendNotification);
    m_paramCombo.setTooltip(
        L"Parâmetros: escolha o parâmetro a enviar para todos os horários "
        L"selecionados (ID/DUR pedem o valor).");
    addAndMakeVisible(m_paramCombo);

    m_applyBtn.onClick = [this] { applyParamToSelected(); };
    m_applyBtn.setTooltip(
        L"Aplica o parâmetro (do combo) a TODOS os horários selecionados.");
    addAndMakeVisible(m_applyBtn);

    m_copyBtn.onClick = [this] { copySelected(); };
    m_pasteBtn.onClick = [this] { pasteSelected(); };
    m_copyBtn.setTooltip(
        L"Copiar os horários + parâmetros dos selecionados (Ctrl+C) — "
        L"compartilhado entre os relógios.");
    m_pasteBtn.setTooltip(
        L"Colar (Ctrl+V) os horários copiados neste relógio: cria os que "
        L"faltam e acrescenta os parâmetros aos existentes.");
    addAndMakeVisible(m_copyBtn);
    addAndMakeVisible(m_pasteBtn);

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
    // Preserva o estado de seleção por posição (rowIndex).
    const std::vector<bool> previousSelection = m_rowSelected;

    m_rows.clear();
    m_rows.reserve(m_doc.lines().size());
    const juce::Font chipFont = monoFont(13.0f);
    for (size_t i = 0; i < m_doc.lines().size(); ++i) {
        const relogio::RelogioDocument::Line& line = m_doc.lines()[i];
        if (line.kind != relogio::RelogioDocument::Line::Kind::Horario) {
            continue;
        }
        Row r;
        r.lineIndex = static_cast<int>(i);
        r.time = line.horario.time;
        r.timeStr = jstr(r.time);
        r.trailing = line.horario.trailing;
        r.trailingStr = jstr(r.trailing);
        for (const relogio::Param& p : line.horario.params) {
            r.kinds.push_back(p.kind);
            const juce::String cs = jstr(paramChipText(p));
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
    repaint(); // garante que as linhas (re)apareçam mesmo sem passar o mouse
}

void RelogioEditor::refreshRow(int rowIndex)
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
    const relogio::RelogioDocument::Line& line =
        lines[static_cast<size_t>(lineIndex)];
    if (line.kind != relogio::RelogioDocument::Line::Kind::Horario) {
        return;
    }

    r.time = line.horario.time;
    r.timeStr = jstr(r.time);
    r.trailing = line.horario.trailing;
    r.trailingStr = jstr(r.trailing);
    r.kinds.clear();
    r.chipStr.clear();
    r.chipW.clear();
    const juce::Font chipFont = monoFont(13.0f);
    for (const relogio::Param& p : line.horario.params) {
        r.kinds.push_back(p.kind);
        const juce::String cs = jstr(paramChipText(p));
        r.chipStr.push_back(cs);
        r.chipW.push_back(static_cast<int>(
            juce::GlyphArrangement::getStringWidth(chipFont, cs)));
    }
    repaint(rowRect(rowIndex));
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

    // Divisórias verticais entre os grupos da fileira de botões.
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
        g.drawText(L"Nenhum horário ainda — use o campo HH:MM + Adicionar, ou "
                   L"Preencher.",
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

        // Fundo da linha: marcada, sob o mouse (com parâmetro armado) ou
        // normal.
        juce::Colour bg = juce::Colour(0xff2b2b2b);
        if (m_armed && i == m_hoverRow) {
            bg = juce::Colour(0xff4a4a4a);
        } else if (rowMarked) {
            bg = juce::Colour(0xff3d3d3d);
        }
        g.setColour(bg);
        g.fillRect(rowArea);

        // Coluna de seleção (checkbox) quando o modo está ativo.
        const int timeX = kMargin + (m_selectMode ? kSelW : 0);
        if (m_selectMode) {
            const juce::Rectangle<int> box(
                2, y + (kRowHeight - 14) / 2, 14, 14);
            g.setColour(rowMarked ? juce::Colour(0xff26a69a)
                                  : juce::Colour(0xff4a4a4a));
            g.fillRoundedRectangle(box.toFloat(), 3.0f);
            if (rowMarked) {
                g.setColour(juce::Colours::white);
                g.setFont(monoFont(12.0f, true));
                g.drawText(L"\u2713", box, juce::Justification::centred, true);
            }
        }

        // Horário.
        g.setColour(juce::Colours::white);
        g.setFont(monoFont(14.0f, true));
        g.drawText(row.timeStr,
                   juce::Rectangle<int>(timeX, y, kTimeWidth, kRowHeight),
                   juce::Justification::centredLeft, true);

        // Chips de parâmetros (larguras cacheadas).
        int cx = chipsBaseX();
        const juce::Font chipFont = monoFont(13.0f);
        for (int c = 0; c < static_cast<int>(row.kinds.size()); ++c) {
            const int tw = row.chipW[static_cast<size_t>(c)];
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
            g.drawText(row.chipStr[static_cast<size_t>(c)], chipRect,
                       juce::Justification::centred, true);
            cx = chipRect.getRight() + kChipGap;
        }

        // Conteúdo preservado (resto da linha): cinza, cortado no fim da linha.
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

        // Separador sutil entre linhas.
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(0, y + kRowHeight - 1, width, 1);
    }

    // Barra de rolagem (quando o conteúdo excede a área visível).
    const int contentH = contentHeight();
    if (contentH > listHeight) {
        const juce::Rectangle<int> thumb = sliderThumb();
        const int sx = thumb.getX();
        // Trilha.
        g.setColour(juce::Colour(0xff2f2f2f));
        g.fillRect(sx, listTop, kSliderW, listHeight);
        // Thumb.
        g.setColour(m_dragSlider ? juce::Colour(0xff6a6a6a) : juce::Colour(0xff505050));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(static_cast<float>(sx + kSliderGap),
                                   static_cast<float>(thumb.getY()),
                                   static_cast<float>(kSliderW - 2 * kSliderGap),
                                   static_cast<float>(thumb.getHeight())),
            3.0f);
    }

    // Chip fantasma do parâmetro armado (segue o mouse).
    if (m_armed) {
        const juce::Rectangle<int> ghost = ghostRect(m_mousePos);
        if (!ghost.isEmpty()) {
            const juce::Font f = monoFont(13.0f, true);
            g.setColour(paramColour(m_armedKind).withAlpha(0.85f));
            g.fillRoundedRectangle(ghost.toFloat(), 6.0f);
            g.setColour(juce::Colours::black);
            g.setFont(f);
            g.drawText(jstr(paramArmedText(m_armedKind, m_armedValue)), ghost,
                       juce::Justification::centred, true);
        }
    }
    (void)width;
}

void RelogioEditor::resized()
{
    const int margin = 8;
    const int bandH = 26;
    const int gap = 6;

    int x = margin;
    const auto placeW = [&x, &gap, &bandH](juce::Component& c, int y, int w) {
        c.setBounds(x, y, w, bandH);
        x = c.getRight() + gap;
    };

    // Fileira 1: paleta de parâmetros ("grudado no mouse") com largura
    // adequada a cada rótulo.
    const int y1 = 6;
    x = margin;
    placeW(m_fixoBtn, y1, m_fixoBtn.getBestWidthForHeight(bandH));
    placeW(m_descarteBtn, y1, m_descarteBtn.getBestWidthForHeight(bandH));
    placeW(m_localBtn, y1, m_localBtn.getBestWidthForHeight(bandH));
    placeW(m_satBtn, y1, m_satBtn.getBestWidthForHeight(bandH));
    placeW(m_lockedBtn, y1, m_lockedBtn.getBestWidthForHeight(bandH));
    placeW(m_idBtn, y1, m_idBtn.getBestWidthForHeight(bandH));
    placeW(m_durBtn, y1, m_durBtn.getBestWidthForHeight(bandH));
    m_hintLabel.setBounds(x, y1, juce::jmax(0, getWidth() - x - margin), bandH);

    // Fileira 2 (única): ordem — Selecionar + Parâmetros + Aplicar + Remover |
    // Preencher Horários + Adicionar + OK | Colar + Copiar.
    const int y2 = y1 + bandH + gap;
    x = margin;

    // Grupo 1: seleção, adicionar parâmetros e remover.
    placeW(m_selectBtn, y2, 90);
    m_paramCombo.setBounds(x, y2, 130, bandH);
    x = m_paramCombo.getRight() + gap;
    placeW(m_applyBtn, y2, 74);
    placeW(m_removeBtn, y2, 84);
    m_sep1X = x; // divisória
    x += 8;

    // Grupo 2: empregos de horário.
    placeW(m_fillBtn, y2, 132);
    m_timeField.setBounds(x, y2, 90, bandH);
    x = m_timeField.getRight() + gap;
    placeW(m_addTimeBtn, y2, 82);
    m_sep2X = x; // divisória
    x += 8;

    // Grupo 3: copiar/colar à direita (ordem: Colar, Copiar).
    x = getWidth() - margin;
    m_copyBtn.setBounds(x - 80, y2, 80, bandH);
    x -= 80 + gap;
    m_pasteBtn.setBounds(x - 80, y2, 80, bandH);

    m_toolArea = juce::Rectangle<int>(0, y2, getWidth(), bandH);

    const int statusH = 20;
    const int statusTop = getHeight() - statusH - 4;
    m_statusLabel.setBounds(margin, statusTop, getWidth() - 2 * margin, statusH);

    const int listTop = y2 + bandH + gap;
    const int listBottom = statusTop - 2;
    m_listRect = juce::Rectangle<int>(0, listTop, getWidth(),
                                      juce::jmax(0, listBottom - listTop));
    clampScroll();
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
    return { i, chipsBaseX() };
}

int RelogioEditor::chipAtX(const Row& row, int x) const
{
    int cx = chipsBaseX();
    for (int c = 0; c < static_cast<int>(row.kinds.size()); ++c) {
        const int chipW =
            row.chipW[static_cast<size_t>(c)] + 2 * kChipPadding;
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

    // Clique na barra de rolagem (somente quando há conteúdo extra).
    if (contentHeight() > m_listRect.getHeight() && overSliderStrip(event.getPosition())) {
        if (event.mods.isRightButtonDown()) {
            return; // nada a fazer com o botão direito por cima da barra
        }
        const juce::Rectangle<int> thumb = sliderThumb();
        if (thumb.contains(event.getPosition())) {
            m_dragSlider = true;
            m_dragStartThumbY = event.getPosition().getY();
            m_dragStartScrollY = m_scrollY;
            repaint();
        } else {
            // Clique na trilha: salta uma página.
            const int listHeight = m_listRect.getHeight();
            const int maxScroll = juce::jmax(0, contentHeight() - listHeight);
            m_scrollY = juce::jlimit(
                0, maxScroll,
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
        refreshRow(row); // removeParam não reordena: atualiza só a linha
        notifyChanged();
        setStatus(L"Parâmetro removido de " + r.timeStr + L".");
        return;
    }

    if (row < 0) {
        return;
    }

    if (m_armed) {
        const relogio::ParamKind kind = m_armedKind;
        const std::wstring value = m_armedValue;
        const bool added = m_doc.addParam(
            m_rows[static_cast<size_t>(row)].lineIndex, kind, value);
        refreshRow(row); // addParam não reordena: só a linha muda
        notifyChanged();
        if (added) {
            setStatus(L"Parâmetro " + jstr(paramArmedText(kind, value)) +
                      L" adicionado a " +
                      m_rows[static_cast<size_t>(row)].timeStr + L".");
        } else {
            setStatus(L"Parâmetro " + jstr(paramArmedText(kind, value)) +
                      L" já está em " +
                      m_rows[static_cast<size_t>(row)].timeStr + L".");
        }
        // Permanece armado para adicionar a outros horários (grudado).
        return;
    }

    if (m_selectMode) {
        toggleSelectedRow(row); // clique alterna a marcação
    } else {
        selectRow(row);
    }
}

void RelogioEditor::mouseDrag(const juce::MouseEvent& event)
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
    const int newScroll =
        m_dragStartScrollY + dy * (contentH - listHeight) / travel;
    m_scrollY = juce::jlimit(0, juce::jmax(0, contentH - listHeight), newScroll);
    repaint();
}

void RelogioEditor::mouseUp(const juce::MouseEvent& event)
{
    if (m_dragSlider) {
        m_dragSlider = false;
        repaint();
    }
    juce::Component::mouseUp(event);
}

void RelogioEditor::mouseMove(const juce::MouseEvent& event)
{
    const bool overList = m_listRect.contains(event.getPosition());
    const int hover = (overList) ? rowAt(event.getPosition()).first : -1;
    const juce::Rectangle<int> oldGhost = ghostRect(m_mousePos);
    m_mousePos = event.getPosition();
    m_mouseOverList = overList;

    if (hover != m_hoverRow) {
        // Atualiza apenas a linha antiga e a nova (fundo destacado).
        if (m_hoverRow >= 0) {
            repaint(rowRect(m_hoverRow));
        }
        m_hoverRow = hover;
        if (m_hoverRow >= 0) {
            repaint(rowRect(m_hoverRow));
        }
    }
    if (m_armed) {
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
    const int listHeight = m_listRect.getHeight();
    if (contentHeight() <= listHeight) {
        juce::Component::mouseWheelMove(event, wheel);
        return;
    }

    // deltaY < 0 = rolar para BAIXO (mostrar horários seguintes); isReversed
    // inverte o sentido (scroll "natural" de touchpad).
    float dy = wheel.deltaY;
    if (wheel.isReversed) {
        dy = -dy;
    }
    // Roda ~3 linhas por "clique" para a navegação não ficar lenta.
    const float rowsPerNotch = 3.0f * std::max(1.0f, std::abs(dy));
    int step = static_cast<int>(rowsPerNotch * kRowHeight);
    if (dy > 0.0f) {
        step = -step; // rolar para CIMA
    }
    m_scrollY = juce::jlimit(0, juce::jmax(0, contentHeight() - listHeight),
                             m_scrollY + step);
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
    repaint(rowRect(index));
}

// ------------------------------------------------------------------
// Seleção múltipla
// ------------------------------------------------------------------
bool RelogioEditor::hasSelection() const
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

std::vector<int> RelogioEditor::selectedRowIndices() const
{
    std::vector<int> out;
    if (m_selectMode) {
        bool anyMarked = false;
        for (size_t k = 0; k < m_rowSelected.size(); ++k) {
            if (m_rowSelected[k]) {
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

void RelogioEditor::toggleSelectedRow(int rowIndex)
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

void RelogioEditor::selectAllRows()
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

void RelogioEditor::selectNoRows()
{
    std::fill(m_rowSelected.begin(), m_rowSelected.end(), false);
    refreshButtons();
    repaint();
    setStatus(L"Seleção limpa.");
}

void RelogioEditor::onSelectCheckClick()
{
    if (!m_selectMode) {
        // Primeiro clique: habilita a seleção (nenhum marcado ainda).
        m_selectMode = true;
        updateSelectCheck();
        repaint();
        return;
    }
    // Modo ativo: comporta-se como "Todos" — se tudo marcado, limpa; senão,
    // marca todos.
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

void RelogioEditor::updateSelectCheck()
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

void RelogioEditor::SelectCheckButton::setState(bool modeOn, bool allSelected)
{
    if (m_modeOn == modeOn && m_allSelected == allSelected) {
        return;
    }
    m_modeOn = modeOn;
    m_allSelected = allSelected;
    repaint();
}

void RelogioEditor::SelectCheckButton::paintButton(juce::Graphics& g, bool,
                                                   bool)
{
    const juce::Rectangle<float> area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillRoundedRectangle(area.reduced(1.0f), 4.0f);

    // Quadrado (checkbox) à esquerda.
    const float size = juce::jmin(area.getHeight() - 8.0f, 18.0f);
    const juce::Rectangle<float> box(area.getX() + 5.0f, area.getCentreY() - size / 2.0f,
                                     size, size);
    if (m_modeOn && m_allSelected) {
        g.setColour(juce::Colour(0xff26a69a));
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(size * 0.8f, juce::Font::bold)));
        g.drawText(L"\u2713", box.toNearestInt(), juce::Justification::centred,
                   true);
    } else {
        g.setColour(m_modeOn ? juce::Colours::lightgrey
                             : juce::Colour(0xff777777));
        g.drawRoundedRectangle(box, 3.0f, 1.5f);
    }

    // Rótulo ao lado do quadrado.
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
// Geometria de linha
// ------------------------------------------------------------------
int RelogioEditor::chipsBaseX() const
{
    return kMargin + (m_selectMode ? kSelW : 0) + kTimeWidth + kChipGap;
}

juce::Rectangle<int> RelogioEditor::rowRect(int rowIndex) const
{
    return juce::Rectangle<int>(
        0, m_listRect.getY() + rowIndex * kRowHeight - m_scrollY,
        m_listRect.getWidth(), kRowHeight);
}

juce::Rectangle<int> RelogioEditor::ghostRect(const juce::Point<int>& pos) const
{
    const int width = getWidth();
    if (!m_armed || m_mouseOverList == false || !m_listRect.contains(pos)) {
        return {};
    }
    const juce::Font f = monoFont(13.0f, true);
    const juce::String label = jstr(paramArmedText(m_armedKind, m_armedValue));
    const int tw = static_cast<int>(
        juce::GlyphArrangement::getStringWidth(f, label));
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

int RelogioEditor::findTimeLine(const std::wstring& hhmm) const
{
    for (const Row& row : m_rows) {
        if (row.time == hhmm) {
            return row.lineIndex;
        }
    }
    return -1;
}

// ------------------------------------------------------------------
// Rolagem
// ------------------------------------------------------------------
int RelogioEditor::contentHeight() const
{
    return static_cast<int>(m_rows.size()) * kRowHeight;
}

int RelogioEditor::sliderThumbHeight() const
{
    const int listHeight = m_listRect.getHeight();
    const int contentH = contentHeight();
    if (contentH <= listHeight) {
        return 0;
    }
    return juce::jmax(22, static_cast<int>(listHeight * listHeight / contentH));
}

juce::Rectangle<int> RelogioEditor::sliderThumb() const
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
    return juce::Rectangle<int>(m_listRect.getRight() - kSliderW, thumbY,
                                kSliderW, thumbH);
}

bool RelogioEditor::overSliderStrip(const juce::Point<int>& pos) const
{
    return m_listRect.contains(pos) &&
           pos.getX() >= m_listRect.getRight() - kSliderW;
}

void RelogioEditor::clampScroll()
{
    const int maxScroll = contentHeight() - m_listRect.getHeight();
    m_scrollY = juce::jlimit(0, juce::jmax(0, maxScroll), m_scrollY);
}

void RelogioEditor::refreshButtons()
{
    const bool any = hasSelection();
    m_copyBtn.setEnabled(any);
    m_pasteBtn.setEnabled(!m_clipboard.empty());
    m_removeBtn.setEnabled(any);
    m_applyBtn.setEnabled(any);
    updateSelectCheck();
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
            L"Parâmetro " + jstr(paramArmedText(m_armedKind, m_armedValue)) +
                L" armado — clique num horário para adicionar (botão direito "
                L"ou ESC cancela)",
            juce::dontSendNotification);
    } else {
        m_hintLabel.setText(
            L"Clique num parâmetro e depois num horário para adicioná-lo.",
            juce::dontSendNotification);
    }
}

void RelogioEditor::addFromField()
{
    std::wstring value = app::wstr(m_timeField.getText());
    trimWs(value);
    if (value.empty()) {
        setStatus(L"Digite um horário no formato HH:MM (ex.: 12:30).");
        return;
    }
    if (relogio::RelogioDocument::parseTime(value) < 0) {
        setStatus(L"Horário inválido — use o formato HH:MM.");
        return;
    }
    if (m_doc.addTime(value) >= 0) {
        rebuildRows();
        m_timeField.clear();
        notifyChanged();
        setStatus(L"Horário " + jstr(value) + L" adicionado.");
    } else {
        setStatus(L"Horário " + jstr(value) + L" já existe.");
    }
}

void RelogioEditor::fillClock()
{
    const FillSpec spec = askFill(*this);
    if (!spec.ok) {
        if (!spec.error.empty()) {
            setStatus(jstr(spec.error));
        }
        return;
    }
    const int start = relogio::RelogioDocument::parseTime(spec.inicio);
    if (start < 0) {
        return;
    }
    std::vector<std::wstring> times;
    for (int t = start; t < 1440; t += spec.intervalo) {
        times.push_back(relogio::RelogioDocument::formatTime(t));
        if (times.size() > 2048) { // salvaguarda de segurança
            break;
        }
    }
    m_doc.reschedule(times);
    rebuildRows();
    notifyChanged();
    setStatus(L"Preencher: " + juce::String(static_cast<int>(times.size())) +
              L" horários de " + jstr(spec.inicio) + L" em " +
              juce::String(spec.intervalo) + L" minutos.");
}

void RelogioEditor::removeSelected()
{
    const std::vector<int> targetRows = selectedRowIndices();
    if (targetRows.empty()) {
        setStatus(L"Selecione um horário para remover (clique nele).");
        return;
    }

    // Pergunta o que remover: horários inteiros ou apenas os parâmetros.
    juce::AlertWindow window(
        L"Remover",
        L"O que você deseja remover nos horários selecionados?",
        juce::MessageBoxIconType::NoIcon, this);
    window.addButton(L"Horários", 1,
                     juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    window.addButton(L"Parâmetros", 2);
    window.addButton(L"Cancelar", 0,
                     juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));
    window.enterModalState(true);
    const int choice = window.runModalLoop();
    if (choice == 0) {
        return; // cancelou
    }

    if (choice == 2) {
        // Apenas os parâmetros: mantém o horário (e o conteúdo).
        int removed = 0;
        for (const int row : targetRows) {
            const int lineIndex = m_rows[static_cast<size_t>(row)].lineIndex;
            const auto& lines = m_doc.lines();
            if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= lines.size()) {
                continue;
            }
            while (lines[static_cast<size_t>(lineIndex)].horario.params.size()
                   > 0) {
                m_doc.removeParam(lineIndex, 0);
                ++removed;
            }
            refreshRow(row);
        }
        notifyChanged();
        setStatus(juce::String(removed) + L" parâmetro(s) removido(s); os "
                  L"horários foram mantidos.");
        return;
    }

    // Horários: remove a linha inteira (horário + parâmetros + conteúdo).
    std::vector<int> lineIndexes;
    lineIndexes.reserve(targetRows.size());
    for (const int row : targetRows) {
        lineIndexes.push_back(m_rows[static_cast<size_t>(row)].lineIndex);
    }
    // Remove das linhas de maior para menor índice (mantém os menores válidos).
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

void RelogioEditor::applyParamToSelected()
{
    const std::vector<int> targetRows = selectedRowIndices();
    if (targetRows.empty()) {
        setStatus(L"Selecione um horário para aplicar o parâmetro.");
        return;
    }
    const int comboId = m_paramCombo.getSelectedId();
    if (comboId == 0) {
        setStatus(L"Escolha um parâmetro no seletor \"Parâmetros\".");
        return;
    }

    relogio::ParamKind kind = relogio::ParamKind::Fixo;
    switch (comboId) {
    case 2: kind = relogio::ParamKind::Descarte; break;
    case 3: kind = relogio::ParamKind::Local; break;
    case 4: kind = relogio::ParamKind::Sat; break;
    case 5: kind = relogio::ParamKind::Locked; break;
    case 6: kind = relogio::ParamKind::Id; break;
    case 7: kind = relogio::ParamKind::Dur; break;
    default: kind = relogio::ParamKind::Fixo; break;
    }

    std::wstring value;
    if (paramHasValue(kind)) {
        value = askParamValue(kind, *this);
        if (value.empty()) {
            setStatus(L"Cancelado.");
            return;
        }
    }

    int applied = 0;
    int skipped = 0;
    for (const int row : targetRows) {
        const int lineIndex = m_rows[static_cast<size_t>(row)].lineIndex;
        if (m_doc.addParam(lineIndex, kind, value)) {
            ++applied;
            refreshRow(row);
        } else {
            ++skipped;
        }
    }
    refreshButtons();
    notifyChanged();
    setStatus(L"Parâmetro " + jstr(paramArmedText(kind, value)) + L" aplicado a " +
              juce::String(applied) + L" horário(s)" +
              (skipped > 0 ? (L" (" + juce::String(skipped) + L" já tinham)")
                           : juce::String())
              + L".");
}

void RelogioEditor::copySelected()
{
    const std::vector<int> targetRows = selectedRowIndices();
    if (targetRows.empty()) {
        setStatus(L"Selecione um horário para copiar (clique nele).");
        return;
    }
    m_clipboard.clear();
    m_clipboard.reserve(targetRows.size());
    for (const int row : targetRows) {
        const Row& r = m_rows[static_cast<size_t>(row)];
        const int lineIndex = r.lineIndex;
        ClipEntry entry;
        entry.time = r.time;
        entry.params = m_doc.lines()[static_cast<size_t>(lineIndex)].horario.params;
        m_clipboard.push_back(std::move(entry));
    }
    refreshButtons();
    setStatus(juce::String(static_cast<int>(m_clipboard.size())) +
              L" horário(s) copiado(s) — cole no mesmo ou em outro relógio.");
}

void RelogioEditor::pasteSelected()
{
    if (m_clipboard.empty()) {
        setStatus(L"Nada para colar — copie horários de um relógio.");
        return;
    }
    int added = 0;
    int merged = 0;
    for (const ClipEntry& entry : m_clipboard) {
        int lineIndex = findTimeLine(entry.time);
        if (lineIndex < 0) {
            lineIndex = m_doc.addTime(entry.time);
            if (lineIndex >= 0) {
                ++added;
            } else {
                lineIndex = findTimeLine(entry.time); // já existia
            }
        }
        if (lineIndex < 0) {
            continue;
        }
        for (const relogio::Param& p : entry.params) {
            if (m_doc.addParam(lineIndex, p.kind, p.value)) {
                ++merged;
            }
        }
    }
    rebuildRows();
    refreshButtons();
    notifyChanged();
    setStatus(juce::String(added) + L" horário(s) criado(s), parâmetros "
              L"aplicados: " + juce::String(merged) + L".");
}

} // namespace app