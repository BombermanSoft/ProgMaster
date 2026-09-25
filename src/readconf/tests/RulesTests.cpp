#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "core/TextFileIO.h"
#include "readconf/FormatRules.h"
#include "readconf/PlaylistFileLocator.h"
#include "readconf/PlaylistIniDocument.h"
#include "readconf/ReadingConfiguration.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what, const char* file, int line)
{
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cout << "  FALHOU " << file << ":" << line << " - " << what
                  << "\n";
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
                if (c >= 32 && c < 127) {
                    r += static_cast<char>(c);
                }
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

using namespace readconf;

void testSectionNames()
{
    CHECK_EQ(sectionNameFor(ConfigScope::Comercial), L"BLOCO COMERCIAL");
    CHECK_EQ(sectionNameFor(ConfigScope::Musical), L"BLOCO MUSICAL");
    CHECK_EQ(sectionNameFor(ConfigScope::RelogioComercial), L"RELOGIO COMERCIAL");
    CHECK_EQ(sectionNameFor(ConfigScope::RelogioMusical), L"RELOGIO MUSICAL");
    CHECK_EQ(sectionNameFor(ConfigScope::Afiliadas), L"AFILIADAS");
    CHECK_EQ(scopeDisplayName(ConfigScope::Comercial), L"Bloco Comercial");
}

void testOptionsForScope()
{
    const auto c = optionsForFormat(ConfigScope::Comercial);
    CHECK_MSG(c.size() == 5, "comercial tem 5 opÃ§Ãµes");
    const auto m = optionsForFormat(ConfigScope::Musical);
    CHECK_MSG(m.size() == 4, "musical tem 4 opÃ§Ãµes");
    const auto r = optionsForFormat(ConfigScope::RelogioComercial);
    CHECK_MSG(r.size() == 2, "relÃ³gio tem 2 opÃ§Ãµes (Ãšnico/Semanal)");
    const auto a = optionsForFormat(ConfigScope::Afiliadas);
    CHECK_MSG(a.empty(), "afiliadas sem opÃ§Ãµes de formato");

    CHECK_MSG(optionAppliesTo(ConfigScope::Comercial, FormatOption::Maker) == false,
              "Maker nÃ£o pertence ao Comercial");
    CHECK_MSG(optionAppliesTo(ConfigScope::Musical, FormatOption::CommercialDay) == false,
              "Commercial Dia nÃ£o pertence ao Musical");
    CHECK_MSG(optionAppliesTo(ConfigScope::Comercial, FormatOption::CommercialDay),
              "Commercial Dia pertence ao Comercial");
}

// helpes local: gera e confere linhas
void expectLines(ConfigScope scope, FormatOption option, const std::wstring& l1,
                 const std::wstring& l2, int fromLine)
{
    const GeneratedFormat g = generate(scope, option);
    const std::vector<std::wstring> lines = g.lines;
    bool ok = true;
    if (l2.empty()) {
        ok = (lines.size() == 1);
    } else {
        ok = (lines.size() == 2);
    }
    if (!ok) {
        ++g_checks;
        ++g_failures;
        std::cout << "  FALHOU (tamanho de linhas) linha " << fromLine << "\n";
        return;
    }
    if (!l1.empty()) CHECK_EQ(lines[0], l1);
    if (!l2.empty()) CHECK_EQ(lines[1], l2);
}

void testGenerateRules()
{
    expectLines(ConfigScope::Comercial, FormatOption::Auto, L"FORMATO=AUTO", L"", __LINE__);
    expectLines(ConfigScope::Comercial, FormatOption::Single, L"FORMATO=TXT1", L"ARQUIVO=MAPAS\\Mapa.txt", __LINE__);
    expectLines(ConfigScope::Comercial, FormatOption::Weekly, L"FORMATO=TXT1", L"ARQUIVO=MAPAS\\Mapa%w.txt", __LINE__);
    expectLines(ConfigScope::Comercial, FormatOption::CommercialDay, L"FORMATO=TXT1", L"ARQUIVO=MAPAS\\Mapa%d", __LINE__);
    expectLines(ConfigScope::Comercial, FormatOption::CommercialDate, L"FORMATO=TXT1", L"ARQUIVO=MAPAS\\Mapa%d-%m%Y", __LINE__);
    expectLines(ConfigScope::Musical, FormatOption::Auto, L"FORMATO=AUTO", L"", __LINE__);
    expectLines(ConfigScope::Musical, FormatOption::Single, L"FORMATO=TXT1", L"ARQUIVO=grades\\Grade.txt", __LINE__);
    expectLines(ConfigScope::Musical, FormatOption::Weekly, L"FORMATO=TXT1", L"ARQUIVO=grades\\Grade%w.txt", __LINE__);
    expectLines(ConfigScope::Musical, FormatOption::Maker, L"FORMATO=TXT1", L"ARQUIVO=grades\\%d-%m-%y", __LINE__);
    expectLines(ConfigScope::RelogioComercial, FormatOption::Single, L"FORMATO=TXT1", L"ARQUIVO=Mapas\\Relogio.txt", __LINE__);
    expectLines(ConfigScope::RelogioComercial, FormatOption::Weekly, L"FORMATO=TXT1", L"ARQUIVO=Mapas\\Relogio%a.txt", __LINE__);
    expectLines(ConfigScope::RelogioMusical, FormatOption::Single, L"FORMATO=TXT1", L"ARQUIVO=Mapas\\Relogio.txt", __LINE__);
    expectLines(ConfigScope::RelogioMusical, FormatOption::Weekly, L"FORMATO=TXT1", L"ARQUIVO=Mapas\\Relogio%a.txt", __LINE__);

    // OpÃ§Ãµes fora do escopo: geraÃ§Ã£o vazia.
    CHECK_MSG(generate(ConfigScope::Musical, FormatOption::CommercialDay).lines.empty(),
              "CommercialDay nÃ£o gera para Musical");
    CHECK_MSG(generate(ConfigScope::Comercial, FormatOption::Maker).lines.empty(),
              "Maker nÃ£o gera para Comercial");
}

void testInterpret()
{
    const auto checkMatch = [](ConfigScope scope, const std::wstring& fmt,
                               const std::wstring& arq, FormatOption expect,
                               const wchar_t* where) {
        const FormatMatch m = interpret(scope, fmt, arq);
        ++g_checks;
        if (m.option != expect) {
            ++g_failures;
            std::cout << "  FALHOU interpret " << where << ": esperado "
                      << static_cast<int>(expect) << " obtido "
                      << static_cast<int>(m.option) << "\n";
        }
    };

    checkMatch(ConfigScope::Comercial, L"AUTO", L"", FormatOption::Auto, L"AUTO c");
    checkMatch(ConfigScope::Musical, L"AUTO", L"", FormatOption::Auto, L"AUTO m");
    checkMatch(ConfigScope::Comercial, L"TXT1", L"MAPAS\\Mapa.txt", FormatOption::Single, L"single c");
    checkMatch(ConfigScope::Comercial, L"TXT1", L"MAPAS\\Mapa%w.txt", FormatOption::Weekly, L"weekly %w c");
    checkMatch(ConfigScope::Comercial, L"TXT1", L"mapas\\mapa%a.txt", FormatOption::Weekly, L"weekly %a c");
    checkMatch(ConfigScope::Comercial, L"TXT1", L"MAPAS\\MapaSeg.txt", FormatOption::Weekly, L"weekly seg c");
    checkMatch(ConfigScope::Comercial, L"TXT1", L"MAPAS\\Mapa%d", FormatOption::CommercialDay, L"day c");
    checkMatch(ConfigScope::Comercial, L"TXT1", L"MAPAS\\Mapa%d-%m%Y", FormatOption::CommercialDate, L"date c");
    checkMatch(ConfigScope::Musical, L"TXT1", L"grades\\Grade.txt", FormatOption::Single, L"single m");
    checkMatch(ConfigScope::Musical, L"TXT1", L"grades\\Grade%w.txt", FormatOption::Weekly, L"weekly m");
    checkMatch(ConfigScope::Musical, L"TXT1", L"grades\\%d-%m-%y", FormatOption::Maker, L"maker m");
    checkMatch(ConfigScope::RelogioComercial, L"TXT1", L"Mapas\\Relogio.txt", FormatOption::Single, L"relogio single");
    checkMatch(ConfigScope::RelogioComercial, L"TXT1", L"Mapas\\Relogio%a.txt", FormatOption::Weekly, L"relogio weekly");
    checkMatch(ConfigScope::RelogioMusical, L"TXT1", L"Mapas\\RelogioSeg.txt", FormatOption::Weekly, L"relogio seg");
    checkMatch(ConfigScope::RelogioComercial, L"AUTO", L"", FormatOption::Unknown, L"relogio auto desconhecido");

    // Valores fora do escopo -> Unknown.
    checkMatch(ConfigScope::Musical, L"TXT1", L"MAPAS\\Mapa%d", FormatOption::Unknown, L"day fora do musical");
    checkMatch(ConfigScope::Comercial, L"TXT1", L"grades\\Grade.txt", FormatOption::Unknown, L"single errado no comercial");

    // SeÃ§Ã£o sem as chaves -> Unknown.
    checkMatch(ConfigScope::Comercial, L"", L"", FormatOption::Unknown, L"sem chaves");

    // Token reconhecido.
    const FormatMatch w = interpret(ConfigScope::Comercial, L"TXT1", L"MAPAS\\Mapa%w.txt");
    CHECK_EQ(w.matchedToken, L"%w");
}

void testDocumentParseSerialize()
{
    const std::wstring ini =
        L";ConfiguraÃ§Ã£o do Playlist\r\n\r\n"
        L"[BLOCO COMERCIAL]\r\n"
        L"FORMATO=AUTO\r\n"
        L"; comentÃ¡rio no meio\r\n"
        L"[BLOCO MUSICAL]\r\n"
        L"FORMATO=TXT1\r\n"
        L"ARQUIVO=grades\\Grade%w.txt\r\n"
        L"[AFILIADAS]\r\n"
        L"AFILIADA=192.168.0.3:3030\r\n"
        L";AFILIADA=192.168.0.11:9090\r\n"
        L"[RELOGIO COMERCIAL]\r\n"
        L"FORMATO=TXT1\r\n"
        L"ARQUIVO=Mapas\\Relogio%a.txt\r\n";

    PlaylistIniDocument doc;
    doc.setText(ini);
    CHECK_EQ(doc.text(), ini); // round-trip idÃªntico (preservaÃ§Ã£o)

    // Leituras estruturais.
    std::wstring fmt;
    CHECK_MSG(doc.sectionKeyValue(ConfigScope::Comercial, L"formato", fmt),
              "formato comercial presente");
    CHECK_EQ(fmt, L"AUTO");
    CHECK_MSG(doc.sectionKeyValue(ConfigScope::Musical, L"arquivo", fmt),
              "arquivo musical presente");
    CHECK_EQ(fmt, L"grades\\Grade%w.txt");
    CHECK_MSG(doc.sectionKeyValue(ConfigScope::RelogioComercial, L"formato", fmt),
              "formato relÃ³gio presente");

    // Afiliadas (2, uma ativa outra desativada).
    const auto afs = doc.afiliadas();
    CHECK_MSG(afs.size() == 2, "nº de afiliadas = 2");
    CHECK_MSG(!afs[0].disabled, "afiliada 0 ativa");
    CHECK_EQ(afs[0].address, L"192.168.0.3");
    CHECK_EQ(afs[0].portText, L"3030");
    CHECK_MSG(afs[1].disabled, "afiliada 1 desativada (linha ;)");
    CHECK_EQ(afs[1].portText, L"9090");
}

void testDocumentApplyFormat()
{
    const std::wstring ini =
        L";cabeÃ§alho\r\n"
        L"[BLOCO COMERCIAL]\r\n"
        L"FORMATO=AUTO\r\n"
        L"[DESCONHECIDA]\r\n"
        L"CHAVE_EXTRA=valor1\r\n"
        L"[BLOCO MUSICAL]\r\n"
        L"FORMATO=TXT1\r\n"
        L"ARQUIVO=grades\\Grade%w.txt\r\n";

    PlaylistIniDocument doc;
    doc.setText(ini);

    // Muda Comercial para Semanal -> sÃ³ FORMATO/ARQUIVO do comercial mudam.
    CHECK_MSG(doc.applyFormat(ConfigScope::Comercial, FormatOption::Weekly),
              "applyFormat weekly comercial ok");
    std::wstring fmt;
    CHECK_MSG(doc.sectionKeyValue(ConfigScope::Comercial, L"arquivo", fmt),
              "arquivo atualizado");
    CHECK_EQ(fmt, L"MAPAS\\Mapa%w.txt");

    // SeÃ§Ã£o desconhecida preservada na ordem e com o valor.
    const std::wstring out = doc.text();
    CHECK_MSG(out.find(L"[DESCONHECIDA]") != std::wstring::npos,
              "seÃ§Ã£o desconhecida preservada");
    CHECK_MSG(out.find(L"CHAVE_EXTRA=valor1") != std::wstring::npos,
              "chave desconhecida preservada");
    CHECK_MSG(out.find(L";cabeÃ§alho") != std::wstring::npos,
              "comentÃ¡rio de topo preservado");

    // AUTO remove o ARQUIVO.
    CHECK_MSG(doc.applyFormat(ConfigScope::Comercial, FormatOption::Auto),
              "applyFormat auto ok");
    CHECK_MSG(!doc.sectionKeyValue(ConfigScope::Comercial, L"arquivo", fmt),
              "auto remove arquivo");
    std::wstring formato;
    CHECK_MSG(doc.sectionKeyValue(ConfigScope::Comercial, L"formato", formato),
              "formato continua");
    CHECK_EQ(formato, L"AUTO");
}

void testDocumentCreateSection()
{
    PlaylistIniDocument doc;
    doc.setText(L";vazio\r\n");
    CHECK_MSG(doc.applyFormat(ConfigScope::Musical, FormatOption::Maker),
              "cria seÃ§Ã£o musical");
    std::wstring arq;
    CHECK_MSG(doc.sectionKeyValue(ConfigScope::Musical, L"arquivo", arq),
              "arquivo da seÃ§Ã£o criada");
    CHECK_EQ(arq, L"grades\\%d-%m-%y");
}

void testDocumentAfiliadasEdit()
{
    PlaylistIniDocument doc;
    doc.setText(
        L"[AFILIADAS]\r\n"
        L"AFILIADA=192.168.0.3:3030\r\n"
        L";AFILIADA=192.168.0.11:9090\r\n");

    // Atualiza a desativada para ativa, com outra porta.
    doc.updateAfiliada(1, L"192.168.0.12", L"8080", /*disabled=*/false);
    const auto afs = doc.afiliadas();
    CHECK_MSG(afs.size() == 2, "nº de afiliadas = 2");
    CHECK_MSG(!afs[1].disabled, "afiliada 1 agora ativa");
    CHECK_EQ(afs[1].address, L"192.168.0.12");
    CHECK_EQ(afs[1].portText, L"8080");

    // Adiciona uma terceira (desativada).
    doc.addAfiliada(L"10.0.0.9", L"1", true);
    const auto afs2 = doc.afiliadas();
    CHECK_MSG(afs2.size() == 3, "nº de afiliadas = 3");
    CHECK_MSG(afs2[2].disabled, "nova afiliada desativada");
    CHECK_EQ(afs2[2].address, L"10.0.0.9");

    // Remove a primeira.
    doc.removeAfiliada(0);
    const auto afs3 = doc.afiliadas();
    CHECK_MSG(afs3.size() == 2, "nº de afiliadas = 2");
    CHECK_EQ(afs3[0].address, L"192.168.0.12");
}

void testDocumentEolPreservation()
{
    PlaylistIniDocument doc;
    const std::wstring lfText = L"[BLOCO COMERCIAL]\nFORMATO=AUTO\n";
    doc.setText(lfText);
    CHECK_EQ(doc.text(), lfText);

    PlaylistIniDocument docCr;
    const std::wstring crText = L"[BLOCO COMERCIAL]\r\nFORMATO=AUTO";
    docCr.setText(crText);
    CHECK_EQ(docCr.text(), crText);
}

void testDocumentTolerances()
{
    // Seção com acento (RELÓGIO -> RELOGIO) e valores entre aspas: o arquivo
    // real do Playlist costuma usar essas formas; o programa deve reconhecer.
    PlaylistIniDocument doc;
    doc.setText(
        L"[BLOCO COMERCIAL]\nFORMATO=AUTO\n"
        L"[REL\u00d3GIO COMERCIAL]\nFORMATO=TXT1\n"
        L"ARQUIVO=\"Mapas\\Relogio.txt\"\n");

    // O round-trip preserva o texto original (inclusive as aspas).
    CHECK_EQ(doc.text(),
             L"[BLOCO COMERCIAL]\nFORMATO=AUTO\n"
             L"[REL\u00d3GIO COMERCIAL]\nFORMATO=TXT1\n"
             L"ARQUIVO=\"Mapas\\Relogio.txt\"\n");

    std::wstring fmt;
    CHECK_MSG(doc.sectionKeyValue(ConfigScope::Comercial, L"formato", fmt),
              "comercial encontrado");
    CHECK_EQ(fmt, L"AUTO");

    // O valor entre aspas é lido SEM as aspas.
    CHECK_MSG(doc.sectionKeyValue(ConfigScope::RelogioComercial, L"arquivo", fmt),
              "relogio com acento encontrado");
    CHECK_EQ(fmt, L"Mapas\\Relogio.txt");

    // A interpretação reconhece a opção dentro da seção acentuada.
    const auto scopes =
        readScopes(doc, std::filesystem::temp_directory_path());
    CHECK_MSG(scopes[2].present &&
                  scopes[2].option == FormatOption::Single,
              "seção acentuada interpretada como Único");
}

void testLocateFiles()
{
    // Cria uma estrutura temporÃ¡ria de instalaÃ§Ã£o.
    const std::filesystem::path base =
        std::filesystem::temp_directory_path() / L"pm_coretest_install";
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base / L"mapas", ec);
    std::filesystem::create_directories(base / L"grades", ec);

    {
        std::ofstream f(base / L"mapas" / L"MapaSeg.txt");
        f << "x";
    }
    {
        std::ofstream f(base / L"mapas" / L"Mapa.txt");
        f << "x";
    }
    {
        std::ofstream f(base / L"grades" / L"23-09-26");
        f << "x";
    }

    const auto weekly = locateFiles(ConfigScope::Comercial, FormatOption::Weekly, base);
    CHECK_MSG(weekly.size() == 7, "semanal = 7 arquivos");
    CHECK_MSG(weekly[0].fileName == L"MapaSeg.txt" && weekly[0].exists,
              "MapaSeg existe no semanal");

    const auto single = locateFiles(ConfigScope::Comercial, FormatOption::Single, base);
    CHECK_MSG(single.size() == 1, "único = 1 arquivo");
    CHECK_MSG(single[0].exists, "Mapa.txt existe no Ãºnico");

    // Maker: varredura de datas dd-mm-aa na pasta de grades.
    const auto maker = locateFiles(ConfigScope::Musical, FormatOption::Maker, base);
    CHECK_MSG(!maker.empty(), "maker tem pelo menos um arquivo");
    bool found0926 = false;
    for (const auto& b : maker) {
        if (b.fileName == L"23-09-26" && b.exists) {
            found0926 = true;
        }
    }
    CHECK_MSG(found0926, "maker encontra 23-09-26");

    std::filesystem::remove_all(base, ec);
}

void testValidation()
{
    PlaylistIniDocument doc;
    doc.setText(L"[AFILIADAS]\r\nAFILIADA=192.168.0.3:3030\r\n");
    const ValidationResult ok = validateForSave(doc);
    CHECK_MSG(ok.ok, "afiliadas vÃ¡lidas passam");

    PlaylistIniDocument doc2;
    doc2.setText(L"[AFILIADAS]\r\nAFILIADA=192.168.0.3:0\r\n");
    CHECK_MSG(!validateForSave(doc2).ok, "porta 0 reprovada");

    PlaylistIniDocument doc3;
    doc3.setText(L"[AFILIADAS]\r\nAFILIADA=192.168.0.3:7070\r\nAFILIADA=192.168.0.3:7070\r\n");
    CHECK_MSG(!validateForSave(doc3).ok, "duplicada reprovada");

    PlaylistIniDocument doc4;
    doc4.setText(L"[AFILIADAS]\r\nAFILIADA=:3030\r\n");
    CHECK_MSG(!validateForSave(doc4).ok, "sem endereÃ§o reprovada");
}

void testReadingScopes()
{
    PlaylistIniDocument doc;
    doc.setText(
        L"[BLOCO COMERCIAL]\r\n"
        L"FORMATO=TXT1\r\n"
        L"ARQUIVO=MAPAS\\Mapa%w.txt\r\n"
        L"[RELOGIO MUSICAL]\r\n"
        L"FORMATO=TXT1\r\n"
        L"ARQUIVO=Mapas\\Relogio%a.txt\r\n");

    const auto scopes = readScopes(doc, std::filesystem::temp_directory_path());
    CHECK_MSG(scopes.size() == 4, "quatro escopos lidos");
    CHECK_MSG(scopes[0].present, "comercial presente");
    CHECK_MSG(scopes[0].option == FormatOption::Weekly, "comercial semanal");
    CHECK_MSG(!scopes[1].present, "musical ausente");
    CHECK_MSG(scopes[3].option == FormatOption::Weekly, "relÃ³gio semanal");
    CHECK_EQ(scopes[3].matchedToken, L"%a");
}

void testTextFileIOAccents()
{
    // Preservação de acentos em todas as codificações suportadas.
    const std::wstring text =
        L"[REL\u00D3GIO COMERCIAL]\r\n"
        L"DESCRI\u00C7\u00C3O=R\u00C1DIO \u00D4NIBUS\r\n"
        L"EMISSORA=\u00C0 VISTA \u2013 R\u00C3O\r\n";

    const struct {
        const char* name;
        TextEncoding enc;
    } kCases[] = {
        { "Utf8",    TextEncoding::Utf8 },
        { "Utf8Bom", TextEncoding::Utf8Bom },
        { "Utf16Le", TextEncoding::Utf16Le },
        { "Ansi",    TextEncoding::Ansi }, // CP_ACP local (CP-1252 no BR)
    };

    for (const auto& c : kCases) {
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() / L"pm_accents_test";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const auto path = dir / (std::string(c.name) + ".ini");

        std::string techErr;
        const bool wrote = TextFileIO::writeWide(path, text, c.enc, techErr);
        const std::string encName = "[" + std::string(c.name) + "]";
        CHECK_MSG(wrote, encName + " writeWide ok");
        if (!wrote) {
            continue;
        }

        const TextFileResult res = TextFileIO::readWide(path);
        CHECK_MSG(res.ok, "readWide ok");
        CHECK_MSG(res.encoding == c.enc, "encoding preservado no read");
        CHECK_EQ(res.text, text);

        const TextFileResult res2 = TextFileIO::readWide(path);
        CHECK_EQ(res2.text, res.text);
        std::filesystem::remove_all(dir, ec);
    }

    // Gravação ANSI recusa caracteres fora da página de código local
    // (ex.: emoji não existe em CP-1252) para não corromper o arquivo.
    if (GetACP() == 1252) {
        const auto path = std::filesystem::temp_directory_path() / L"pm_accent_bad.ini";
        std::string techErr;
        const std::wstring bad = L"BEM\u2013VINDO \u0416\r\n"; // vem-vindo + cirílico (fora de CP1252)
        const bool wrote = TextFileIO::writeWide(path, bad, TextEncoding::Ansi, techErr);
        CHECK_MSG(!wrote, "ANSI recusa emoji (caracteres fora de CP1252)");
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
}

} // namespace

int main()
{
    std::cout << "ProgMaster Core Tests (Etapa 2)\n";

    testSectionNames();
    testOptionsForScope();
    testInterpret();
    testGenerateRules();
    testDocumentParseSerialize();
    testDocumentApplyFormat();
    testDocumentCreateSection();
    testDocumentAfiliadasEdit();
    testDocumentEolPreservation();
    testDocumentTolerances();
    testLocateFiles();
    testValidation();
    testReadingScopes();
    testTextFileIOAccents();

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " ok\n";
    if (g_failures != 0) {
        std::cout << g_failures << " FALHA(S)\n";
        return 1;
    }
    std::cout << "TODOS OS TESTES PASSARAM\n";
    return 0;
}