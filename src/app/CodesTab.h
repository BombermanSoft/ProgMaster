#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <vector>

#include "models/FolderEntry.h"

class PlaylistInstallation;

namespace app {

// Guia "Códigos": exibe em uma tabela todos os registros do folders.xml
// (Etapa 1). A coluna principal é o DBFId; Title auxilia a identificação.
// O arquivo é SOMENTE LEITURA (regra mantida desde a Etapa 1).
class CodesTab final : public juce::Component,
                       public juce::TableListBoxModel {
public:
    explicit CodesTab(PlaylistInstallation& installation);
    ~CodesTab() override = default;

    // Relê o folders.xml da instalação atual e preenche a tabela.
    // Exibe mensagem amigável quando o arquivo não existir.
    void reload();

    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
                            bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width,
                   int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId,
                                             bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void clearTable();

    PlaylistInstallation& m_installation;

    juce::TableListBox m_table;
    juce::Label m_statusLabel; // fonte do folders.xml (rodapé da guia)
    juce::Label m_summaryLabel; // resumo: totais / duplicados

    std::vector<FolderEntry> m_entries;
    std::wstring m_sourcePath;
};

} // namespace app