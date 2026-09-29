#include <iostream>
#include <string>
#include <vector>

#include "relogio/RelogioDocument.h"

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
                } else {
                    r += '.';
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

using namespace relogio;

int countHorarios(const RelogioDocument& d)
{
    int n = 0;
    for (const RelogioDocument::Line& l : d.lines()) {
        if (l.kind == RelogioDocument::Line::Kind::Horario) {
            ++n;
        }
    }
    return n;
}

void testParseTime()
{
    CHECK_MSG(RelogioDocument::parseTime(L"00:00") == 0, "00:00 -> 0");
    CHECK_MSG(RelogioDocument::parseTime(L"12:30") == 750, "12:30 -> 750");
    CHECK_MSG(RelogioDocument::parseTime(L"23:59") == 1439, "23:59 -> 1439");
    CHECK_MSG(RelogioDocument::parseTime(L"24:00") == -1, "24:00 invalido");
    CHECK_MSG(RelogioDocument::parseTime(L"00:60") == -1, "00:60 invalido");
    CHECK_MSG(RelogioDocument::parseTime(L"0:00") == -1, "0:00 curto");
    CHECK_MSG(RelogioDocument::parseTime(L"12:3x") == -1, "12:3x invalido");
    CHECK_MSG(RelogioDocument::parseTime(L"1200") == -1, "sem ':' invalido");
    CHECK_MSG(RelogioDocument::parseTime(L"") == -1, "vazio invalido");
}

void testFormatTime()
{
    CHECK_EQ(RelogioDocument::formatTime(0), L"00:00");
    CHECK_EQ(RelogioDocument::formatTime(9), L"00:09");
    CHECK_EQ(RelogioDocument::formatTime(750), L"12:30");
    CHECK_EQ(RelogioDocument::formatTime(1439), L"23:59");
    CHECK_EQ(RelogioDocument::formatTime(-1), L"");
    CHECK_EQ(RelogioDocument::formatTime(1440), L"");
}

void testParamTokens()
{
    Param p;
    CHECK_MSG(RelogioDocument::parseParamToken(L"FIXO", p), "FIXO reconhecido");
    CHECK_MSG(p.kind == ParamKind::Fixo, "FIXO kind");
    CHECK_MSG(p.value.empty(), "FIXO sem valor");

    CHECK_MSG(RelogioDocument::parseParamToken(L"fixo", p), "fixo em caixa baixa");
    CHECK_MSG(RelogioDocument::parseParamToken(L"DESCARTE", p), "DESCARTE");
    CHECK_MSG(RelogioDocument::parseParamToken(L"LOCAL", p), "LOCAL");
    CHECK_MSG(RelogioDocument::parseParamToken(L"SAT", p), "SAT");
    CHECK_MSG(RelogioDocument::parseParamToken(L"LOCKED", p), "LOCKED");

    CHECK_MSG(RelogioDocument::parseParamToken(L"ID=Exemplo", p), "ID=Exemplo");
    CHECK_MSG(p.kind == ParamKind::Id, "ID kind");
    CHECK_EQ(p.value, L"Exemplo");

    CHECK_MSG(RelogioDocument::parseParamToken(L"DUR=3:00", p), "DUR=3:00");
    CHECK_MSG(p.kind == ParamKind::Dur, "DUR kind");
    CHECK_EQ(p.value, L"3:00");

    CHECK_MSG(RelogioDocument::parseParamToken(L"XYZ", p) == false,
              "XYZ desconhecido");
    CHECK_MSG(RelogioDocument::parseParamToken(L"", p) == false, "vazio");
    CHECK_MSG(RelogioDocument::parseParamToken(L"=x", p) == false, "=x invalido");
}

void testParamText()
{
    Param f;
    f.kind = ParamKind::Fixo;
    f.name = L"FIXO";
    CHECK_EQ(RelogioDocument::paramText(f), L"(FIXO)");

    Param id;
    id.kind = ParamKind::Id;
    id.name = L"ID";
    id.value = L"Exemplo";
    CHECK_EQ(RelogioDocument::paramText(id), L"(ID=Exemplo)");

    Param dur;
    dur.kind = ParamKind::Dur;
    dur.name = L"DUR";
    CHECK_EQ(RelogioDocument::paramText(dur), L"(DUR)");
}

void testRoundTripTimes()
{
    RelogioDocument d;
    d.setText(L"00:00\r\n00:15\r\n00:30\r\n");
    CHECK_EQ(d.text(), L"00:00\r\n00:15\r\n00:30\r\n");
    CHECK_MSG(countHorarios(d) == 3, "3 horarios");
    CHECK_EQ(d.lines()[1].horario.time, L"00:15");

    RelogioDocument lf;
    lf.setText(L"06:00\n07:00\n");
    CHECK_EQ(lf.text(), L"06:00\n07:00\n");

    RelogioDocument noeol;
    noeol.setText(L"06:00\n07:00");
    CHECK_EQ(noeol.text(), L"06:00\n07:00");
}

void testPreserveBlanksAndComments()
{
    RelogioDocument d;
    d.setText(L"00:00\r\n\r\n# meu relogio\r\n00:30 (FIXO)\r\n");
    CHECK_EQ(d.text(), L"00:00\r\n\r\n# meu relogio\r\n00:30 (FIXO)\r\n");
    CHECK_MSG(countHorarios(d) == 2, "2 horarios entre blanks/comentarios");
    CHECK_MSG(d.lines()[1].kind == RelogioDocument::Line::Kind::Raw,
              "linha em branco e Raw");
    CHECK_MSG(d.lines()[2].kind == RelogioDocument::Line::Kind::Raw,
              "comentario e Raw");
    CHECK_MSG(d.lines()[3].horario.params.size() == 1, "linha com (FIXO)");
}

void testModeloExample()
{
    RelogioDocument d;
    d.setText(L"00:02 (DUR=13:00) AB, LOC1, MUS1, VHP, ...\r\n");
    CHECK_MSG(countHorarios(d) == 1, "1 horario");
    CHECK_EQ(d.lines()[0].horario.time, L"00:02");
    CHECK_MSG(d.lines()[0].horario.params.size() == 1, "1 parametro");
    CHECK_MSG(d.lines()[0].horario.params[0].kind == ParamKind::Dur, "par DUR");
    CHECK_EQ(d.lines()[0].horario.params[0].value, L"13:00");
    CHECK_EQ(d.lines()[0].horario.trailing, L" AB, LOC1, MUS1, VHP, ...");
    CHECK_EQ(d.text(), L"00:02 (DUR=13:00) AB, LOC1, MUS1, VHP, ...\r\n");
}

void testMultipleParams()
{
    // Formato antigo (um grupo por parâmetro) continua sendo lido.
    RelogioDocument d;
    d.setText(L"07:30 (FIXO) (ID=Exemplo) conteudo\r\n");
    CHECK_MSG(d.lines()[0].horario.params.size() == 2, "2 parametros");
    CHECK_MSG(d.lines()[0].horario.params[0].kind == ParamKind::Fixo,
              "primeiro FIXO");
    CHECK_MSG(d.lines()[0].horario.params[1].kind == ParamKind::Id,
              "segundo ID");
    // A serialização usa UM grupo com vírgulas: (FIXO, ID=Exemplo).
    CHECK_EQ(d.text(), L"07:30 (FIXO, ID=Exemplo) conteudo\r\n");

    // Formato canônico: vários parâmetros num único parêntese, com vírgulas.
    RelogioDocument c;
    c.setText(L"07:30 (SAT, DESCARTE, ID=Noticias) conteudo\r\n");
    CHECK_MSG(c.lines()[0].horario.params.size() == 3, "3 parametros no grupo");
    CHECK_MSG(c.lines()[0].horario.params[0].kind == ParamKind::Sat, "SAT");
    CHECK_MSG(c.lines()[0].horario.params[1].kind == ParamKind::Descarte,
              "DESCARTE");
    CHECK_MSG(c.lines()[0].horario.params[2].kind == ParamKind::Id,
              "ID depois de vírgula");
    CHECK_EQ(c.text(), L"07:30 (SAT, DESCARTE, ID=Noticias) conteudo\r\n");
}

void testUnknownParenIsContent()
{
    // Parenteses desconhecidos sao conteudo preservado, nunca parametro.
    RelogioDocument a;
    a.setText(L"09:00 (FOO) x\r\n");
    CHECK_MSG(a.lines()[0].horario.params.empty(), "(FOO) nao e parametro");
    CHECK_EQ(a.text(), L"09:00 (FOO) x\r\n");

    // Parametro parecido com nome conhecido no MEIO do conteudo nao e lido.
    RelogioDocument b;
    b.setText(L"09:00 conteudo (FIXO)\r\n");
    CHECK_MSG(b.lines()[0].horario.params.empty(), "(FIXO) no conteudo ignorado");
    CHECK_EQ(b.text(), L"09:00 conteudo (FIXO)\r\n");
}

void testAddTime()
{
    RelogioDocument d;
    d.setText(L"00:00\r\n00:30\r\n");
    const int idx = d.addTime(L"00:15");
    CHECK_MSG(idx == 1, "inserido no meio");
    CHECK_EQ(d.text(), L"00:00\r\n00:15\r\n00:30\r\n");

    CHECK_MSG(d.addTime(L"00:15") == -1, "duplicado recusado");
    CHECK_MSG(d.addTime(L"24:00") == -1, "hora invalida recusada");
    CHECK_MSG(d.addTime(L"00:00") == -1, "duplicado existente recusado");

    const int last = d.addTime(L"23:59");
    CHECK_MSG(last == 3, "novo horario no fim");
    CHECK_EQ(d.text(), L"00:00\r\n00:15\r\n00:30\r\n23:59\r\n");
}

void testAddTimeAroundRawLines()
{
    RelogioDocument d;
    d.setText(L"00:00\r\n\r\n00:30\r\n");
    d.addTime(L"00:15");
    CHECK_EQ(d.text(), L"00:00\r\n\r\n00:15\r\n00:30\r\n");
}

void testAddRemoveParams()
{
    RelogioDocument d;
    d.setText(L"10:00\r\n");
    CHECK_MSG(d.addParam(0, ParamKind::Fixo, L""), "FIXO adicionado");
    CHECK_EQ(d.text(), L"10:00 (FIXO)\r\n");
    CHECK_MSG(d.lines()[0].horario.params.size() == 1, "1 parametro apos add");

    // O mesmo parâmetro não pode ser adicionado duas vezes.
    CHECK_MSG(!d.addParam(0, ParamKind::Fixo, L""), "FIXO duplicado recusado");
    CHECK_MSG(d.lines()[0].horario.params.size() == 1, "ainda 1 parametro");
    CHECK_EQ(d.text(), L"10:00 (FIXO)\r\n");

    d.removeParam(0, 0);
    CHECK_MSG(d.lines()[0].horario.params.empty(), "parametro removido");
    CHECK_EQ(d.text(), L"10:00\r\n");

    CHECK_MSG(d.addParam(0, ParamKind::Id, L"Exemplo"), "ID adicionado");
    CHECK_EQ(d.text(), L"10:00 (ID=Exemplo)\r\n");

    CHECK_MSG(d.addParam(0, ParamKind::Dur, L"3:00"), "DUR adicionado");
    // Mais de um parâmetro: mesmo grupo, separados por vírgula.
    CHECK_EQ(d.text(), L"10:00 (ID=Exemplo, DUR=3:00)\r\n");

    // ID duplicado recusado mesmo com outro valor.
    CHECK_MSG(!d.addParam(0, ParamKind::Id, L"Outro"), "ID duplicado recusado");
    CHECK_EQ(d.text(), L"10:00 (ID=Exemplo, DUR=3:00)\r\n");
}

void testReplaceParams()
{
    RelogioDocument src;
    src.setText(L"05:00 (FIXO) (DUR=2:00)\r\n");

    RelogioDocument dst;
    dst.setText(L"06:00\r\n");
    dst.replaceParams(0, src.lines()[0].horario.params);
    CHECK_EQ(dst.text(), L"06:00 (FIXO, DUR=2:00)\r\n");
    CHECK_MSG(dst.lines()[0].horario.params.size() == 2, "2 parametros copiados");

    // Colar SUBSTITUI os parâmetros do destino (sem duplicar): o destino fica
    // com exatamente o que estava na área de transferência.
    RelogioDocument dst2;
    dst2.setText(L"06:00 (FIXO)\r\n");
    dst2.replaceParams(0, src.lines()[0].horario.params);
    CHECK_EQ(dst2.text(), L"06:00 (FIXO, DUR=2:00)\r\n");
}

void testRemoveLine()
{
    RelogioDocument d;
    d.setText(L"00:00\r\n00:15\r\n00:30\r\n");
    CHECK_MSG(d.removeLine(1), "remove linha 1");
    CHECK_EQ(d.text(), L"00:00\r\n00:30\r\n");
    d.removeLine(9);
    CHECK_MSG(countHorarios(d) == 2, "indice invalido nao altera");
}

void testReplaceFrom()
{
    RelogioDocument d;
    d.setText(L"05:00\r\n");

    RelogioDocument other;
    other.setText(L"06:00 (FIXO)\r\n# nota\r\n");

    d.replaceFrom(other);
    CHECK_EQ(d.text(), L"06:00 (FIXO)\r\n# nota\r\n");
    CHECK_MSG(countHorarios(d) == 1, "novo documento tem 1 horario");
}

void testOrderOfParamsPreserved()
{
    // Ordem dos parametros no arquivo != ordem canonica da enum.
    RelogioDocument d;
    d.setText(L"08:00 (SAT) (LOCKED) (FIXO)\r\n");
    CHECK_MSG(d.lines()[0].horario.params.size() == 3, "3 parametros");
    CHECK_MSG(d.lines()[0].horario.params[0].kind == ParamKind::Sat, "SAT");
    CHECK_MSG(d.lines()[0].horario.params[2].kind == ParamKind::Fixo, "FIXO");
    CHECK_EQ(d.text(), L"08:00 (SAT, LOCKED, FIXO)\r\n");
}

void testReschedule()
{
    // "Preencher": mantém comentários/em branco, remove os horários antigos e
    // insere a nova seqüência em ordem cronológica.
    RelogioDocument d;
    d.setText(L"# turno da manhã\r\n00:10\r\n00:40\r\n\r\n");
    d.reschedule({ L"01:00", L"00:30", L"00:00" });
    CHECK_EQ(d.text(), L"# turno da manhã\r\n\r\n00:00\r\n00:30\r\n01:00\r\n");
    CHECK_MSG(countHorarios(d) == 3, "3 horarios apos preencher");

    // Horários inválidos e repetidos são ignorados.
    d.reschedule({ L"25:00", L"00:00", L"00:00", L"00:15" });
    CHECK_EQ(d.text(), L"# turno da manhã\r\n\r\n00:00\r\n00:15\r\n");
}

} // namespace

int main()
{
    std::cout << "ProgMaster Relogio Tests (Etapa 3)\n";

    testParseTime();
    testFormatTime();
    testParamTokens();
    testParamText();
    testRoundTripTimes();
    testPreserveBlanksAndComments();
    testModeloExample();
    testMultipleParams();
    testUnknownParenIsContent();
    testAddTime();
    testAddTimeAroundRawLines();
    testAddRemoveParams();
    testReplaceParams();
    testRemoveLine();
    testReplaceFrom();
    testOrderOfParamsPreserved();
    testReschedule();

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " ok\n";
    if (g_failures != 0) {
        std::cout << g_failures << " FALHA(S)\n";
        return 1;
    }
    std::cout << "TODOS OS TESTES PASSARAM\n";
    return 0;
}