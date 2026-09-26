#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Tela provisória (Etapa 3) para os itens de menu "Blocos Musicais" e
// "Blocos Comerciais": os atalhos existem, mas a edição visual dos blocos
// fica para uma etapa futura (NECESSITA DE EXEMPLO REAL — ver prompt).

namespace app {

class BlocosTab final : public juce::Component {
public:
    BlocosTab();

    void setMusical(bool musical);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    bool m_musical = true;
    juce::Label m_title;
    juce::Label m_detail;
};

} // namespace app