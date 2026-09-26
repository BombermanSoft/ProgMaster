#pragma once

#include <string>
#include <vector>

// ============================================================================
// Modelo em MEMÓRIA de um arquivo de Relógio (Etapa 3).
//
// Um relógio (Relogio.txt / RelogioSeg.txt..) é um arquivo de texto com UM
// horário por linha, no formato real do Playlist:
//
//   00:00
//   00:15
//   ...
//
// E quando o horário carrega parâmetros (como no Modelo.txt de referência):
//
//   00:02 (DUR=13:00) AB, LOC1, MUS1, VHP, ...
//
// Cada linha pode ter, DEPOIS do horário, parâmetros entre parênteses:
//   (FIXO)  (DESCARTE)  (LOCAL)  (SAT)  (LOCKED)     <- sem valor
//   (ID=Exemplo)  (DUR=3:00)                          <- com valor
// O que vem depois dos parâmetros (conteúdo de programação do horário, ex.:
// "AB, LOC1, MUS1...") é CONTEÚDO PRESERVADO: o editor não interpreta nem
// apaga (item 43/44 do prompt).
//
// Fidelidade: cada linha guarda o texto ORIGINAL ("linha crua") para linhas
// que não são horário, e para horários guarda a hora + os tokens de parâmetro
// exatamente como escritos (incluindo o espaço que os precede) + o restante da
// linha intacto. Nada é perdido ao serializar: linhas em branco, comentários
// e linhas desconhecidas são preservados na ordem.
//
// Linhas em branco e comentários são representados como "Raw" e ficam fora do
// modelo visual (o editor visual trabalha com os horários).
// ============================================================================

namespace relogio {

// Parâmetro do relógio conhecido nesta etapa (item 43 do prompt).
enum class ParamKind {
    Fixo,     // (FIXO)
    Descarte, // (DESCARTE)
    Local,    // (LOCAL)
    Sat,      // (SAT)
    Locked,   // (LOCKED)
    Id,       // (ID=valor)
    Dur,      // (DUR=valor)
};

// Um parâmetro de um horário, com o token EXATO como aparece no arquivo
// (incluindo o espaço que o precede) para round-trip fiel.
struct Param {
    ParamKind kind = ParamKind::Fixo;
    std::wstring name;  // nome canônico maiúsculo (FIXO, ID, DUR...)
    std::wstring value; // apenas para Id/Dur (senão vazio)
    // (NAME) ou (NAME=valor), precedido pelo espaço/whitespace original.
    std::wstring token;
};

// Um horário do relógio.
struct Horario {
    std::wstring time;              // "00:02"
    std::vector<Param> params;      // parâmetros reconhecidos, na ordem
    std::wstring trailing;          // resto da linha depois dos parâmetros
                                    // (conteúdo preservado, verbatim)
};

class RelogioDocument {
public:
    // Uma linha do documento: ou um horário estruturado ou uma linha crua
    // preservada (em branco, comentário, seção desconhecida...).
    struct Line {
        enum class Kind { Horario, Raw } kind = Kind::Raw;
        Horario horario;      // válido quando kind == Horario
        std::wstring raw;     // válido quando kind == Raw (sem fim de linha)
    };

    // Substitui o conteúdo inteiro pelo texto fornecido (parse completo).
    // Detecta o fim de linha do arquivo e preserva o \n final.
    void setText(const std::wstring& text);

    // Serializa o estado atual de volta para o texto (mesmo fim de linha e
    // preservando se o arquivo terminava com quebra de linha).
    std::wstring text() const;

    const std::vector<Line>& lines() const { return m_lines; }

    // ---------------------------------------------------------------------
    // Alteração (editor visual)
    // ---------------------------------------------------------------------

    // Adiciona um horário HH:MM na posição cronológica (ordem do arquivo),
    // pulando se já existir. Devolve o índice da linha criada (-1 se duplicado
    // ou horário inválido).
    int addTime(const std::wstring& hhmm);

    // Adiciona um parâmetro ao horário na linha de índice lineIndex.
    void addParam(int lineIndex, ParamKind kind, const std::wstring& value);

    // Remove o parâmetro na posição paramIndex do horário.
    void removeParam(int lineIndex, int paramIndex);

    // Substitui TODOS os parâmetros do horário (copiar/colar entre horários).
    void replaceParams(int lineIndex, const std::vector<Param>& params);

    // Apaga a LINHA inteira (horário ou crua) do documento.
    bool removeLine(int lineIndex);

    // Substitui o conteúdo inteiro pelo de outro documento (copiar relógio ->
    // outro relógio). Copia também o fim de linha.
    void replaceFrom(const RelogioDocument& other);

    // ---------------------------------------------------------------------
    // Utilidades (puras)
    // ---------------------------------------------------------------------

    // "00:00" -> minutos (0..1439) ou -1 se inválido.
    static int parseTime(const std::wstring& hhmm);
    // minutos (0..1439) -> "HH:MM" (sempre 2 dígitos).
    static std::wstring formatTime(int minutes);

    // Interpreta o conteúdo de um parêntese ("FIXO", "ID=Exemplo") e devolve
    // o Param. Retorna false quando o token não é um parâmetro conhecido.
    static bool parseParamToken(const std::wstring& token, Param& out);

    // "(NAME)" ou "(NAME=valor)" (sem espaço à esquerda).
    static std::wstring paramText(const Param& p);

private:
    // Índice da primeira linha de horário cujo tempo é >= minutos (ponto de
    // inserção cronológica). Devolve m_lines.size() quando for para o fim.
    size_t insertionIndex(int minutes) const;

    std::vector<Line> m_lines;
    std::wstring m_eol = L"\r\n";
    bool m_hasTrailingEol = true;
};

} // namespace relogio