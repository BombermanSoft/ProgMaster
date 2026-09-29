#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "blocos/BlockDocument.h"
#include "blocos/CodeCatalogue.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what, const char* file, int line)
{
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cout << "  FALHOU " << file << ":" << line << " - " << what << "\n";
    }
}

#define CHECK_MSG(cond, what) check((cond), what, __FILE__, __LINE__)

void checkEq(const std::wstring& actual, const std::wstring& expected,
             const std::string& what, const char* file, int line)
{
    ++g_checks;
    if (actual != expected) {
        ++g_failures;
        const auto toDisplay = [](const std::wstring& s) {
            std::string r;
            for (wchar_t c : s) {
                r += (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
            }
            return r;
        };
        std::cout << "  FALHOU " << file << ":" << line << " - " << what
                  << "\n      esperado: [" << toDisplay(expected)
                  << "]\n      obtido:   [" << toDisplay(actual) << "]\n";
    }
}

#define CHECK_EQ(actual, expected) \
    checkEq((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)

using namespace blocos;

int countHorarios(const BlockDocument& d)
{
    int n = 0;
    for (const BlockDocument::Line& l : d.lines()) {
        if (l.kind == BlockDocument::Line::Kind::Horario) {
            ++n;
        }
    }
    return n;
}

int lineOfTime(const BlockDocument& d, const std::wstring& hhmm)
{
    for (size_t i = 0; i < d.lines().size(); ++i) {
        const BlockDocument::Line& l = d.lines()[i];
        if (l.kind == BlockDocument::Line::Kind::Horario &&
            l.horario.time == hhmm) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int codeCount(const BlockDocument& d, const std::wstring& hhmm)
{
    const int idx = lineOfTime(d, hhmm);
    if (idx < 0) {
        return -1;
    }
    return static_cast<int>(d.lines()[static_cast<size_t>(idx)].horario.codes.size());
}

bool hasCode(const BlockDocument& d, const std::wstring& hhmm,
             const std::wstring& code)
{
    const int idx = lineOfTime(d, hhmm);
    if (idx < 0) {
        return false;
    }
    const auto& codes =
        d.lines()[static_cast<size_t>(idx)].horario.codes;
    for (const std::wstring& c : codes) {
        if (c == code) {
            return true;
        }
    }
    return false;
}

// Quantas vezes um código aparece no horário (o formato real repete códigos:
// o Mapa oficial traz COMER cinco vezes na primeira linha).
int occurrences(const BlockDocument& d, const std::wstring& hhmm,
                const std::wstring& code)
{
    const int idx = lineOfTime(d, hhmm);
    if (idx < 0) {
        return -1;
    }
    int n = 0;
    for (const std::wstring& c :
         d.lines()[static_cast<size_t>(idx)].horario.codes) {
        if (c == code) {
            ++n;
        }
    }
    return n;
}

// ============================================================================
// Formato real (conteúdo copiado de C:\Playlist_oficial\pgm\Mapas\Mapa.txt e
// ...\Grades\GRADE.txt): horário, espaço, códigos separados por ", " e a linha
// TERMINANDO em ", " depois do último código.
// ============================================================================

void testParseMapaReal()
{
    const std::wstring texto =
        L"00:27 VHAB, COMER, COMER, COMER, COMER, COMER, VHAB, \r\n"
        L"00:57 VHAB, COMER, COMER, COMER, COMER, COMER, VHAB, \r\n"
        L"23:57 VHAB, COMER, \r\n";

    BlockDocument d;
    d.setText(texto);

    CHECK_MSG(countHorarios(d) == 3, "3 horarios no Mapa real");
    const int idx = lineOfTime(d, L"00:27");
    CHECK_MSG(idx >= 0, "horario 00:27 encontrado");
    if (idx >= 0) {
        const auto& h = d.lines()[static_cast<size_t>(idx)].horario;
        CHECK_EQ(h.time, L"00:27");
        CHECK_EQ(h.sep, L" ");
        CHECK_MSG(h.codes.size() == 7, "7 codigos em 00:27");
        if (!h.codes.empty()) {
            CHECK_EQ(h.codes[0], L"VHAB");
            CHECK_EQ(h.codes[1], L"COMER");
        }
        if (h.codes.size() >= 7) {
            CHECK_EQ(h.codes[6], L"VHAB");
        }
        // A vírgula + espaço final é o `trailing` preservado do arquivo real.
        CHECK_EQ(h.trailing, L", ");
    }
    CHECK_MSG(codeCount(d, L"23:57") == 2, "2 codigos em 23:57");

    // Ida e volta sem alteração devolve exatamente o texto original.
    CHECK_EQ(d.text(), texto);
}

void testParseGradeReal()
{
    const std::wstring texto =
        L"00:00 INT, NAC, VHPAS, INT, NAC, VHPAS, INT, NAC, VHPAS, INT, NAC, \r\n"
        L"23:30 INT, NAC, VHPAS, \r\n";

    BlockDocument d;
    d.setText(texto);

    CHECK_MSG(countHorarios(d) == 2, "2 horarios na Grade real");
    const int idx = lineOfTime(d, L"00:00");
    CHECK_MSG(idx >= 0, "horario 00:00 encontrado");
    if (idx >= 0) {
        const auto& h = d.lines()[static_cast<size_t>(idx)].horario;
        CHECK_MSG(h.codes.size() == 11, "11 codigos em 00:00");
        if (!h.codes.empty()) {
            CHECK_EQ(h.codes[0], L"INT");
        }
        if (h.codes.size() >= 3) {
            CHECK_EQ(h.codes[2], L"VHPAS");
        }
        CHECK_EQ(h.trailing, L", ");
    }
    CHECK_EQ(d.text(), texto);
}

// Horário sem códigos (o "Mapa - Copia.txt" real é assim: "00:00 ").
void testHorarioSemCodigos()
{
    const std::wstring texto = L"00:00 \r\n00:30 \r\n";
    BlockDocument d;
    d.setText(texto);

    CHECK_MSG(countHorarios(d) == 2, "2 horarios sem codigos");
    const int idx = lineOfTime(d, L"00:00");
    CHECK_MSG(idx >= 0 && d.lines()[static_cast<size_t>(idx)].horario.codes.empty(),
              "horario 00:00 sem codigos");
    CHECK_EQ(d.text(), texto);
}

// Linha com horário e sem espaço nenhum depois ("00:00").
void testHorarioSemSeparador()
{
    const std::wstring texto = L"00:00\r\n";
    BlockDocument d;
    d.setText(texto);
    CHECK_MSG(countHorarios(d) == 1, "horario sem separador e reconhecido");
    const int idx = lineOfTime(d, L"00:00");
    if (idx >= 0) {
        CHECK_EQ(d.lines()[static_cast<size_t>(idx)].horario.sep, L"");
        CHECK_EQ(d.lines()[static_cast<size_t>(idx)].horario.trailing, L"");
    }
    CHECK_EQ(d.text(), texto);
}

// Linhas cruas (comentário, em branco, seção) são preservadas verbatim.
void testLinhasCruasPreservadas()
{
    const std::wstring texto =
        L"; comentario do mapa\r\n"
        L"\r\n"
        L"[SECAO]\r\n"
        L"00:00 COMER, \r\n"
        L"nao e horario\r\n";

    BlockDocument d;
    d.setText(texto);

    CHECK_MSG(countHorarios(d) == 1, "apenas a linha de horario e estruturada");
    CHECK_MSG(d.lines().size() == 5, "as 5 linhas do arquivo estao no modelo");
    // Round-trip preserva as linhas desconhecidas exatamente como estavam.
    CHECK_EQ(d.text(), texto);
}

// Adicionar código mantém o padrão do arquivo (vírgula + espaço ao final).
// NÃO existe limite de um código por horário: o mesmo código pode ser
// repetido, como no Mapa real (COMER cinco vezes na primeira linha).
void testAdicionarCodigo()
{
    const std::wstring texto = L"00:27 VHAB, COMER, \r\n00:57 VHAB, \r\n";
    BlockDocument d;
    d.setText(texto);

    const int idx = lineOfTime(d, L"00:27");
    CHECK_MSG(idx >= 0, "linha 00:27 encontrada");
    CHECK_MSG(d.addCode(idx, L"INT"), "codigo INT adicionado");
    CHECK_MSG(d.addCode(idx, L"INT"), "mesmo codigo pode entrar de novo");
    CHECK_MSG(!d.addCode(idx, L"  "), "codigo vazio e recusado");
    CHECK_MSG(!d.addCode(idx, L""), "codigo ausente e recusado");
    CHECK_MSG(codeCount(d, L"00:27") == 4, "4 codigos em 00:27");
    CHECK_MSG(hasCode(d, L"00:27", L"INT"), "INT presente");
    CHECK_MSG(occurrences(d, L"00:27", L"INT") == 2, "INT aparece 2 vezes");

    // O arquivo continua terminando a linha com ", ".
    CHECK_EQ(d.text(), L"00:27 VHAB, COMER, INT, INT, \r\n00:57 VHAB, \r\n");
}

// O formato real repete códigos no mesmo horário; o editor tem de permitir
// reproduzir isso quantas vezes o usuário quiser.
void testCodigoRepetidoNoMesmoHorario()
{
    const std::wstring real =
        L"00:27 VHAB, COMER, COMER, COMER, COMER, COMER, VHAB, \r\n";
    BlockDocument d;
    d.setText(real);
    const int idx = lineOfTime(d, L"00:27");
    CHECK_MSG(idx >= 0, "linha real encontrada");
    CHECK_MSG(occurrences(d, L"00:27", L"COMER") == 5, "COMER cinco vezes no arquivo");
    CHECK_MSG(d.addCode(idx, L"COMER"), "mais um COMER aceito");
    CHECK_MSG(occurrences(d, L"00:27", L"COMER") == 6, "COMER seis vezes");
    CHECK_EQ(d.text(),
             L"00:27 VHAB, COMER, COMER, COMER, COMER, COMER, VHAB, COMER, \r\n");

    // E a remoção continua valendo por ocorrência (não apaga todas).
    // A linha real tem 7 códigos (VHAB + 5xCOMER + VHAB); com o extra são 8.
    d.removeCode(idx, 2);
    CHECK_MSG(occurrences(d, L"00:27", L"COMER") == 5, "remove so uma ocorrencia");
    CHECK_MSG(codeCount(d, L"00:27") == 7, "7 codigos restantes");
}

void testAdicionarCodigoSemSeparador()
{
    BlockDocument d;
    d.setText(L"00:00\r\n");
    const int idx = lineOfTime(d, L"00:00");
    CHECK_MSG(idx >= 0, "horario criado/encontrado");
    CHECK_MSG(d.addCode(idx, L"COMER"), "primeiro codigo adicionado");
    // Sem separador no arquivo, o editor usa o padrão do Playlist (" ").
    CHECK_EQ(d.text(), L"00:00 COMER\r\n");
}

void testRemoverCodigo()
{
    const std::wstring texto = L"00:00 INT, NAC, VHPAS, \r\n";
    BlockDocument d;
    d.setText(texto);
    const int idx = lineOfTime(d, L"00:00");
    CHECK_MSG(idx >= 0, "linha encontrada");
    d.removeCode(idx, 1); // remove NAC
    CHECK_MSG(!hasCode(d, L"00:00", L"NAC"), "NAC removido");
    CHECK_MSG(hasCode(d, L"00:00", L"INT"), "INT mantido");
    CHECK_MSG(hasCode(d, L"00:00", L"VHPAS"), "VHPAS mantido");
    CHECK_EQ(d.text(), L"00:00 INT, VHPAS, \r\n");

    // Removendo o último, o horário fica só com o separador.
    d.removeCode(idx, 0);
    d.removeCode(idx, 0);
    CHECK_MSG(codeCount(d, L"00:00") == 0, "sem codigos");
    CHECK_EQ(d.text(), L"00:00 \r\n");
}

void testReplaceCodes()
{
    const std::wstring texto = L"00:00 INT, \r\n";
    BlockDocument d;
    d.setText(texto);
    const int idx = lineOfTime(d, L"00:00");
    // A lista é copiada VERBATIM, com repetições (copiar/colar de uma linha do
    // Mapa real precisa trazer o COMER cinco vezes).
    d.replaceCodes(idx, { L"COMER", L"NAC", L"COMER" });
    CHECK_MSG(codeCount(d, L"00:00") == 3, "copia mantem os repetidos");
    CHECK_MSG(occurrences(d, L"00:00", L"COMER") == 2, "COMER copiado duas vezes");
    CHECK_MSG(hasCode(d, L"00:00", L"NAC"), "NAC copiado");
    CHECK_EQ(d.text(), L"00:00 COMER, NAC, COMER, \r\n");

    // Linha real do Mapa colada em outro horário: a vírgula final e as repetições
    // sobrevivem.
    d.replaceCodes(idx, { L"VHAB", L"COMER", L"COMER", L"COMER", L"COMER",
                          L"COMER", L"VHAB" });
    CHECK_MSG(occurrences(d, L"00:00", L"COMER") == 5, "5 COMER");
    CHECK_MSG(occurrences(d, L"00:00", L"VHAB") == 2, "2 VHAB");
    CHECK_EQ(d.text(), L"00:00 VHAB, COMER, COMER, COMER, COMER, COMER, VHAB, \r\n");
}

void testAdicionarHorarioAvulso()
{
    const std::wstring texto = L"00:00 COMER, \r\n00:30 COMER, \r\n";
    BlockDocument d;
    d.setText(texto);

    const int idx = d.addTime(L"00:15");
    CHECK_MSG(idx >= 0, "horario avulso 00:15 inserido");
    CHECK_EQ(d.lines()[static_cast<size_t>(idx)].horario.time, L"00:15");
    // Fica em ordem cronológica.
    CHECK_EQ(d.text(), L"00:00 COMER, \r\n00:15 \r\n00:30 COMER, \r\n");

    CHECK_MSG(d.addTime(L"00:15") < 0, "horario duplicado e recusado");
    CHECK_MSG(d.addTime(L"25:99") < 0, "horario invalido e recusado");
}

void testPreencherHorarios()
{
    const std::wstring texto = L"; comentario\r\n00:00 INT, \r\n00:30 INT, \r\n";
    BlockDocument d;
    d.setText(texto);

    d.reschedule({ L"02:00", L"00:00", L"00:30", L"00:00", L"invalido" });
    // Linhas cruas preservadas, horários substituídos em ordem.
    CHECK_EQ(d.text(), L"; comentario\r\n00:00 \r\n00:30 \r\n02:00 \r\n");
}

void testRemoverLinha()
{
    const std::wstring texto = L"00:00 COMER, \r\n00:30 NAC, \r\n";
    BlockDocument d;
    d.setText(texto);
    const int idx = lineOfTime(d, L"00:30");
    CHECK_MSG(idx >= 0, "linha 00:30 encontrada");
    CHECK_MSG(d.removeLine(idx), "linha removida");
    CHECK_EQ(d.text(), L"00:00 COMER, \r\n");
}

void testReplaceFrom()
{
    const std::wstring a = L"00:00 COMER, \r\n";
    const std::wstring b = L"01:00 NAC, VHPAS, \r\n";
    BlockDocument da;
    da.setText(a);
    BlockDocument db;
    db.setText(b);

    da.replaceFrom(db);
    CHECK_EQ(da.text(), b);
}

void testUtilidadesTempo()
{
    CHECK_MSG(BlockDocument::parseTime(L"00:00") == 0, "00:00 = 0");
    CHECK_MSG(BlockDocument::parseTime(L"23:59") == 1439, "23:59 = 1439");
    CHECK_MSG(BlockDocument::parseTime(L"12:30") == 750, "12:30 = 750");
    CHECK_MSG(BlockDocument::parseTime(L"24:00") < 0, "24:00 invalido");
    CHECK_MSG(BlockDocument::parseTime(L"9:30") < 0, "9:30 invalido (sem zero)");
    CHECK_MSG(BlockDocument::parseTime(L"") < 0, "vazio invalido");
    CHECK_EQ(BlockDocument::formatTime(750), L"12:30");
    CHECK_EQ(BlockDocument::formatTime(0), L"00:00");
    CHECK_EQ(BlockDocument::formatTime(1439), L"23:59");
}

// Fim de linha: LF puro também é preservado (arquivos gerados em Linux).
void testFimDeLinhaLf()
{
    const std::wstring texto = L"00:00 COMER, \n00:30 COMER, \n";
    BlockDocument d;
    d.setText(texto);
    CHECK_EQ(d.text(), texto);
}

// Arquivo sem vírgula final (estilo alternativo permitido pelo manual).
void testSemVirgulaFinal()
{
    const std::wstring texto = L"06:00 VH, 55, 23, VH, 62, 12, VH, 42, VHC, HC\r\n";
    BlockDocument d;
    d.setText(texto);
    const int idx = lineOfTime(d, L"06:00");
    CHECK_MSG(idx >= 0 && d.lines()[static_cast<size_t>(idx)].horario.codes.size() == 10,
              "10 codigos no exemplo oficial do manual");
    CHECK_EQ(d.text(), texto);
}

// ============================================================================
// Lista de Códigos: vem do folders.xml (somente leitura). Códigos novos são de
// SESSÃO — o arquivo original nunca é gravado.
// ============================================================================

std::filesystem::path writeTemp(const std::wstring& name,
                                const std::wstring& content)
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / L"progmaster_blocos_tests";
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / name;
    std::ofstream out(path, std::ios::binary);
    for (const wchar_t c : content) {
        if (c < 128) {
            out.put(static_cast<char>(c));
        }
    }
    out.close();
    return path;
}

// Folders.xml com a estrutura real: <Shared> (pastas) + <Folder0..N> (códigos).
const wchar_t* kFoldersXml =
    L"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
    L"<Folders>\r\n"
    L"  <Folders>3</Folders>\r\n"
    L"  <Shared>\r\n"
    L"    <Folders>1</Folders>\r\n"
    L"    <Server>Shared</Server>\r\n"
    L"  </Shared>\r\n"
    L"  <Folder0>\r\n"
    L"    <DBFId>COMER</DBFId>\r\n"
    L"    <Title>Comercial</Title>\r\n"
    L"  </Folder0>\r\n"
    L"  <Folder1>\r\n"
    L"    <DBFId>INT</DBFId>\r\n"
    L"    <Title>Internacional</Title>\r\n"
    L"  </Folder1>\r\n"
    L"  <Folder2>\r\n"
    L"    <DBFId>VHAB</DBFId>\r\n"
    L"    <Title>Vinheta Habitacional</Title>\r\n"
    L"  </Folder2>\r\n"
    L"  <Folder3>\r\n"
    L"    <DBFId>COMER</DBFId>\r\n"
    L"    <Title>Repetido</Title>\r\n"
    L"  </Folder3>\r\n"
    L"  <Other>x</Other>\r\n"
    L"</Folders>\r\n";

// Conteúdo bruto de um arquivo (para conferir que NADA foi gravado nele).
std::string readBytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
}

void testCatalogoDoArquivo()
{
    const std::filesystem::path path = writeTemp(L"FoldersCatalogo.xml", kFoldersXml);
    CodeCatalogue cat;
    std::string err;
    const std::wstring msg = cat.loadFromFoldersXml(path, err);

    CHECK_MSG(msg.empty(), "catalogo carregado sem erro");
    CHECK_MSG(cat.size() == 3, "3 codigos: COMER, INT, VHAB (COMER repetido eliminado)");
    CHECK_MSG(cat.indexOf(L"COMER") >= 0, "COMER no catalogo");
    CHECK_MSG(cat.indexOf(L"INT") >= 0, "INT no catalogo");
    CHECK_MSG(cat.indexOf(L"VHAB") >= 0, "VHAB no catalogo");
    CHECK_MSG(!cat.contains(L"SEGREDO"), "codigo inexistente ausente");
    CHECK_MSG(!cat.entries()[0].sessionOnly, "codigo do arquivo nao e de sessao");
}

void testCatalogoCodigoDeSessao()
{
    const std::filesystem::path path = writeTemp(L"FoldersSessao.xml", kFoldersXml);
    CodeCatalogue cat;
    std::string err;
    CHECK_MSG(cat.loadFromFoldersXml(path, err).empty(), "catalogo carregado");
    const size_t base = cat.size();
    // O conteúdo do arquivo original não muda.
    const std::string before = readBytes(path);

    std::wstring why;
    CHECK_MSG(cat.addSessionCode(L"MEU1", L"Meu Codigo", why), "codigo de sessao criado");
    CHECK_MSG(cat.size() == base + 1, "codigo de sessao entrou no catalogo");
    CHECK_MSG(cat.contains(L"MEU1"), "MEU1 disponivel");
    CHECK_MSG(!cat.addSessionCode(L"MEU1", L"Repetido", why), "codigo repetido recusado");
    CHECK_MSG(!cat.addSessionCode(L"comer", L"Repetido", why), "repetido sem diferenciar caixa");
    CHECK_MSG(!cat.addSessionCode(L"COM ER", L"Com espaco", why), "espaco no codigo recusado");
    CHECK_MSG(!cat.addSessionCode(L"", L"Vazio", why), "codigo vazio recusado");
    CHECK_MSG(!cat.addSessionCode(L"12345678901234567", L"Longo", why), "codigo longo recusado");

    // Códigos do arquivo não podem ser removidos pelo editor.
    CHECK_MSG(!cat.removeSessionCode(L"COMER"), "codigo do arquivo nao e removido");
    CHECK_MSG(cat.contains(L"COMER"), "COMER continua no catalogo");
    CHECK_MSG(cat.removeSessionCode(L"MEU1"), "codigo de sessao removido");
    CHECK_MSG(!cat.contains(L"MEU1"), "MEU1 saiu do catalogo");

    // O folders.xml deve estar byte a byte igual: nada foi gravado.
    CHECK_MSG(readBytes(path) == before,
              "folders.xml NAO foi modificado (somente leitura)");
}

void testCatalogoRecarregaMantemSessao()
{
    const std::filesystem::path path = writeTemp(L"FoldersReload.xml", kFoldersXml);
    CodeCatalogue cat;
    std::string err;
    CHECK_MSG(cat.loadFromFoldersXml(path, err).empty(), "primeiro carregamento ok");
    std::wstring why;
    CHECK_MSG(cat.addSessionCode(L"TEMP", L"Temporario", why), "codigo de sessao criado");

    CHECK_MSG(cat.loadFromFoldersXml(path, err).empty(), "recarregamento ok");
    CHECK_MSG(cat.contains(L"TEMP"), "codigo de sessao sobrevive ao recarregar");
    CHECK_MSG(cat.contains(L"COMER"), "codigo do arquivo continua");
}

// ============================================================================
// Códigos que o ARQUIVO usa mas o folders.xml não lista (o SOR real do
// "GRADE - Copia.txt"). Eles entram na Lista de Códigos para ganhar cor,
// botão na paleta e entrada no combo — sem alterar o folders.xml.
// ============================================================================

void testDistinctCodesDoArquivo()
{
    // Linha real do "GRADE - Copia.txt" da instalação oficial: SOR repetido.
    const std::wstring real =
        L"00:00 VHAB, SOR, SOR, VHPAS, SOR, SOR, VHPAS, SOR, SOR, VHPAS, SOR, SOR, VHAB\r\n";
    BlockDocument d;
    d.setText(real);
    const std::vector<std::wstring> codes = d.distinctCodes();
    CHECK_MSG(codes.size() == 3, "tres codigos distintos (repeticao conta uma vez)");
    CHECK_MSG(codes[0] == L"VHAB", "primeiro na ordem de aparicao");
    CHECK_MSG(codes[1] == L"SOR", "SOR logo depois de VHAB");
    CHECK_MSG(codes[2] == L"VHPAS", "VHPAS por ultimo");
}

void testCatalogoAdotaCodigoDoArquivo()
{
    const std::filesystem::path path = writeTemp(L"FoldersAdota.xml", kFoldersXml);
    CodeCatalogue cat;
    std::string err;
    CHECK_MSG(cat.loadFromFoldersXml(path, err).empty(), "catalogo carregado");
    const size_t base = cat.size();
    const std::string before = readBytes(path);

    // SOR nao esta no folders.xml; COMER ja esta; vazio e espaco sao ignorados.
    const int added =
        cat.adoptCodesFromFile({ L"SOR", L"COMER", L"sor", L"", L"  " });
    CHECK_MSG(added == 1, "so o SOR entrou");
    CHECK_MSG(cat.size() == base + 1, "a Lista cresceu um codigo");
    CHECK_MSG(cat.contains(L"SOR"), "SOR agora esta na Lista de Codigos");
    CHECK_MSG(cat.indexOf(L"SOR") >= 0, "SOR tem indice — logo tem cor (nao cinza)");

    const int i = cat.indexOf(L"SOR");
    CHECK_MSG(cat.entries()[static_cast<size_t>(i)].sessionOnly,
              "codigo adotado do arquivo e de sessao (borda tracejada)");
    CHECK_MSG(cat.entries()[static_cast<size_t>(i)].origin == CodeOrigin::FromBlockFile,
              "origem marcada como FromBlockFile");
    CHECK_MSG(cat.entries()[0].origin == CodeOrigin::FoldersXml,
              "codigos do folders.xml continuam FoldersXml");

    CHECK_MSG(cat.adoptCodesFromFile({ L"SOR" }) == 0, "adotar de novo nao duplica");
    CHECK_MSG(cat.size() == base + 1, "tamanho estavel apos segunda adocao");

    // Adotado nao vem do folders.xml, entao a lixeira pode remove-lo da Lista.
    CHECK_MSG(cat.removeSessionCode(L"SOR"), "codigo adotado pode ser removido da Lista");
    CHECK_MSG(!cat.contains(L"SOR"), "SOR saiu da Lista");
    CHECK_MSG(!cat.removeSessionCode(L"COMER"), "COMER (do folders.xml) continua intocavel");

    CHECK_MSG(readBytes(path) == before,
              "folders.xml NAO foi modificado ao adotar codigo");
}

void testCodigoAdotadoSobreviveRecarga()
{
    const std::filesystem::path path = writeTemp(L"FoldersAdotaReload.xml", kFoldersXml);
    CodeCatalogue cat;
    std::string err;
    CHECK_MSG(cat.loadFromFoldersXml(path, err).empty(), "carregado");
    CHECK_MSG(cat.adoptCodesFromFile({ L"SOR" }) == 1, "SOR adotado");
    CHECK_MSG(cat.loadFromFoldersXml(path, err).empty(), "recarregado do folders.xml");
    CHECK_MSG(cat.contains(L"SOR"), "codigo adotado sobrevive ao recarregar");
    CHECK_MSG(cat.contains(L"COMER"), "codigo do arquivo continua");
}

} // namespace

int main()
{
    std::cout << "ProgMaster Blocos Tests (Etapa 4)\n\n";

    testParseMapaReal();
    testParseGradeReal();
    testHorarioSemCodigos();
    testHorarioSemSeparador();
    testLinhasCruasPreservadas();
    testAdicionarCodigo();
    testCodigoRepetidoNoMesmoHorario();
    testAdicionarCodigoSemSeparador();
    testRemoverCodigo();
    testReplaceCodes();
    testAdicionarHorarioAvulso();
    testPreencherHorarios();
    testRemoverLinha();
    testReplaceFrom();
    testUtilidadesTempo();
    testFimDeLinhaLf();
    testSemVirgulaFinal();
    testCatalogoDoArquivo();
    testCatalogoCodigoDeSessao();
    testCatalogoRecarregaMantemSessao();
    testDistinctCodesDoArquivo();
    testCatalogoAdotaCodigoDoArquivo();
    testCodigoAdotadoSobreviveRecarga();

    std::cout << g_checks << "/" << g_checks << " ok"
              << (g_failures == 0 ? "" : " (com falhas)") << "\n";
    if (g_failures == 0) {
        std::cout << "TODOS OS TESTES PASSARAM\n";
        return 0;
    }
    std::cout << g_failures << " FALHA(S)\n";
    return 1;
}
