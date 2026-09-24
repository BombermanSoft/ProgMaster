#include "app/CodesTab.h"

#include <map>

#include "app/JuceHelpers.h"
#include "playlist/FoldersXml.h"
#include "playlist/PlaylistInstallation.h"

namespace app {

CodesTab::CodesTab(PlaylistInstallation& installation)
    : m_installation(installation)
{
    m_table.getHeader().addColumn("ID (DBFId)", 1, 230);
    m_table.getHeader().addColumn("Nome (Title)", 2, 220);
    m_table.getHeader().addColumn("Tipo (aux.)", 3, 140);
    m_table.getHeader().setStretchToFitActive(true);
    m_table.setModel(this);
    m_table.setMultipleSelectionEnabled(false);
    m_table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2b2b2b));
    m_table.setColour(juce::ListBox::textColourId, juce::Colours::white);
    m_table.setColour(juce::ListBox::outlineColourId, juce::Colours::dimgrey);
    addAndMakeVisible(m_table);

    m_summaryLabel.setText("Sem dados carregados.", juce::dontSendNotification);
    m_summaryLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_summaryLabel);
    addAndMakeVisible(m_statusLabel);
}

void CodesTab::reload()
{
    const std::filesystem::path foldersPath = m_installation.foldersXmlPath();
    const std::wstring sourcePath =
        foldersPath.empty() ? L"(arquivo não encontrado)" : foldersPath.wstring();

    m_entries.clear();
    m_entries.shrink_to_fit();
    m_table.updateContent();

    if (foldersPath.empty()) {
        m_statusLabel.setText("Arquivo folders.xml não encontrado na instalação.",
                              juce::dontSendNotification);
        m_summaryLabel.setText("Nenhum registro carregado.", juce::dontSendNotification);
        return;
    }

    std::string err;
    const std::wstring message = FoldersXml::readAll(foldersPath, m_entries, err);

    if (!message.empty()) {
        m_statusLabel.setText(jstr(message), juce::dontSendNotification);
        m_summaryLabel.setText("Nenhum registro carregado.", juce::dontSendNotification);
        m_table.updateContent();
        return;
    }

    m_table.updateContent();

    // Resumo: total de registros, sem DBFId e repetidos.
    int noCode = 0;
    int repeated = 0;
    {
        std::map<std::wstring, int> count;
        for (const auto& e : m_entries) {
            if (e.dbfId.empty()) {
                ++noCode;
            } else {
                ++count[e.dbfId];
            }
        }
        for (const auto& pair : count) {
            if (pair.second > 1) {
                ++repeated;
            }
        }
    }

    std::wstring summary = L"Total de registros: " + std::to_wstring(m_entries.size());
    if (noCode > 0) {
        summary += L"  |  Sem DBFId: " + std::to_wstring(noCode);
    }
    if (repeated > 0) {
        summary += L"  |  DBFId repetidos: " + std::to_wstring(repeated);
    }
    m_summaryLabel.setText(jstr(summary), juce::dontSendNotification);
    m_statusLabel.setText(jstr(sourcePath), juce::dontSendNotification);
}

int CodesTab::getNumRows()
{
    return static_cast<int>(m_entries.size());
}

void CodesTab::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/,
                                  int height, bool rowIsSelected)
{
    if (rowIsSelected) {
        g.setColour(juce::Colour(0xff3a5b8a));
    } else if (rowNumber % 2 == 1) {
        g.setColour(juce::Colour(0xff323232));
    } else {
        g.setColour(juce::Colour(0xff2b2b2b));
    }
    g.fillRect(0, 0, getWidth(), height);
}

void CodesTab::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                         int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(m_entries.size())) {
        return;
    }

    const FolderEntry& e = m_entries[static_cast<size_t>(rowNumber)];
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.setColour(rowIsSelected ? juce::Colours::white : juce::Colours::lightgrey);

    const int padding = 4;
    const juce::Rectangle<int> textArea(padding, 0, width - 2 * padding, height);

    switch (columnId) {
    case 1: {
        const std::wstring code = e.hasDbfId() ? e.dbfId : L"(sem código)";
        if (!e.hasDbfId()) {
            g.setColour(juce::Colours::orange);
        }
        g.drawText(jstr(code), textArea, juce::Justification::centredLeft, true);
        break;
    }
    case 2:
        g.drawText(jstr(e.title), textArea, juce::Justification::centredLeft, true);
        break;
    case 3:
        g.drawText(jstr(e.type), textArea, juce::Justification::centredLeft, true);
        break;
    default:
        break;
    }
}

juce::Component* CodesTab::refreshComponentForCell(int, int, bool, juce::Component* existing)
{
    ignoreUnused(existing);
    return nullptr;
}

void CodesTab::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void CodesTab::resized()
{
    const int margin = 6;
    const int labelH = 20;
    const auto area = getLocalBounds();

    m_summaryLabel.setBounds(area.getX() + margin, area.getY() + 2,
                             area.getWidth() - 2 * margin, labelH);
    m_table.setBounds(area.getX() + margin, area.getY() + labelH + 4,
                      area.getWidth() - 2 * margin,
                      juce::jmax(0, area.getBottom() - (area.getY() + labelH + 4)
                                        - 2 * labelH - 8));
    m_statusLabel.setBounds(area.getX() + margin,
                            juce::jmax(0, m_table.getBottom()) + 4,
                            area.getWidth() - 2 * margin, labelH);
}

} // namespace app