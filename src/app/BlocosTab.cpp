#include "app/BlocosTab.h"

#include "app/JuceHelpers.h"

namespace app {

BlocosTab::BlocosTab()
{
    m_title.setColour(juce::Label::textColourId, juce::Colours::white);
    m_title.setFont(juce::Font(juce::FontOptions(
        juce::Font::getDefaultSansSerifFontName(), "Bold", 18.0f)));
    m_title.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(m_title);

    m_detail.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    m_detail.setFont(juce::Font(juce::FontOptions(13.0f)));
    m_detail.setJustificationType(juce::Justification::topLeft);
    m_detail.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(m_detail);

    setMusical(true);
}

void BlocosTab::setMusical(bool musical)
{
    m_musical = musical;
    m_title.setText(jstr(musical ? L"Blocos Musicais" : L"Blocos Comerciais"),
                    juce::dontSendNotification);
    m_detail.setText(
        jstr(L"O editor visual dos blocos está previsto para uma próxima "
             L"etapa.\n\n"
             L"Por enquanto, a edição destes arquivos está disponível em "
             L"\"Avançado ▸\n" +
             (musical ? std::wstring(L"Grades Musicais")
                      : std::wstring(L"Mapa Comercial")) +
             L"\" (bloco de notas textual).\n\n"
             L"NECESSITA DE EXEMPLO REAL para especificar o editor visual."),
        juce::dontSendNotification);
}

void BlocosTab::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void BlocosTab::resized()
{
    const int margin = 16;
    m_title.setBounds(margin, margin, getWidth() - 2 * margin, 24);
    m_detail.setBounds(margin, margin + 28, getWidth() - 2 * margin,
                       getHeight() - margin - 28 - margin);
}

} // namespace app