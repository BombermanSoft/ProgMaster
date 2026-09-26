#include "readconf/FormatRules.h"

#include <cwctype>

namespace readconf {
namespace {

// Compara duas strings ASCII/wchar case-insensitive (usado para nomes de
// seção e chaves, que no playlist.ini são ASCII).
bool iequals(const std::wstring& a, const std::wstring& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) {
            return false;
        }
    }
    return true;
}

bool istartsWith(const std::wstring& s, const std::wstring& prefix)
{
    if (s.size() < prefix.size()) {
        return false;
    }
    return iequals(s.substr(0, prefix.size()), prefix);
}

bool iendsWith(const std::wstring& s, const std::wstring& suffix)
{
    if (s.size() < suffix.size()) {
        return false;
    }
    return iequals(s.substr(s.size() - suffix.size()), suffix);
}

// Dobra acentos PT-BR e caixa para comparação tolerante: "Sáb" == "Sab"
// (o Manual escreve com acento; arquivos antigos no disco usam "Sab").
wchar_t foldAccent(wchar_t c)
{
    switch (c) {
    case L'á': case L'à': case L'â': case L'ã': case L'ä': return L'a';
    case L'é': case L'è': case L'ê': case L'ë': return L'e';
    case L'í': case L'ì': case L'î': case L'ï': return L'i';
    case L'ó': case L'ò': case L'ô': case L'õ': case L'ö': return L'o';
    case L'ú': case L'ù': case L'û': case L'ü': return L'u';
    case L'ç': return L'c';
    default: return std::towlower(c);
    }
}

// Igualdade ignorando caixa E acentos (acordes de tamanho igual).
bool equivalence(const std::wstring& a, const std::wstring& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (foldAccent(a[i]) != foldAccent(b[i])) {
            return false;
        }
    }
    return true;
}

// termina com (sufixo) ignorando caixa E acentos.
bool iendsWithTolerant(const std::wstring& s, const std::wstring& suffix)
{
    if (s.size() < suffix.size()) {
        return false;
    }
    return equivalence(s.substr(s.size() - suffix.size()), suffix);
}

// Extrai a parte final (nome do arquivo) de um valor ARQUIVO, ignorando
// pastas separadas por '\' ou '/'.
std::wstring fileNamePartOf(const std::wstring& arquivoValue)
{
    const size_t slash = arquivoValue.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return arquivoValue;
    }
    return arquivoValue.substr(slash + 1);
}

// Nomes de dia da semana em português, na ordem Seg..Dom, como o Playlist os
// grava nos arquivos semanais (MapaSeg.txt, GradeTer.txt, RelogioSáb.txt).
// A grafia canônica usa "Sáb" (Manual); a comparação é tolerante a acentos
// para também reconhecer arquivos antigos gravados como "Sab" (ASCII).
const std::vector<std::wstring>& weekdayNames()
{
    static const std::vector<std::wstring> names = {
        L"Seg", L"Ter", L"Qua", L"Qui", L"Sex", L"Sáb", L"Dom",
    };
    return names;
}

// Padrões de data no valor ARQUIVO. No Comercial, COM prefixo "Mapa" a data é
// "Commercial Data" (Mapa%d-%m-%Y); SEM prefixo é "Planner" (\%d-%m-%Y.TXT).
// No Musical, SEM prefixo a data é "Maker" (GRADES\%d-%m-%Y.TXT). O padrão
// antigo "%d-%m%Y" (sem hífen) e "%d-%m-%y" (ano 2 díg) ainda são lidos.
bool isDateToken(const std::wstring& token)
{
    return iequals(token, L"%d-%m-%Y") || iequals(token, L"%d-%m%Y");
}

bool isOldMakerToken(const std::wstring& token)
{
    return iequals(token, L"%d-%m-%y");
}

bool isDayPattern(const std::wstring& token)
{
    return iequals(token, L"%d");
}

bool isWeeklyToken(const std::wstring& token)
{
    return iequals(token, L"%w") || iequals(token, L"%a");
}

// Verifica se o nome do arquivo termina com "<Prefix><dia da semana>" (com
// ou sem extensão). Devolve o nome do dia quando reconhecido. A comparação é
// tolerante a caixa E acentos ("RelogioSab.txt" reconhece o dia "Sáb").
bool matchWeeklyLiteral(const std::wstring& fileName,
                        const std::wstring& prefix,
                        std::wstring& outDay)
{
    for (const std::wstring& day : weekdayNames()) {
        std::wstring candidate = prefix + day;
        if (iendsWithTolerant(fileName, candidate) ||
            iendsWithTolerant(fileName, candidate + L".txt")) {
            outDay = day;
            return true;
        }
    }
    return false;
}

// Extrai o "token" final de um valor ARQUIVO: remove o nome de base fixo e a
// extensão, deixando apenas o padrão variável (ex.: "Mapa%d-%m%Y" -> "%d-%m%Y").
// Janeiro caso o arquivo não siga nenhum padrão conhecido.
std::wstring extractToken(const std::wstring& fileName,
                          const std::wstring& prefix)
{
    std::wstring base = fileName;
    if (!prefix.empty() && istartsWith(base, prefix)) {
        base = base.substr(prefix.size());
    }
    if (!base.empty() && base[0] == L' ') {
        base = base.substr(1);
    }

    // Remove extensão, se houver.
    const size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        // Só remove extensão típica de texto; não mexe em padrões com
        // extensão embutida (ex.: "%d-%m-%y" sem ponto não tem extensão).
        const std::wstring ext = base.substr(dot);
        if (iequals(ext, L".txt") || iequals(ext, L".ini")) {
            base = base.substr(0, dot);
        }
    }
    return base;
}

// Busca o parâmetro de uma opção dentro do posto. hasPrefix indica se o
// nome do ARQUIVO começava com o prefixo do escopo (Mapa/Grade/Relogio) —
// o que separa "Commercial Data" (com prefixo) de "Planner" (sem prefixo,
// no Comercial) e de "Maker" (sem prefixo, no Musical).
//   * "%d-%m-%Y" com prefixo                      -> Commercial Date
//   * "%d-%m-%Y" sem prefixo no Comercial         -> Planner
//   * "%d-%m-%Y" sem prefixo no Musical           -> Maker
//   * "%d-%m-%y" (Maker antigo, Musical)          -> Maker
//   * "%d-%m%Y" (antigo, Comercial com prefixo)   -> Commercial Date
//   * "%d"        -> Commercial Dia
//   * "%w"/"%a"   -> Weekly
FormatMatch matchToken(ConfigScope scope, const std::wstring& base,
                       bool hasPrefix)
{
    FormatMatch match;
    if (isDateToken(base)) {
        if (scope == ConfigScope::Comercial) {
            match.option = hasPrefix ? FormatOption::CommercialDate
                                     : FormatOption::Planner;
            match.matchedToken = base;
            return match;
        }
        if (scope == ConfigScope::Musical) {
            match.option = FormatOption::Maker;
            match.matchedToken = base;
            return match;
        }
        return {};
    }
    if (isOldMakerToken(base)) {
        if (scope == ConfigScope::Musical) {
            match.option = FormatOption::Maker;
            match.matchedToken = base;
            return match;
        }
        return {};
    }
    if (isDayPattern(base)) {
        if (scope == ConfigScope::Comercial) {
            match.option = FormatOption::CommercialDay;
            match.matchedToken = base;
            return match;
        }
        return {};
    }
    if (isWeeklyToken(base)) {
        match.option = FormatOption::Weekly;
        match.matchedToken = base;
        return match;
    }
    return {};
}

bool sectionHasSingleFile(const std::wstring& fileName,
                          const std::wstring& baseName)
{
    return iendsWith(fileName, baseName + L".txt") ||
           iequals(fileName, baseName);
}

} // namespace

std::wstring sectionNameFor(ConfigScope scope)
{
    switch (scope) {
    case ConfigScope::Comercial:        return L"BLOCO COMERCIAL";
    case ConfigScope::Musical:          return L"BLOCO MUSICAL";
    case ConfigScope::RelogioComercial: return L"RELOGIO COMERCIAL";
    case ConfigScope::RelogioMusical:   return L"RELOGIO MUSICAL";
    case ConfigScope::Afiliadas:        return L"AFILIADAS";
    }
    return L"";
}

std::wstring scopeDisplayName(ConfigScope scope)
{
    switch (scope) {
    case ConfigScope::Comercial:        return L"Bloco Comercial";
    case ConfigScope::Musical:          return L"Bloco Musical";
    case ConfigScope::RelogioComercial: return L"Relógio Comercial";
    case ConfigScope::RelogioMusical:   return L"Relógio Musical";
    case ConfigScope::Afiliadas:        return L"Afiliadas";
    }
    return L"";
}

std::vector<FormatOption> optionsForFormat(ConfigScope scope)
{
    switch (scope) {
    case ConfigScope::Comercial:
        return { FormatOption::Auto, FormatOption::Single, FormatOption::Weekly,
                 FormatOption::CommercialDay, FormatOption::CommercialDate,
                 FormatOption::Planner };
    case ConfigScope::Musical:
        return { FormatOption::Auto, FormatOption::Single, FormatOption::Weekly,
                 FormatOption::Maker };
    case ConfigScope::RelogioComercial:
    case ConfigScope::RelogioMusical:
        return { FormatOption::Single, FormatOption::Weekly };
    case ConfigScope::Afiliadas:
        return {};
    }
    return {};
}

std::wstring displayName(FormatOption option)
{
    switch (option) {
    case FormatOption::Auto:           return L"AUTO";
    case FormatOption::Single:         return L"Mapa/Grade (único)";
    case FormatOption::Weekly:         return L"Semanal";
    case FormatOption::CommercialDay:  return L"Commercial Dia";
    case FormatOption::CommercialDate: return L"Commercial Data";
    case FormatOption::Planner:        return L"Planner";
    case FormatOption::Maker:          return L"Maker";
    case FormatOption::Unknown:        return L"Não reconhecido";
    }
    return L"";
}

bool optionAppliesTo(ConfigScope scope, FormatOption option)
{
    for (const FormatOption o : optionsForFormat(scope)) {
        if (o == option) {
            return true;
        }
    }
    return false;
}

FormatMatch interpret(ConfigScope scope,
                      const std::wstring& formatoValue,
                      const std::wstring& arquivoValue)
{
    // FORMATO=AUTO (somente blocos) mapeia direto para AUTO.
    if (iequals(formatoValue, L"AUTO")) {
        if (scope == ConfigScope::Comercial || scope == ConfigScope::Musical) {
            FormatMatch m;
            m.option = FormatOption::Auto;
            m.matchedToken = L"AUTO";
            return m;
        }
        // Relógios não têm AUTO; um FORMATO=AUTO lá é preservado como
        // desconhecido.
        return {};
    }

    if (arquivoValue.empty()) {
        return {}; // nenhum padrão objetivo sem ARQUIVO
    }

    return interpretArquivo(scope, arquivoValue);
}

FormatMatch interpretArquivo(ConfigScope scope, const std::wstring& arquivoValue)
{
    const std::wstring fileName = fileNamePartOf(arquivoValue);
    if (fileName.empty()) {
        return {};
    }

    // Prefixo do nome de base da cada escopo.
    std::wstring prefix;
    std::wstring singleBase;
    switch (scope) {
    case ConfigScope::Comercial:        prefix = L"Mapa"; singleBase = L"Mapa"; break;
    case ConfigScope::Musical:          prefix = L"Grade"; singleBase = L"Grade"; break;
    case ConfigScope::RelogioComercial:
    case ConfigScope::RelogioMusical:   prefix = L"Relogio"; singleBase = L"Relogio"; break;
    case ConfigScope::Afiliadas:        return {};
    }

    // 1) Arquivo único: Mapa.txt / Grade.txt / Relogio.txt.
    if (sectionHasSingleFile(fileName, singleBase)) {
        FormatMatch m;
        m.option = FormatOption::Single;
        m.matchedToken = singleBase + L".txt";
        return m;
    }

    // 2) Semanal por nome de dia por extenso (MapaSeg.txt, GradeTer.txt,
    //    RelogioQua.txt...).
    std::wstring day;
    if (matchWeeklyLiteral(fileName, prefix, day)) {
        FormatMatch m;
        m.option = FormatOption::Weekly;
        m.matchedToken = prefix + day;
        return m;
    }

    // 3) Token variável (Mapa%w.txt, Mapa%d, Mapa%d-%m-%Y, Grade%w,
    //    %d-%m-%Y, %d-%m-%y).
    if (iendsWith(fileName, prefix) || istartsWith(fileName, prefix) ||
        istartsWith(fileName, L"%")) {
        const bool hasPrefix = istartsWith(fileName, prefix);
        const std::wstring token = extractToken(fileName, prefix);
        if (!token.empty()) {
            return matchToken(scope, token, hasPrefix);
        }
    }

    return {};
}

GeneratedFormat generate(ConfigScope scope, FormatOption option)
{
    GeneratedFormat out;

    // AUTO: FORMATO=AUTO e removal do ARQUIVO (a busca é por regra).
    if (option == FormatOption::Auto) {
        if (!optionAppliesTo(scope, option)) {
            return out;
        }
        out.lines.push_back(L"FORMATO=AUTO");
        out.removesArquivo = true;
        return out;
    }

    // Os demais formatos usam FORMATO=TXT1.
    out.lines.push_back(L"FORMATO=TXT1");

    switch (option) {
    case FormatOption::Single:
        switch (scope) {
        case ConfigScope::Comercial:        out.lines.push_back(L"ARQUIVO=MAPAS\\Mapa.txt"); break;
        case ConfigScope::Musical:          out.lines.push_back(L"ARQUIVO=GRADES\\Grade.txt"); break;
        case ConfigScope::RelogioComercial: out.lines.push_back(L"ARQUIVO=MAPAS\\Relogio.txt"); break;
        case ConfigScope::RelogioMusical:   out.lines.push_back(L"ARQUIVO=GRADES\\Relogio.txt"); break;
        case ConfigScope::Afiliadas:        out.lines.clear(); break;
        }
        break;
    case FormatOption::Weekly:
        switch (scope) {
        case ConfigScope::Comercial:        out.lines.push_back(L"ARQUIVO=MAPAS\\Mapa%a.txt"); break;
        case ConfigScope::Musical:          out.lines.push_back(L"ARQUIVO=GRADES\\Grade%a.txt"); break;
        case ConfigScope::RelogioComercial: out.lines.push_back(L"ARQUIVO=MAPAS\\Relogio%a.txt"); break;
        case ConfigScope::RelogioMusical:   out.lines.push_back(L"ARQUIVO=GRADES\\Relogio%a.txt"); break;
        case ConfigScope::Afiliadas:        out.lines.clear(); break;
        }
        break;
    case FormatOption::CommercialDay:
        if (scope == ConfigScope::Comercial) {
            out.lines.push_back(L"ARQUIVO=MAPAS\\Mapa%d.txt");
        } else {
            out.lines.clear();
        }
        break;
    case FormatOption::CommercialDate:
        if (scope == ConfigScope::Comercial) {
            out.lines.push_back(L"ARQUIVO=MAPAS\\Mapa%d-%m-%Y.txt");
        } else {
            out.lines.clear();
        }
        break;
    case FormatOption::Planner:
        if (scope == ConfigScope::Comercial) {
            out.lines.push_back(L"ARQUIVO=MAPAS\\%d-%m-%Y.TXT");
        } else {
            out.lines.clear();
        }
        break;
    case FormatOption::Maker:
        if (scope == ConfigScope::Musical) {
            out.lines.push_back(L"ARQUIVO=GRADES\\%d-%m-%Y.TXT");
        } else {
            out.lines.clear();
        }
        break;
    case FormatOption::Auto:
    case FormatOption::Unknown:
        out.lines.clear();
        break;
    }

    // Se gerou apenas FORMATO=TXT1 sem ARQUIVO (não deveria acontecer para
    // uma opção válida), consideramos inválido.
    if (out.lines.size() == 1) {
        out.lines.clear();
    }
    return out;
}

const std::vector<std::wstring>& weekdayFileNames()
{
    return weekdayNames();
}

std::vector<std::wstring> expectedFileBases(ConfigScope scope,
                                            FormatOption option)
{
    std::vector<std::wstring> bases;
    switch (option) {
    case FormatOption::Single:
        if (scope == ConfigScope::Comercial) bases.push_back(L"Mapa.txt");
        if (scope == ConfigScope::Musical)   bases.push_back(L"Grade.txt");
        if (scope == ConfigScope::RelogioComercial ||
            scope == ConfigScope::RelogioMusical) bases.push_back(L"Relogio.txt");
        break;
    case FormatOption::Weekly:
        for (const std::wstring& day : weekdayNames()) {
            switch (scope) {
            case ConfigScope::Comercial:        bases.push_back(L"Mapa" + day); break;
            case ConfigScope::Musical:          bases.push_back(L"Grade" + day); break;
            case ConfigScope::RelogioComercial:
            case ConfigScope::RelogioMusical:   bases.push_back(L"Relogio" + day); break;
            case ConfigScope::Afiliadas:        break;
            }
        }
        break;
    case FormatOption::CommercialDay:
        bases.push_back(L"MapaDD.txt");
        break;
    case FormatOption::CommercialDate:
        bases.push_back(L"MapaDD-MM-AAAA.txt");
        break;
    case FormatOption::Planner:
        bases.push_back(L"DD-MM-AAAA.TXT");
        break;
    case FormatOption::Maker:
        bases.push_back(L"DD-MM-AAAA.TXT");
        break;
    case FormatOption::Auto:
    case FormatOption::Unknown:
        switch (scope) {
        case ConfigScope::Comercial:
            bases = { L"MapaDD-MM-AAAA", L"MapaDD", L"MapaSeg", L"MapaTer",
                      L"MapaQua", L"MapaQui", L"MapaSex", L"MapaSáb", L"MapaDom",
                      L"Mapa.txt" };
            break;
        case ConfigScope::Musical:
            bases = { L"DD-MM-AAAA", L"GradeDD", L"GradeSeg", L"GradeTer",
                      L"GradeQua", L"GradeQui", L"GradeSex", L"GradeSáb",
                      L"GradeDom", L"Grade.txt" };
            break;
        default:
            break;
        }
        break;
    }
    return bases;
}

} // namespace readconf