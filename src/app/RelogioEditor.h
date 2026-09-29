#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

#include "relogio/RelogioDocument.h"

// ============================================================================
// Editor VISUAL de um arquivo de Relógio (Etapa 3).
//
// Opera sobre um relogio::RelogioDocument (o modelo em memória do arquivo).
// A interface NÃO interpreta conteúdo de programação: mostra cada horário com
// os parâmetros reconhecidos em "chips" coloridos e o restante da linha
// (conteúdo preservado) em cinza.
//
// "Parâmetro grudado no mouse": clicar num chip da paleta ARMA o parâmetro —
// um chip fantasma acompanha o cursor e o clique num horário o adiciona,
// mantendo-o armado para o próximo horário. Cancelar: clicar no mesmo chip
// (ou em outro), botão direito ou ESC.
//
// Horários: o campo "Adicionar Horário" + OK insere um horário; "Preencher
// Horários" abre um diálogo (Início + Intervalo) e gera a seqüência completa
// do dia; "Remover" pergunta o que remover dos selecionados — Horários
// (apaga as linhas inteiras) ou Parâmetros (apaga só os chips, mantendo o
// horário e o conteúdo).
//
// Seleção: o botão quadrado (ícone) habilita a seleção — desativado, clicar
// ativa o modo; ativado, o próprio botão vira o quadrado "selecionar todos":
// marca todos (e mostra ✓) ou, se todos já estiverem marcados, limpa. O combo
// + "Aplicar" envia um parâmetro (ID/DUR pedem valor) para TODOS os marcados.
//
// Copiar/Colar: "Copiar" (Ctrl+C) copia os horários + parâmetros dos
// selecionados para a área de transferência COMPARTILHADA entre os relógios
// (as abas); "Colar" (Ctrl+V) traz essas linhas para o documento: horários
// inexistentes são criados e os existentes ganham os parâmetros que faltam.
// ============================================================================

namespace app {

// Um horário "copiado": hora + parâmetros. Usado na área de transferência
// COMPARTILHADA entre os editores (copiar/colar horários inteiros entre
// relógios).
struct ClipEntry {
    std::wstring time;
    std::vector<relogio::Param> params;
};

class RelogioEditor final : public juce::Component {
public:
    // doc: modelo do arquivo em edição; clipboard: área de transferência
    // COMPARTILHADA com os demais editores do RelogioEditorTab.
    RelogioEditor(relogio::RelogioDocument& doc,
                  std::vector<ClipEntry>& clipboard);
    ~RelogioEditor() override;

    // Re-lê o documento e reconstrói as linhas visuais (chamado ao carregar,
    // ao trocar o modo e após alterações feitas por fora).
    void rebuild();

    // Disparado quando este editor ALTERA o documento (adiciona horário,
    // adiciona/remove/copia parâmetro). Usado pela página para marcar sujo.
    std::function<void()> onChange;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    // Uma linha visual = um horário do documento (linhas cruas não aparecem).
    // As strings/larguras de exibição são CACHEADAS aqui (timeStr, trailingStr,
    // chipStr, chipW) para que o paint e o hover não recomputem a cada evento.
    struct Row {
        int lineIndex = -1; // índice da LINE no RelogioDocument
        std::wstring time;
        juce::String timeStr;
        std::wstring trailing;
        juce::String trailingStr;
        std::vector<relogio::ParamKind> kinds;
        std::vector<juce::String> chipStr;
        std::vector<int> chipW;
    };

    void rebuildRows();
    void notifyChanged();
    void setStatus(const juce::String& text);
    void refreshButtons();

    std::pair<int, int> rowAt(const juce::Point<int>& pos) const;
    int chipAtX(const Row& row, int x) const;
    void selectRow(int index);
    // Atualiza APENAS a linha visual (e repara) sem reconstruir todas.
    void refreshRow(int rowIndex);
    juce::Rectangle<int> rowRect(int rowIndex) const;
    // X inicial dos chips (comporta a coluna de seleção quando ativa).
    int chipsBaseX() const;
    juce::Rectangle<int> ghostRect(const juce::Point<int>& pos) const;

    // Seleção múltipla.
    bool hasSelection() const;
    std::vector<int> selectedRowIndices() const;
    void toggleSelectedRow(int rowIndex);
    void selectAllRows();
    void selectNoRows();
    // Clique no botão de seleção (habilita o modo ou alterna marcar todos).
    void onSelectCheckClick();
    // Reflete o estado atual do modo/marcação no botão quadrado.
    void updateSelectCheck();

    // Rolagem.
    void clampScroll();
    int contentHeight() const;
    int sliderThumbHeight() const;
    juce::Rectangle<int> sliderThumb() const;
    bool overSliderStrip(const juce::Point<int>& pos) const;

    // Paleta "grudada".
    void armParam(relogio::ParamKind kind);
    void cancelArmed();
    void updateArmedUi();

    // Empregos de horário.
    void addFromField();
    void fillClock();
    void removeSelected();
    void applyParamToSelected();
    void copySelected();
    void pasteSelected();
    // Índice da LINE cujo horário == hhmm, ou -1.
    int findTimeLine(const std::wstring& hhmm) const;

    // Layout (preenchido em resized).
    juce::Rectangle<int> m_listRect;
    // Faixa da fileira única de botões (para as divisórias verticais).
    juce::Rectangle<int> m_toolArea;
    // X das divisórias verticais que separam os grupos (-1 = sem linha).
    int m_sep1X = -1;
    int m_sep2X = -1;

    relogio::RelogioDocument& m_doc;
    std::vector<ClipEntry>& m_clipboard;
    std::vector<Row> m_rows;
    int m_selectedRow = -1;
    int m_hoverRow = -1;
    int m_scrollY = 0;

    bool m_selectMode = false;
    std::vector<bool> m_rowSelected;

    bool m_armed = false;
    relogio::ParamKind m_armedKind = relogio::ParamKind::Fixo;
    std::wstring m_armedValue;
    juce::Point<int> m_mousePos;
    bool m_mouseOverList = false;

    // Arrasto da barra de rolagem.
    bool m_dragSlider = false;
    int m_dragStartThumbY = 0;
    int m_dragStartScrollY = 0;

    // Botão quadrado de seleção: ícone de "habilitar seleção" que, com o modo
    // ativo, vira o quadrado "selecionar todos" (alterna marcação de todos).
    class SelectCheckButton final : public juce::Button {
    public:
        SelectCheckButton() : juce::Button(L"Selecionar") {}
        // modeOn: modo de seleção ativo; allSelected: todos marcaram.
        void setState(bool modeOn, bool allSelected);
        bool isModeOn() const { return m_modeOn; }
    private:
        void paintButton(juce::Graphics& g, bool isOver, bool isDown) override;
        bool m_modeOn = false;
        bool m_allSelected = false;
    };

    juce::TextButton m_fixoBtn{ "FIXO" };
    juce::TextButton m_descarteBtn{ "DESCARTE" };
    juce::TextButton m_localBtn{ "LOCAL" };
    juce::TextButton m_satBtn{ "DISPARO" };
    juce::TextButton m_lockedBtn{ "BLOQUEADO" };
    juce::TextButton m_idBtn{ "NOMEAR BLOCO" };
    juce::TextButton m_durBtn{ L"DURAÇÃO" };
    juce::Label m_hintLabel;
    juce::TextEditor m_timeField;
    juce::TextButton m_addTimeBtn{ L"OK" };
    juce::TextButton m_fillBtn{ L"Preencher Horários" };
    juce::TextButton m_removeBtn{ L"Remover" };
    juce::TextButton m_copyBtn{ L"Copiar" };
    juce::TextButton m_pasteBtn{ L"Colar" };
    SelectCheckButton m_selectBtn;
    juce::ComboBox m_paramCombo;
    juce::TextButton m_applyBtn{ L"Aplicar" };
    juce::Label m_statusLabel;
};

// Cor dos chips por parâmetro (paleta e linhas usam a mesma cor).
juce::Colour paramColour(relogio::ParamKind kind);

// Texto pede ao usuário um valor de ID/DUR; devolve vazio se cancelado.
    std::wstring askParamValue(relogio::ParamKind kind, juce::Component& parent);

    // Rótulo amigável (PT-BR) de um parâmetro para a paleta e os chips.
    const wchar_t* paramFriendlyName(relogio::ParamKind kind);
    // Texto de chip de um parâmetro (sem parênteses): "DISPARO",
    // "NOMEAR BLOCO: Noticias", "DURAÇÃO: 3:00"...
    std::wstring paramChipText(const relogio::Param& p);
    // Texto de chip de um parâmetro armado (nome + valor se houver).
    std::wstring paramArmedText(relogio::ParamKind kind,
                                const std::wstring& value);

} // namespace app