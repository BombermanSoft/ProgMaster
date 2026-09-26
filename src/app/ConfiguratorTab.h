#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "readconf/FormatRules.h"
#include "readconf/ReadingConfiguration.h"

namespace app {
class PlaylistConfigController;
class ScopeCard;
class AfiliadaRow;
class AfiliadasCard;

// Guia "Configuração" (Etapa 2): NASCE da Etapa 2. Apresenta um cartão para
// cada escopo de configuração do playlist.ini ([BLOCO COMERCIAL],
// [BLOCO MUSICAL], [RELOGIO COMERCIAL], [RELOGIO MUSICAL] e [AFILIADAS]).
//
// Cada cartão mostra a opção de formato atual (ComboBox), os arquivos de
// programação esperados (✓/✗ no disco) e permite que o usuário altere tudo.
// As afiliadas são listadas com edição de nome/endereço/porta/ativa e
// remoção.
//
// Alterações ficam APENAS em memória (no PlaylistConfigController) até o
// SALVAR. O botão "✎ Visualizar como texto" grava o documento em disco (sem
// validação) e abre o próprio Bloco de Notas do Windows para edição manual;
// ao voltar para esta guia, o arquivo é relido do disco.
class ConfiguratorTab final : public juce::Component {
public:
    explicit ConfiguratorTab(PlaylistConfigController& controller);
    ~ConfiguratorTab() override = default;

    // Re-lê o documento (após trocar de localização ou salvar) e reconstrói.
    void refreshFromController();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    void savePlaylistIni();
    void discardChanges();
    // Atualiza o rótulo de estado ("alterações não salvas" / "documento em dia").
    void updateDirtyLabel();

private:
    // Abre o playlist.ini no Bloco de Notas do Windows (grava antes, sem
    // validação, para o editor mostrar o estado atual em memória).
    void openInNotepad();

    // Aplicou uma opção no ComboBox de um cartão.
    void onOptionChanged(readconf::ConfigScope scope,
                         readconf::FormatOption option);
    // Criou configuração ausente de um escopo ("+ Adicionar").
    void onAddScope(readconf::ConfigScope scope);
    // Removeu a seção inteira de um escopo (botão "Remover").
    void onRemoveScope(readconf::ConfigScope scope);
    // Afiliadas: sincronização de uma linha / remoção / adição.
    void onRowChanged(size_t position);
    void onRemoveAfiliada(size_t position);
    void onAddAfiliada();
    void onAddAfiliadaSection();

    // Reconstrói TODOS os cartões (combo/status/arquivos).
    void rebuildAll();
    // Re-aplica o layout vertical de todos os cartões dentro do conteúdo.
    void layoutContent();
    // Atualiza apenas o cartão de um escopo (sem recriar os controles).
    void refreshScopeCard(readconf::ConfigScope scope);
    // Reconstroi apenas o cartão de afiliadas (linhas novas/preenchidas).
    void refreshAfiliadasOnly();
    // Altura preferida do cartão de afiliadas (para o layout vertical).
    int preferredAfiliadasHeight() const;

    PlaylistConfigController& m_controller;

    // Acesso ao controlador pelos cartões (classes amigas).
    PlaylistConfigController& controller() noexcept { return m_controller; }

    juce::Label m_headerLabel;   // playlist.ini -> caminho
    juce::Label m_dirtyLabel;    // "alterações não salvas" / salvo

    juce::TextButton m_textModeButton;  // "✎ Visualizar como texto" (Bloco de Notas)
    bool m_reloadFromDiskOnVisible = false; // reler após editar no Bloco de Notas
    juce::Viewport m_visualArea;          // cartões

    juce::TextButton m_saveButton{ L"Salvar" };
    juce::TextButton m_discardButton{ L"Descartar alterações" };

    juce::Component m_content;            // área rolável que segura os cartões
    std::vector<std::unique_ptr<juce::Component>> m_cards;
    std::vector<ScopeCard*> m_scopeCards;
    AfiliadasCard* m_afiliadasCard = nullptr;

    friend class ScopeCard;
    friend class AfiliadaRow;
    friend class AfiliadasCard;
};

} // namespace app