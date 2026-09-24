#pragma once

#include <filesystem>
#include <string>

#include "playlist/PlaylistIni.h"
#include "readconf/FormatRules.h"
#include "readconf/PlaylistIniDocument.h"
#include "readconf/ReadingConfiguration.h"

// ============================================================================
// Controlador central da configuração (Etapa 2).
//
// É a ÚNICA fonte de verdade em memória do playlist.ini durante a edição:
//   * a interface visual altera o documento (doc) -> atualiza o texto;
//   * o editor textual entrega o texto -> o doc é re-interpretado ao voltar
//     para a interface;
//   * NADA é gravado até o SALVAR (tudo fica em memória).
//
// Gravação (SALVAR): valida -> serializa o documento (preservando tudo que
// não foi alterado) -> grava via PlaylistIni na codificação original.
// ============================================================================

namespace app {

class PlaylistConfigController {
public:
    explicit PlaylistConfigController(std::filesystem::path playlistIniPath);

    // Altera o caminho do playlist.ini (usado ao trocar de localização).
    void setPlaylistIniPath(std::filesystem::path playlistIniPath);

    // Pasta raiz da instalação do Playlist (usada para localizar os arquivos
    // de programação de cada escopo).
    void setInstallationFolder(std::filesystem::path installFolder);
    const std::filesystem::path& installationFolder() const { return m_installFolder; }

    // Carrega o playlist.ini do disco para o documento. Se o arquivo não
    // existir, inicia um documento vazio (todas as configurações "ausentes").
    // Nunca falha: a mensagem de aviso (se houver) fica em lastLoadMessage().
    void load();

    bool loaded() const { return m_loaded; }
    const std::wstring& lastLoadMessage() const { return m_loadMessage; }
    const std::filesystem::path& path() const;
    const std::wstring& displayName() const;

    // --- Estado -----------------------------------------------------------------

    // True quando há alterações não salvas no documento.
    bool isDirty() const { return m_dirty; }
    void markClean() { m_dirty = false; }

    const readconf::PlaylistIniDocument& document() const { return m_doc; }

    // Retrato dos escopos para a interface (interpretação + arquivos).
    std::vector<readconf::ScopeSnapshot> scopes() const;
    std::vector<readconf::PlaylistIniDocument::Afiliada> afiliadas() const;

    // --- Alterações pela INTERFACE VISUAL (marcam sujo) --------------------------

    // Aplica a opção de formato ao escopo. Criar a seção (se faltar).
    bool applyOption(readconf::ConfigScope scope, readconf::FormatOption option);

    // Cria uma configuração ausente (botão "+ Adicionar").
    bool addMissingConfiguration(readconf::ConfigScope scope);

    void addAfiliada(const std::wstring& address,
                     const std::wstring& portText,
                     bool disabled);
    void updateAfiliada(size_t position, const std::wstring& address,
                        const std::wstring& portText, bool disabled);
    void removeAfiliada(size_t position);

    // --- Editor TEXTUAL ----------------------------------------------------------

    // Texto atual serializado (para preencher o editor ao alternar p/ texto).
    std::wstring currentText() const;

    // Recebe o texto editado no editor e o torna o novo documento
    // (marca sujo). Usado ao alternar de volta para a interface visual.
    void setTextFromEditor(const std::wstring& text);

    // --- SALVAR ------------------------------------------------------------------

    // Valida e grava o documento no disco (codificação original preservada).
    // Em caso de erro, preenche userMessage/technicalError e retorna false;
    // NADA é gravado parcialmente.
    bool save(std::wstring& userMessage, std::string& technicalError);

private:
    void setDirty() { m_dirty = true; }

    PlaylistIni m_ini;
    readconf::PlaylistIniDocument m_doc;
    std::filesystem::path m_installFolder;
    bool m_loaded = false;
    bool m_dirty = false;
    std::wstring m_loadMessage;
};

} // namespace app