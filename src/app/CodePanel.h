#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "blocos/CodeCatalogue.h"

namespace app {

// ============================================================================
// Painel da LISTA DE CÓDIGOS usado pelo editor visual de Mapas/Grades.
//
// Os códigos vêm do catálogo (folders.xml + códigos de sessão) e aparecem como
// botões COLORIDOS — um botão por código, com a cor determinística do seu
// índice. A MESMA cor é usada no painel e no "chip" do código dentro do
// horário, para o usuário reconhecer o código pela cor.
//
// O painel mostra NO MÁXIMO DUAS FILEIRAS por padrão. Quando os códigos não
// cabem, existem botões de navegação (‹ ›) que passam o conjunto de códigos
// exibido — o conjunto permanece completo, apenas a exibição avança. A altura
// pode ser redimensionada pelo usuário arrastando a borda inferior (de 1 a 8
// fileiras), e o número de fileiras é lembrado durante a sessão.
//
// Botões: "+" cria um CÓDIGO DE SESSÃO (não grava o folders.xml) e "lixeira"
// remove somente códigos de sessão. Clicar num código o ARMA (fica "grudado no
// mouse"); clicar no mesmo código, apertar ESC ou clicar com o botão direito
// desarma.
// ============================================================================

// Cor determinística de um código pelo seu índice no catálogo.
juce::Colour codeColour(int index);

class CodePanel final : public juce::Component {
public:
    explicit CodePanel(blocos::CodeCatalogue& catalogue);
    ~CodePanel() override;

    // Reconstrói os botões a partir do catálogo (após carregar o folders.xml ou
    // criar/remover um código de sessão).
    void rebuild();

    // Código atualmente ARMADO (vazio quando nenhum).
    std::wstring armedCode() const { return m_armed; }
    bool isArmed() const { return !m_armed.empty(); }
    void setArmed(const std::wstring& code);
    void cancelArmed();

    // Dispara quando um código é armado (a página usa para atualizar a dica e
    // redesenhar o chip fantasma).
    std::function<void(const std::wstring&)> onArmed;
    // Dispara quando o conjunto de códigos muda (criação/remoção/recarga).
    std::function<void()> onCatalogueChanged;
    // Dispara quando o usuário altera a quantidade de fileiras (o editor
    // reserva a altura correspondente).
    std::function<void(int)> onRowsChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // Quantidade de fileiras exibidas (1..8) — 2 por padrão.
    int rows() const { return m_rows; }
    void setRows(int rows);

    // Altura que o painel precisa para as fileiras atuais (com a fileira da
    // dica e da borda de redimensionamento). Quem contém o painel usa isso
    // para reservar o espaço vertical.
    int preferredHeight() const;

    // Texto de dica exibido ao lado do painel.
    void setHint(const juce::String& text) { m_hint.setText(text, juce::dontSendNotification); }

private:
    // Um botão de código: quadrado colorido com o DBFId escrito por cima.
    class CodeButton final : public juce::Button {
    public:
        CodeButton(std::wstring codeText, std::wstring titleText,
                   juce::Colour colour, blocos::CodeOrigin origin,
                   bool sessionOnly);

        std::wstring code() const { return m_code; }
        void setArmedUi(bool armed);
        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    private:
        std::wstring m_code;
        juce::Colour m_colour;
        blocos::CodeOrigin m_origin = blocos::CodeOrigin::FoldersXml;
        bool m_sessionOnly = false;
        bool m_armed = false;
    };

    void updatePagination();
    void updateNavButtons();
    void createCode();
    void removeArmedSessionCode();
    // Clique num código da paleta: arma (ou solta, se já estiver armado).
    void armFromButton(CodeButton* button);
    bool overResizeStrip(int y) const;
    int itemsPerPage() const { return m_itemsPerPage; }
    int itemWidth(int index) const;

    blocos::CodeCatalogue& m_catalogue;

    std::vector<std::unique_ptr<CodeButton>> m_codeButtons;
    // Botões visíveis na página atual (o catálogo inteiro é construído; só a
    // fatia da página é exibida).
    std::vector<CodeButton*> m_pageButtons;

    juce::TextButton m_prevBtn{ juce::String(L"‹") };
    juce::TextButton m_nextBtn{ juce::String(L"›") };
    juce::TextButton m_addBtn{ juce::String(L"+") };
    juce::TextButton m_trashBtn{ juce::String(L"\u2715") };
    juce::Label m_hint;

    std::wstring m_armed;

    int m_rows = 2;
    int m_page = 0;
    int m_itemsPerPage = 1;
    int m_pageCount = 1;

    // Arraste da borda inferior (redimensionamento do painel).
    bool m_draggingResize = false;
    int m_dragStartY = 0;
    int m_dragStartRows = 0;
    bool m_overResize = false;
};

} // namespace app
