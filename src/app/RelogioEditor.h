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
// Horários: "Início" adiciona 00:00; "Intervalo" adiciona o horário do MEIO
// entre o horário selecionado e o seguinte; "Avulso..." pede um HH:MM.
//
// Copiar/Colar: o horário selecionado tem os parâmetros copiados (Ctrl+C ou
// botão "Copiar") para a área de transferência COMPARTILHADA entre os
// relógios (as abas); "Colar" (Ctrl+V) aplica no horário selecionado.
// ============================================================================

namespace app {

class RelogioEditor final : public juce::Component {
public:
    // doc: modelo do arquivo em edição; clipboard: área de transferência
    // COMPARTILHADA com os demais editores do RelogioEditorTab.
    RelogioEditor(relogio::RelogioDocument& doc,
                  std::vector<relogio::Param>& clipboard);
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
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    // Uma linha visual = um horário do documento (linhas cruas não aparecem).
    struct Row {
        int lineIndex = -1; // índice da LINE no RelogioDocument
        std::wstring time;
        std::wstring trailing;
        std::vector<relogio::ParamKind> kinds;
        std::vector<std::wstring> chipTexts;
    };

    void rebuildRows();
    void notifyChanged();
    void setStatus(const juce::String& text);
    void refreshButtons();

    std::pair<int, int> rowAt(const juce::Point<int>& pos) const;
    int chipAtX(const Row& row, int x) const;
    void selectRow(int index);

    // Paleta "grudada".
    void armParam(relogio::ParamKind kind);
    void cancelArmed();
    void updateArmedUi();

    // Empregos de horário.
    void addInicio();
    void addIntervalo();
    void addAvulso();
    void copySelected();
    void pasteSelected();

    // Layout (preenchido em resized).
    juce::Rectangle<int> m_listRect;

    relogio::RelogioDocument& m_doc;
    std::vector<relogio::Param>& m_clipboard;
    std::vector<Row> m_rows;
    int m_selectedRow = -1;
    int m_hoverRow = -1;
    int m_scrollY = 0;

    bool m_armed = false;
    relogio::ParamKind m_armedKind = relogio::ParamKind::Fixo;
    std::wstring m_armedValue;
    juce::Point<int> m_mousePos;
    bool m_mouseOverList = false;

    juce::TextButton m_fixoBtn{ "(FIXO)" };
    juce::TextButton m_descarteBtn{ "(DESCARTE)" };
    juce::TextButton m_localBtn{ "(LOCAL)" };
    juce::TextButton m_satBtn{ "(SAT)" };
    juce::TextButton m_lockedBtn{ "(LOCKED)" };
    juce::TextButton m_idBtn{ "(ID=)" };
    juce::TextButton m_durBtn{ "(DUR=)" };
    juce::Label m_hintLabel;
    juce::TextButton m_iniBtn{ L"Início" };
    juce::TextButton m_intBtn{ L"Intervalo" };
    juce::TextButton m_avulsoBtn{ L"Avulso..." };
    juce::TextButton m_copyBtn{ L"Copiar" };
    juce::TextButton m_pasteBtn{ L"Colar" };
    juce::Label m_statusLabel;
};

// Cor dos chips por parâmetro (paleta e linhas usam a mesma cor).
juce::Colour paramColour(relogio::ParamKind kind);

// Texto pede ao usuário um valor de ID/DUR; devolve vazio se cancelado.
std::wstring askParamValue(relogio::ParamKind kind, juce::Component& parent);

} // namespace app