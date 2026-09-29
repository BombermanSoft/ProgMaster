#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

#include "app/CodePanel.h"
#include "blocos/BlockDocument.h"
#include "blocos/CodeCatalogue.h"

// ============================================================================
// Editor VISUAL de um arquivo de Bloco (Mapa Comercial / Grade Musical) —
// Etapa 4.
//
// Opera sobre um blocos::BlockDocument (o modelo em memória do arquivo). A
// interface NÃO inventa sintaxe: cada horário do arquivo é mostrado com os
// CÓDIGOS em chips coloridos (a cor é a do botão no painel de códigos) e o
// trecho final preservado da linha em cinza.
//
// "Código grudado no mouse": clicar num código no painel ARMA o código — um
// chip fantasma acompanha o cursor e o clique num horário o adiciona, mantendo-o
// armado para o próximo horário. Desarma: clicar no mesmo código, botão direito
// ou ESC.
//
// Horários: o campo "Adicionar Horário" + OK insere um horário; "Preencher
// Horários" abre um diálogo (Início + Intervalo) e gera a sequência do dia;
// "Remover" pergunta o que remover dos selecionados — Horários (apaga as
// linhas inteiras) ou Códigos (apaga só os chips, mantendo o horário).
//
// Seleção: o botão quadrado habilita a seleção; ativado, vira o quadrado
// "selecionar todos". O combo + "Aplicar" envia um código para TODOS os
// marcados.
//
// Copiar/Colar: "Copiar" (Ctrl+C) copia horários + códigos dos selecionados
// para a área de transferência COMPARTILHADA entre os arquivos (as abas);
// "Colar" (Ctrl+V) traz esses horários para o documento.
// ============================================================================

namespace app {

// Um horário "copiado": hora + códigos. Compartilhado entre os arquivos.
struct BlockClip {
    std::wstring time;
    std::vector<std::wstring> codes;
};

class BlockEditor final : public juce::Component {
public:
    BlockEditor(blocos::BlockDocument& doc, blocos::CodeCatalogue& catalogue,
                std::vector<BlockClip>& clipboard);
    ~BlockEditor() override;

    // Relê o documento e reconstrói as linhas visuais.
    void rebuild();

    // Disparado quando este editor ALTERA o documento.
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

    // Código armado (o painel de códigos compartilha o estado).
    bool isArmed() const { return m_codePanel.isArmed(); }
    std::wstring armedCode() const { return m_codePanel.armedCode(); }

private:
    struct Row {
        int lineIndex = -1;
        std::wstring time;
        juce::String timeStr;
        std::wstring trailing;
        juce::String trailingStr;
        // Índices no catálogo (para a cor) e o texto/largura do chip.
        std::vector<int> colourIndex;
        std::vector<juce::String> chipStr;
        std::vector<int> chipW;
    };

    void rebuildRows();
    void refreshRow(int rowIndex);
    // Refaz o combo de códigos (após carregar o folders.xml ou criar/remover).
    void rebuildCombo();
    void notifyChanged();
    void setStatus(const juce::String& text);
    void refreshButtons();

    std::pair<int, int> rowAt(const juce::Point<int>& pos) const;
    int chipAtX(const Row& row, int x) const;
    void selectRow(int index);
    juce::Rectangle<int> rowRect(int rowIndex) const;
    int chipsBaseX() const;
    juce::Rectangle<int> ghostRect(const juce::Point<int>& pos) const;

    bool hasSelection() const;
    std::vector<int> selectedRowIndices() const;
    void toggleSelectedRow(int rowIndex);
    void selectAllRows();
    void selectNoRows();
    void onSelectCheckClick();
    void updateSelectCheck();

    void clampScroll();
    int contentHeight() const;
    int sliderThumbHeight() const;
    juce::Rectangle<int> sliderThumb() const;
    bool overSliderStrip(const juce::Point<int>& pos) const;

    void addFromField();
    void fillTimes();
    void removeSelected();
    void applyCodeToSelected();
    void copySelected();
    void pasteSelected();
    int findTimeLine(const std::wstring& hhmm) const;

    class SelectCheckButton final : public juce::Button {
    public:
        SelectCheckButton() : juce::Button(L"Selecionar") {}
        void setState(bool modeOn, bool allSelected);
        bool isModeOn() const { return m_modeOn; }

    private:
        void paintButton(juce::Graphics& g, bool isOver, bool isDown) override;
        bool m_modeOn = false;
        bool m_allSelected = false;
    };

    blocos::BlockDocument& m_doc;
    blocos::CodeCatalogue& m_catalogue;
    std::vector<BlockClip>& m_clipboard;

    CodePanel m_codePanel;
    std::vector<Row> m_rows;
    int m_selectedRow = -1;
    int m_hoverRow = -1;
    int m_scrollY = 0;

    bool m_selectMode = false;
    std::vector<bool> m_rowSelected;

    juce::Point<int> m_mousePos;
    bool m_mouseOverList = false;

    bool m_dragSlider = false;
    int m_dragStartThumbY = 0;
    int m_dragStartScrollY = 0;

    juce::Label m_hintLabel;
    juce::TextEditor m_timeField;
    juce::TextButton m_addTimeBtn{ L"OK" };
    juce::TextButton m_fillBtn{ L"Preencher Horários" };
    juce::TextButton m_removeBtn{ L"Remover" };
    juce::TextButton m_copyBtn{ L"Copiar" };
    juce::TextButton m_pasteBtn{ L"Colar" };
    SelectCheckButton m_selectBtn;
    juce::ComboBox m_codeCombo;
    juce::TextButton m_applyBtn{ L"Aplicar" };
    juce::Label m_statusLabel;

    juce::Rectangle<int> m_listRect;
    juce::Rectangle<int> m_toolArea;
    int m_sep1X = -1;
    int m_sep2X = -1;
    int m_panelH = 0;
};

} // namespace app
