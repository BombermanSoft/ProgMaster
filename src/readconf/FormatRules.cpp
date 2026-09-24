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

// Grams semanais em português, na ordem Seg..Dom, como "Seg" (os arquivos
// reais são MapaSeg.txt, GradeTer.txt, etc.).
const std::vector<std::wstring>& weekdayNames()
{
    static const std::vector<std::wstring> names = {
        L"Seg", L"Ter", L"Qua", L"Qui", L"Sex", L"Sab", L"Dom",
    };
    return names;
}

// npad do mês pra o padrão de data comercial: %d-%m%Y.
bool isDatePattern(const std::wstring& token)
{
    return iequals(token, L"%d-%m%Y");
}

bool isDayPattern(const std::wstring& token)
{
    return iequals(token, L"%d");
}

bool isMakerPattern(const std::wstring& token)
{
    return iequals(token, L"%d-%m-%y");
}

bool isWeeklyToken(const std::wstring& token)
{
    return iequals(token, L"%w") || iequals(token, L"%a");
}

// Verifica se o nome do arquivo termina com "<Prefix><dia da semana>" (com
// ou sem extensão). Devolve o nome do dia quando reconhecido.
bool matchWeeklyLiteral(const std::wstring& fileName,
                        const std::wstring& prefix,
                        std::wstring& outDay)
{
    for (const std::wstring& day : weekdayNames()) {
        std::wstring candidate = prefix + day;
        if (iendsWith(fileName, candidate) ||
            iendsWith(fileName, candidate + L".txt")) {
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

// Busca o parâmetro de uma opção dentro do posto:
// - base == "%d-%m%Y" -> Date
// - base == "%d"      -> Day
// - base == "%d-%m-%y"-> Maker
// - base == "%w"/"%a" -> Weekly
FormatMatch matchToken(ConfigScope scope, const std::wstring& base)
{
    FormatMatch match;
    if (isDatePattern(base)) {
        match.option = FormatOption::CommercialDate;
        match.matchedToken = base;
    } else if (isDayPattern(base)) {
        match.option = FormatOption::CommercialDay;
        match.matchedToken = base;
    } else if (isMakerPattern(base)) {
        match.option = FormatOption::Maker;
        match.matchedToken = base;
    } else if (isWeeklyToken(base)) {
        match.option = FormatOption::Weekly;
        match.matchedToken = base;
    }

    // Day/Date só se aplicam ao Comercial; Maker só ao Musical. Se detectado
    // fora do escopo, devolve Unknown (não invalida a chave: valor é
    // preservado).
    if (match.option == FormatOption::CommercialDay ||
        match.option == FormatOption::CommercialDate) {
        if (scope != ConfigScope::Comercial) {
            match = FormatMatch{};
        }
    } else if (match.option == FormatOption::Maker) {
        if (scope != ConfigScope::Musical) {
            match = FormatMatch{};
        }
    }
    return match;
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
                 FormatOption::CommercialDay, FormatOption::CommercialDate };
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

    // 3) Token variável (Mapa%w.txt, Mapa%d, Mapa%d-%m%Y, Grade%w, %d-%m-%y).
    if (iendsWith(fileName, prefix) || istartsWith(fileName, prefix) ||
        istartsWith(fileName, L"%")) {
        const std::wstring token = extractToken(fileName, prefix);
        if (!token.empty()) {
            return matchToken(scope, token);
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
        case ConfigScope::Musical:          out.lines.push_back(L"ARQUIVO=grades\\Grade.txt"); break;
        case ConfigScope::RelogioComercial:
        case ConfigScope::RelogioMusical:   out.lines.push_back(L"ARQUIVO=Mapas\\Relogio.txt"); break;
        case ConfigScope::Afiliadas:        out.lines.clear(); break;
        }
        break;
    case FormatOption::Weekly:
        switch (scope) {
        case ConfigScope::Comercial:        out.lines.push_back(L"ARQUIVO=MAPAS\\Mapa%w.txt"); break;
        case ConfigScope::Musical:          out.lines.push_back(L"ARQUIVO=grades\\Grade%w.txt"); break;
        case ConfigScope::RelogioComercial:
        case ConfigScope::RelogioMusical:   out.lines.push_back(L"ARQUIVO=Mapas\\Relogio%a.txt"); break;
        case ConfigScope::Afiliadas:        out.lines.clear(); break;
        }
        break;
    case FormatOption::CommercialDay:
        if (scope == ConfigScope::Comercial) {
            out.lines.push_back(L"ARQUIVO=MAPAS\\Mapa%d");
        } else {
            out.lines.clear();
        }
        break;
    case FormatOption::CommercialDate:
        if (scope == ConfigScope::Comercial) {
            out.lines.push_back(L"ARQUIVO=MAPAS\\Mapa%d-%m%Y");
        } else {
            out.lines.clear();
        }
        break;
    case FormatOption::Maker:
        if (scope == ConfigScope::Musical) {
            out.lines.push_back(L"ARQUIVO=grades\\%d-%m-%y");
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
        bases.push_back(L"MapaDD");
        break;
    case FormatOption::CommercialDate:
        bases.push_back(L"MapaDD-MM-AAAA");
        break;
    case FormatOption::Maker:
        bases.push_back(L"DD-MM-AA");
        break;
    case FormatOption::Auto:
    case FormatOption::Unknown:
        switch (scope) {
        case ConfigScope::Comercial:
            bases = { L"MapaDD-MM-AAAA", L"MapaDD", L"MapaSeg", L"MapaTer",
                      L"MapaQua", L"MapaQui", L"MapaSex", L"MapaSab", L"MapaDom",
                      L"Mapa.txt" };
            break;
        case ConfigScope::Musical:
            bases = { L"DD-MM-AAAA", L"GradeDD", L"GradeSeg", L"GradeTer",
                      L"GradeQua", L"GradeQui", L"GradeSex", L"GradeSab",
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