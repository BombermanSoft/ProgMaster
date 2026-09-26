#include "relogio/RelogioDocument.h"

#include <cwctype>

namespace relogio {
namespace {

// Espaço ou tabulação (separador de parâmetros no formato do Playlist).
bool isWs(wchar_t c)
{
    return c == L' ' || c == L'\t';
}

// Nome canônico (maiúsculo, como o Playlist grava) de um parâmetro.
const wchar_t* canonicalName(ParamKind kind)
{
    switch (kind) {
    case ParamKind::Fixo:     return L"FIXO";
    case ParamKind::Descarte: return L"DESCARTE";
    case ParamKind::Local:    return L"LOCAL";
    case ParamKind::Sat:      return L"SAT";
    case ParamKind::Locked:   return L"LOCKED";
    case ParamKind::Id:       return L"ID";
    case ParamKind::Dur:      return L"DUR";
    }
    return L"";
}

// Parâmetros com valor: (ID=valor), (DUR=valor). Os demais são sem valor.
bool paramHasValue(ParamKind kind)
{
    return kind == ParamKind::Id || kind == ParamKind::Dur;
}

// Igualdade case-insensitive entre uma wstring e uma literal ASCII maiúscula.
bool paramNameEquals(const std::wstring& s, const wchar_t* canon)
{
    const wchar_t* q = canon;
    for (const wchar_t c : s) {
        if (*q == L'\0') {
            return false;
        }
        if (std::towlower(c) != std::towlower(*q)) {
            return false;
        }
        ++q;
    }
    return *q == L'\0';
}

} // namespace

int RelogioDocument::parseTime(const std::wstring& hhmm)
{
    if (hhmm.size() != 5 || hhmm[2] != L':') {
        return -1;
    }
    for (int k = 0; k < 5; ++k) {
        if (k != 2 && (hhmm[k] < L'0' || hhmm[k] > L'9')) {
            return -1;
        }
    }
    const int hh = (hhmm[0] - L'0') * 10 + (hhmm[1] - L'0');
    const int mm = (hhmm[3] - L'0') * 10 + (hhmm[4] - L'0');
    if (hh > 23 || mm > 59) {
        return -1;
    }
    return hh * 60 + mm;
}

std::wstring RelogioDocument::formatTime(int minutes)
{
    if (minutes < 0 || minutes > 1439) {
        return L"";
    }
    const int hh = minutes / 60;
    const int mm = minutes % 60;
    wchar_t buf[6] = {
        static_cast<wchar_t>(L'0' + hh / 10),
        static_cast<wchar_t>(L'0' + hh % 10),
        L':',
        static_cast<wchar_t>(L'0' + mm / 10),
        static_cast<wchar_t>(L'0' + mm % 10),
    };
    return std::wstring(buf, 5);
}

bool RelogioDocument::parseParamToken(const std::wstring& token, Param& out)
{
    if (token.empty() || token[0] == L'=') {
        return false;
    }
    const size_t eq = token.find(L'=');
    std::wstring name = (eq == std::wstring::npos) ? token : token.substr(0, eq);
    const std::wstring value =
        (eq == std::wstring::npos) ? std::wstring() : token.substr(eq + 1);

    while (!name.empty() && isWs(name.front())) {
        name.erase(name.begin());
    }
    while (!name.empty() && isWs(name.back())) {
        name.pop_back();
    }
    if (name.empty()) {
        return false;
    }

    ParamKind kind;
    if (paramNameEquals(name, L"FIXO")) {
        kind = ParamKind::Fixo;
    } else if (paramNameEquals(name, L"DESCARTE")) {
        kind = ParamKind::Descarte;
    } else if (paramNameEquals(name, L"LOCAL")) {
        kind = ParamKind::Local;
    } else if (paramNameEquals(name, L"SAT")) {
        kind = ParamKind::Sat;
    } else if (paramNameEquals(name, L"LOCKED")) {
        kind = ParamKind::Locked;
    } else if (paramNameEquals(name, L"ID")) {
        kind = ParamKind::Id;
    } else if (paramNameEquals(name, L"DUR")) {
        kind = ParamKind::Dur;
    } else {
        return false;
    }

    out.kind = kind;
    out.name = canonicalName(kind);
    out.value = value;
    out.token.clear();
    return true;
}

std::wstring RelogioDocument::paramText(const Param& p)
{
    std::wstring s = L"(" + p.name;
    if (paramHasValue(p.kind) && !p.value.empty()) {
        s += L"=" + p.value;
    }
    s += L")";
    return s;
}

// Interpreta uma linha: devolve o horário estruturado se a linha começa com
// HH:MM válido; senão deixa o conteúdo para o editor visual preservar como
// "Raw". Parâmetros são os parênteses reconhecidos que aparecem logo após o
// horário; qualquer outra coisa (inclusive parênteses desconhecidos) é
// conteúdo preservado.
void RelogioDocument::setText(const std::wstring& text)
{
    m_lines.clear();

    m_eol = L"\r\n";
    m_hasTrailingEol = false;
    if (text.empty()) {
        return;
    }

    const size_t nl = text.find(L'\n');
    if (nl != std::wstring::npos) {
        m_eol = (nl > 0 && text[nl - 1] == L'\r') ? L"\r\n" : L"\n";
    }
    m_hasTrailingEol = text.back() == L'\n';

    size_t start = 0;
    while (start < text.size()) {
        const size_t lineEnd = text.find(L'\n', start);
        std::wstring line;
        size_t next;
        if (lineEnd == std::wstring::npos) {
            line = text.substr(start);
            next = text.size();
        } else {
            size_t end = lineEnd;
            if (end > start && text[end - 1] == L'\r') {
                --end;
            }
            line = text.substr(start, end - start);
            next = lineEnd + 1;
        }

        Line l;
        if (line.size() >= 5 && parseTime(line.substr(0, 5)) >= 0) {
            l.kind = Line::Kind::Horario;
            Horario& h = l.horario;
            h.time = line.substr(0, 5);
            const std::wstring rest = line.substr(5);
            size_t i = 0;
            while (i < rest.size()) {
                // O próximo parâmetro é: (espaços) seguidos de "(" ... ")" com
                // conteúdo reconhecido. Qualquer outro texto encerra a zona de
                // parâmetros e vira conteúdo preservado.
                size_t wsEnd = i;
                while (wsEnd < rest.size() && isWs(rest[wsEnd])) {
                    ++wsEnd;
                }
                if (wsEnd >= rest.size() || rest[wsEnd] != L'(') {
                    break;
                }
                const size_t close = rest.find(L')', wsEnd);
                if (close == std::wstring::npos) {
                    break;
                }
                Param p;
                if (!parseParamToken(rest.substr(wsEnd + 1, close - wsEnd - 1), p)) {
                    break;
                }
                p.token = rest.substr(i, close - i + 1);
                h.params.push_back(std::move(p));
                i = close + 1;
            }
            h.trailing = rest.substr(i);
        } else {
            l.kind = Line::Kind::Raw;
            l.raw = std::move(line);
        }
        m_lines.push_back(std::move(l));

        start = next;
    }
}

std::wstring RelogioDocument::text() const
{
    std::wstring out;
    bool first = true;
    for (const Line& l : m_lines) {
        if (!first) {
            out += m_eol;
        }
        first = false;
        if (l.kind == Line::Kind::Horario) {
            out += l.horario.time;
            for (const Param& p : l.horario.params) {
                out += p.token;
            }
            out += l.horario.trailing;
        } else {
            out += l.raw;
        }
    }
    if (m_hasTrailingEol) {
        out += m_eol;
    }
    return out;
}

int RelogioDocument::addTime(const std::wstring& hhmm)
{
    const int minutes = parseTime(hhmm);
    if (minutes < 0) {
        return -1;
    }
    for (const Line& l : m_lines) {
        if (l.kind == Line::Kind::Horario && parseTime(l.horario.time) == minutes) {
            return -1; // duplicado
        }
    }
    const size_t idx = insertionIndex(minutes);
    Line l;
    l.kind = Line::Kind::Horario;
    l.horario.time = formatTime(minutes);
    m_lines.insert(m_lines.begin() + idx, std::move(l));
    return static_cast<int>(idx);
}

void RelogioDocument::addParam(int lineIndex, ParamKind kind,
                               const std::wstring& value)
{
    if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= m_lines.size()) {
        return;
    }
    Line& l = m_lines[static_cast<size_t>(lineIndex)];
    if (l.kind != Line::Kind::Horario) {
        return;
    }
    Param p;
    p.kind = kind;
    p.name = canonicalName(kind);
    p.value = paramHasValue(kind) ? value : std::wstring();
    p.token = L" " + paramText(p);
    l.horario.params.push_back(std::move(p));
}

void RelogioDocument::removeParam(int lineIndex, int paramIndex)
{
    if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= m_lines.size()) {
        return;
    }
    Line& l = m_lines[static_cast<size_t>(lineIndex)];
    if (l.kind != Line::Kind::Horario) {
        return;
    }
    std::vector<Param>& params = l.horario.params;
    if (paramIndex < 0 || static_cast<size_t>(paramIndex) >= params.size()) {
        return;
    }
    params.erase(params.begin() + paramIndex);
}

void RelogioDocument::replaceParams(int lineIndex, const std::vector<Param>& params)
{
    if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= m_lines.size()) {
        return;
    }
    Line& l = m_lines[static_cast<size_t>(lineIndex)];
    if (l.kind != Line::Kind::Horario) {
        return;
    }
    l.horario.params = params;
}

bool RelogioDocument::removeLine(int lineIndex)
{
    if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= m_lines.size()) {
        return false;
    }
    m_lines.erase(m_lines.begin() + lineIndex);
    return true;
}

void RelogioDocument::replaceFrom(const RelogioDocument& other)
{
    m_lines = other.m_lines;
    m_eol = other.m_eol;
    m_hasTrailingEol = other.m_hasTrailingEol;
}

size_t RelogioDocument::insertionIndex(int minutes) const
{
    for (size_t i = 0; i < m_lines.size(); ++i) {
        const Line& l = m_lines[i];
        if (l.kind == Line::Kind::Horario && parseTime(l.horario.time) >= minutes) {
            return i;
        }
    }
    return m_lines.size();
}

} // namespace relogio