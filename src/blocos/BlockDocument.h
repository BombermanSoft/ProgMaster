#pragma once

#include <string>
#include <vector>

// ============================================================================
// Modelo em MEMÓRIA de um arquivo de BLOCO (Mapa Comercial / Grade Musical)
// — Etapa 4.
//
// Um bloco é um arquivo de texto com UM horário por linha, no formato real do
// Playlist (exemplos verificados nos arquivos da instalação oficial):
//
//   Mapa.txt   ->  00:27 VHAB, COMER, COMER, COMER, COMER, COMER, VHAB,
//   GRADE.txt  ->  00:00  INT, NAC, VHPAS, INT, NAC, VHPAS, ...
//
// Ou seja: horário, espaço, e a lista de CÓDIGOS separados por vírgula
// (o manual oficial diz que a vírgula separa um código do outro e que o
// espaço após a vírgula é opcional). Nos arquivos reais a LINHA TERMINA com
// ", " (vírgula + espaço) depois do último código — por isso o separador
// original e o que vem depois do último código são PRESERVADOS.
//
// Os códigos são os DBFId da Lista de Códigos (folders.xml, somente leitura):
// COMER, MUSI, VHAB, VHHORCER, VHPAS, VIDCOM, INT, NAC...
//
// Não há parênteses, não há colchetes e não existe campo de nome/valor: o
// código é apenas o identificador gravado no arquivo. Esta etapa NÃO inventa
// sintaxe: o que não for reconhecido na linha é preservado verbatim.
//
// Fidelidade: linhas que não são horário (em branco, comentários, seções
// desconhecidas) são guardadas VERBATIM e preservadas na ordem, assim como o
// separador entre o horário e os códigos e o trecho final da linha.
// ============================================================================

namespace blocos {

// Um horário do bloco com a lista de códigos do seu trecho.
struct Horario {
    std::wstring time;                    // "00:27"
    std::vector<std::wstring> codes;      // códigos, na ordem do arquivo
    std::wstring sep;                     // separador entre o horário e o
                                          // primeiro código (ex.: " ")
    std::wstring trailing;                // o que vem DEPOIS do último
                                          // código (ex.: ", "), preservado
};

class BlockDocument {
public:
    // Uma linha do documento: horário estruturado ou linha crua preservada.
    struct Line {
        enum class Kind { Horario, Raw } kind = Kind::Raw;
        Horario horario;   // válido quando kind == Horario
        std::wstring raw;  // válido quando kind == Raw (sem fim de linha)
    };

    // Substitui o conteúdo inteiro pelo texto fornecido (parse completo).
    // Detecta o fim de linha do arquivo e preserva o \n final.
    void setText(const std::wstring& text);

    // Serializa o estado atual de volta para o texto (mesmo fim de linha,
    // mesmo separador e mesmo trecho final de cada linha).
    std::wstring text() const;

    const std::vector<Line>& lines() const { return m_lines; }

    // ---------------------------------------------------------------------
    // Alteração (editor visual)
    // ---------------------------------------------------------------------

    // Adiciona um horário HH:MM na posição cronológica, pulando se já existir.
    // Devolve o índice da linha criada (-1 se duplicado ou horário inválido).
    int addTime(const std::wstring& hhmm);

    // Adiciona um código ao horário da linha lineIndex. Devolve false (e NÃO
    // adiciona) se o código já estiver nesse horário.
    bool addCode(int lineIndex, const std::wstring& code);

    // Remove o código na posição codeIndex do horário.
    void removeCode(int lineIndex, int codeIndex);

    // Substitui TODOS os códigos do horário (copiar/colar entre horários),
    // sem nunca deixar o mesmo código repetido.
    void replaceCodes(int lineIndex, const std::vector<std::wstring>& codes);

    // Apaga a LINHA inteira (horário ou crua) do documento.
    bool removeLine(int lineIndex);

    // "Preencher": remove os horários existentes, mantém as linhas cruas e
    // insere a lista fornecida em ordem cronológica. Horários inválidos e
    // repetidos são ignorados.
    void reschedule(const std::vector<std::wstring>& hhmmList);

    // Substitui o conteúdo inteiro pelo de outro documento.
    void replaceFrom(const BlockDocument& other);

    // ---------------------------------------------------------------------
    // Utilidades (puras)
    // ---------------------------------------------------------------------

    // "00:00" -> minutos (0..1439) ou -1 se inválido.
    static int parseTime(const std::wstring& hhmm);
    // minutos (0..1439) -> "HH:MM" (sempre 2 dígitos).
    static std::wstring formatTime(int minutes);

    // Interpreta uma linha de bloco. Devolve false quando a linha não começa
    // com um horário válido (nesse caso o chamador guarda a linha como crua).
    static bool parseLine(const std::wstring& line, Horario& out);

    // Serializa um horário no formato do arquivo (usado por text()).
    static std::wstring horarioText(const Horario& h);

private:
    // Índice da primeira linha de horário cujo tempo é >= minutos.
    size_t insertionIndex(int minutes) const;

    std::vector<Line> m_lines;
    std::wstring m_eol = L"\r\n";
    bool m_hasTrailingEol = true;
};

} // namespace blocos
