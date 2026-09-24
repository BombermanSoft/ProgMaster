#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "readconf/FormatRules.h"
#include "readconf/PlaylistFileLocator.h"
#include "readconf/PlaylistIniDocument.h"

// ============================================================================
// Camada de INTERPRETAÇÃO (E+tapa 2, item 42): lê o documento em memória e
// produz o retrato dos escopos de configuração que a interface exibe.
//
// Também centraliza a VALIDAÇÃO executada antes do SALVAR (itens 13/14 do
// prompt): nada é gravado parcialmente — qualquer erro aborta a gravação.
// ============================================================================

namespace readconf {

// Retrato de um escopo (uma seção do playlist.ini) para a interface.
struct ScopeSnapshot {
    ConfigScope scope = ConfigScope::Comercial;
    bool present = false;        // a seção EXISTE no documento
    FormatOption option = FormatOption::Unknown;
    std::wstring matchedToken;   // token real reconhecido (ex.: "%w")
    std::wstring formatoAsWritten;
    std::wstring arquivoAsWritten;
    std::vector<FileBinding> files;   // arquivos esperados x disco (✓/✗)
};

// Lê todos os escopos que possuem formato ([BLOCO ...] e [RELOGIO ...]).
std::vector<ScopeSnapshot> readScopes(const PlaylistIniDocument& doc,
                                      const std::filesystem::path& installationFolder);

// Lê um escopo específico.
ScopeSnapshot readScope(const PlaylistIniDocument& doc,
                        ConfigScope scope,
                        const std::filesystem::path& installationFolder);

// Lê as afiliadas ([AFILIADAS]) na ordem do arquivo.
std::vector<PlaylistIniDocument::Afiliada> readAfiliadas(const PlaylistIniDocument& doc);

// ---------------------------------------------------------------------------
// Validação para o SALVAR (itens 13/14 do prompt)
// ---------------------------------------------------------------------------

// Resultado da validação do documento ANTES de gravar.
struct ValidationResult {
    bool ok = false;                 // true quando pode gravar
    std::wstring errorMessage;       // primeiro problema (amigável)
    std::wstring detail;             // detalhe técnico (para exibir ao usuário)
};

// Valida:
//   * todas as afiliadas têm endereço e porta numérica válida (1..65535);
//   * não há afiliadas duplicadas (mesmo endereço:porta, ativas);
//   * o estado do documento é consistente (seções reconhecíveis).
// Se algo estiver errado, ok=false e o problema vem descrito (NÃO gravar).
ValidationResult validateForSave(const PlaylistIniDocument& doc);

} // namespace readconf