#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "readconf/FormatRules.h"

// ============================================================================
// Modelo em MEMÓRIA do arquivo playlist.ini (Etapa 2).
//
// Diferente da Etapa 1 (que tratava o arquivo como texto cru), este documento
// estrutura o conteúdo em linhas e seções — o suficiente para a interface
// exibir/alterar os escopos de configuração — SEM perder NADA do texto real:
//
//   * cada linha guarda o texto ORIGINAL (para ressabidização fiel);
//   * comentários, seções e chaves desconhecidas são PRESERVADAS na ordem;
//   * alterações (formato, afiliadas) reescrevem APENAS as linhas afetadas.
//
// As regras de sintaxe (o que FORMATO=AUTO significa etc.) ficam em
// FormatRules; aqui só acontece a manipulação ESTRUTURAL do documento.
//
// O documento NÃO sabe nada de disco nem de codificação: recebe/serializa
// wstring. Disco e codificação são responsabilidade da camada externa
// (PlaylistIni/TextFileIO existentes da Etapa 1).
// ============================================================================

namespace readconf {

// Coisas que podem acontecer em uma linha do playlist.ini.
enum class IniLineKind {
    Blank,     // linha em branco (espaços apenas) — preservada
    Comment,   // comentário (; ou #) sem conteúdo CHAVE=VALOR
    Section,   // cabeçalho [NOME]
    Key,       // CHAVE=VALOR (incluindo linhas comentadas de função real)
};

// Uma linha do playlist.ini em memória.
struct IniLine {
    IniLineKind kind = IniLineKind::Blank;
    // Texto ORIGINAL da linha (sem o fim de linha), tal qual existia no disco.
    // É ESSA string que volta na serialização das linhas não alteradas.
    std::wstring raw;

    // Válido quando kind == Section: nome da seção, normalizado
    // (minúsculas, sem colchetes, sem espaços externos).
    std::wstring sectionName;
    // Nome da seção como escrito no arquivo (ex.: "BLOCO COMERCIAL").
    std::wstring rawSectionName;

    // Válido quando kind == Key:
    std::wstring rawKey;  // nome da chave como escrito (ex.: "FORMATO")
    std::wstring key;     // chave normalizada (minúsculas): "formato"
    std::wstring value;   // valor já aparado (ex.: "AUTO", "MAPAS\Mapa.txt")
    bool disabled = false;// linha escrita como ";CHAVE=valor" (config desativada)
};

// Documento do playlist.ini.
class PlaylistIniDocument {
public:
    // Constrói o documento vazio (nenhuma linha).
    PlaylistIniDocument() = default;

    // Substitui o conteúdo inteiro pelo texto fornecido (parse completo).
    // Preserva a ordem e o texto literal de todas as linhas.
    void setText(const std::wstring& text);

    // Serializa o estado atual de volta para texto, usando o fim de linha
    // original do arquivo (e preservando o \n final, se existia).
    std::wstring text() const;

    const std::vector<IniLine>& lines() const { return m_lines; }

    // ---------------------------------------------------------------------
    // Consulta
    // ---------------------------------------------------------------------

    // Índice da linha de cabeçalho da seção (ou -1). A comparação é
    // case-insensitive e tolerante a espaços.
    int sectionIndex(ConfigScope scope) const;

    // Índice da linha seguinte à seção (início da próxima seção ou fim do
    // arquivo), para delimitar o corpo da seção.
    int sectionEnd(int sectionIdx) const;

    // Valor de uma chave ATIVA (não desativada) dentro da seção. Devolve
    // false se a seção ou a chave não existirem.
    bool sectionKeyValue(ConfigScope scope,
                         const std::wstring& normalizedKey,
                         std::wstring& outValue) const;

    // ---------------------------------------------------------------------
    // Alteração de formato dos escopos
    // ---------------------------------------------------------------------

    // Aplica a opção de formato escolhida ao escopo: reescreve as chaves
    // FORMATO/ARQUIVO da seção. Se a seção não existe, a cria (com uma linha
    // em branco antes, se necessário). Devolve true em caso de sucesso.
    bool applyFormat(ConfigScope scope, FormatOption option);

    // Remove a seção inteira do escopo (cabeçalho e todas as linhas até a
    // próxima seção), se existir. Devolve true se removeu.
    bool removeSection(ConfigScope scope);

    // ---------------------------------------------------------------------
    // Afiliadas ([AFILIADAS])
    // ---------------------------------------------------------------------

    struct Afiliada {
        bool disabled = false;   // linha ";NOME=..." (afiliada desativada)
        std::wstring name;       // nome da afiliada = a CHAVE (ex.: "TESTE")
        std::wstring address;    // endereço (antes dos ':')
        std::wstring portText;   // porta como escrita (pode ser inválida/vazia)
        std::wstring rawValue;   // value completo como estava no disco
        int lineIndex = -1;      // índice da linha dentro do documento
    };

    // Lista as afiliadas na ordem do arquivo (ativas e desativadas). A chave
    // de cada linha é o NOME configurável da afiliada.
    std::vector<Afiliada> afiliadas() const;

    // Adiciona uma afiliada nova (ou cria a seção, se faltar).
    // Disabled controla se é gravada como ";NOME=".
    void addAfiliada(const std::wstring& name,
                     const std::wstring& address,
                     const std::wstring& portText,
                     bool disabled);

    // Atualiza a afiliada na posição da lista (veja afiliadas()).
    void updateAfiliada(size_t position,
                        const std::wstring& name,
                        const std::wstring& address,
                        const std::wstring& portText,
                        bool disabled);

    // Remove a afiliada na posição da lista.
    void removeAfiliada(size_t position);

    // True se a seção [AFILIADAS] existe no documento.
    bool hasAfiliadasSection() const;

    // Cria a seção [AFILIADAS] (sem entradas) caso ainda não exista.
    bool ensureAfiliadasSection();

private:
    // Operações internas sobre a lista de linhas.
    int findSectionLine(const std::wstring& normalizedName) const;
    int findKeyLine(int sectionIdx, const std::wstring& normalizedKey) const;
    void setKeyValue(int sectionIdx, const std::wstring& rawKeyName,
                     const std::wstring& normalizedKey,
                     const std::wstring& value);
    void eraseLineAt(int index);
    void insertKeyAfterHeader(int sectionIdx, const std::wstring& rawKeyName,
                              const std::wstring& normalizedKey,
                              const std::wstring& value);
    int ensureSection(ConfigScope scope);
    void parseLine(const std::wstring& raw);
    static std::wstring normalizedSection(const std::wstring& rawName);

    std::vector<IniLine> m_lines;
    std::wstring m_eol = L"\r\n";
    bool m_hasTrailingEol = false;
};

} // namespace readconf