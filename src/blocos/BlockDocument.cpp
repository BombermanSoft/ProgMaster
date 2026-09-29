#include "blocos/BlockDocument.h"

#include <algorithm>
#include <cwctype>

namespace blocos {
namespace {

bool isWs(wchar_t c)
{
    return c == L' ' || c == L'\t';
}

void trimWs(std::wstring& s)
{
    while (!s.empty() && isWs(s.front())) {
        s.erase(s.begin());
    }
    while (!s.empty() && isWs(s.back())) {
        s.pop_back();
    }
}

// Comparação de códigos: a lista do Playlist é MAIÚSCULA, mas o arquivo pode
// conter variações; a comparação ignora a caixa.
bool codeEquals(const std::wstring& a, const std::wstring& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(static_cast<unsigned short>(a[i])) !=
            std::towlower(static_cast<unsigned short>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

int BlockDocument::parseTime(const std::wstring& hhmm)
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

std::wstring BlockDocument::formatTime(int minutes)
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

// Interpreta o trecho DEPOIS do horário: a lista de códigos separados por
// vírgula. Tudo que não for código (parênteses, textos) é devolvido em
// `trailing` para ser preservado verbatim.
bool BlockDocument::parseLine(const std::wstring& line, Horario& out)
{
    out = Horario();
    if (line.size() < 5 || parseTime(line.substr(0, 5)) < 0) {
        return false;
    }
    out.time = line.substr(0, 5);

    // Separador entre o horário e o primeiro código.
    size_t i = 5;
    const size_t sepStart = i;
    while (i < line.size() && isWs(line[i])) {
        ++i;
    }
    out.sep = line.substr(sepStart, i - sepStart);

    const std::wstring body = line.substr(i);

    // Divide por vírgulas; guarda até onde vai o ÚLTIMO código não vazio para
    // que a vírgula/espaços finais (padrão dos arquivos reais) virem o
    // `trailing` preservado.
    size_t tokenStart = 0;
    size_t lastCodeEnd = 0;
    bool sawAny = false;
    while (tokenStart <= body.size()) {
        const size_t comma = body.find(L',', tokenStart);
        const size_t tokenEnd =
            (comma == std::wstring::npos) ? body.size() : comma;
        std::wstring token = body.substr(tokenStart, tokenEnd - tokenStart);
        trimWs(token);
        if (!token.empty()) {
            out.codes.push_back(std::move(token));
            lastCodeEnd = tokenEnd;
            sawAny = true;
        }
        if (comma == std::wstring::npos) {
            break;
        }
        tokenStart = comma + 1;
    }

    if (sawAny) {
        // Desconta os espaços que antecediam o último código, que já fazem
        // parte da separação entre códigos.
        size_t end = lastCodeEnd;
        while (end > 0 && isWs(body[end - 1])) {
            --end;
        }
        out.trailing = body.substr(end);
    } else {
        out.trailing = body;
    }
    return true;
}

std::wstring BlockDocument::horarioText(const Horario& h)
{
    std::wstring out = h.time;
    if (!h.codes.empty()) {
        out += h.sep.empty() ? std::wstring(L" ") : h.sep;
        for (size_t k = 0; k < h.codes.size(); ++k) {
            if (k != 0) {
                out += L", ";
            }
            out += h.codes[k];
        }
    } else if (!h.sep.empty()) {
        out += h.sep;
    }
    out += h.trailing;
    return out;
}

void BlockDocument::setText(const std::wstring& text)
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
        Horario h;
        if (parseLine(line, h)) {
            l.kind = Line::Kind::Horario;
            l.horario = std::move(h);
        } else {
            l.kind = Line::Kind::Raw;
            l.raw = std::move(line);
        }
        m_lines.push_back(std::move(l));

        start = next;
    }
}

std::wstring BlockDocument::text() const
{
    std::wstring out;
    bool first = true;
    for (const Line& l : m_lines) {
        if (!first) {
            out += m_eol;
        }
        first = false;
        if (l.kind == Line::Kind::Horario) {
            out += horarioText(l.horario);
        } else {
            out += l.raw;
        }
    }
    if (m_hasTrailingEol) {
        out += m_eol;
    }
    return out;
}

int BlockDocument::addTime(const std::wstring& hhmm)
{
    const int minutes = parseTime(hhmm);
    if (minutes < 0) {
        return -1;
    }
    for (const Line& l : m_lines) {
        if (l.kind == Line::Kind::Horario &&
            parseTime(l.horario.time) == minutes) {
            return -1; // duplicado
        }
    }
    const size_t idx = insertionIndex(minutes);
    Line l;
    l.kind = Line::Kind::Horario;
    l.horario.time = formatTime(minutes);
    l.horario.sep = L" ";
    m_lines.insert(m_lines.begin() + idx, std::move(l));
    return static_cast<int>(idx);
}

bool BlockDocument::addCode(int lineIndex, const std::wstring& code)
{
    if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= m_lines.size()) {
        return false;
    }
    Line& l = m_lines[static_cast<size_t>(lineIndex)];
    if (l.kind != Line::Kind::Horario) {
        return false;
    }
    std::wstring clean = code;
    trimWs(clean);
    if (clean.empty()) {
        return false; // só o código em branco é recusado
    }
    // O MESMO código pode entrar várias vezes no mesmo horário: é o que os
    // arquivos reais fazem (no Mapa.txt oficial o COMER aparece cinco vezes na
    // primeira linha). A restrição de "um por vez" vale para os PARÂMETROS dos
    // relógios (relogio::RelogioDocument::addParam), não para os códigos.
    Horario& h = l.horario;
    if (h.codes.empty()) {
        // Primeiro código do horário: garante o separador do formato real.
        if (h.sep.empty()) {
            h.sep = L" ";
        }
    }
    h.codes.push_back(std::move(clean));
    return true;
}

void BlockDocument::removeCode(int lineIndex, int codeIndex)
{
    if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= m_lines.size()) {
        return;
    }
    Line& l = m_lines[static_cast<size_t>(lineIndex)];
    if (l.kind != Line::Kind::Horario) {
        return;
    }
    Horario& h = l.horario;
    if (codeIndex < 0 || static_cast<size_t>(codeIndex) >= h.codes.size()) {
        return;
    }
    h.codes.erase(h.codes.begin() + codeIndex);
    if (h.codes.empty()) {
        // O horário fica só com o separador ("00:00 "), como nos arquivos
        // reais sem códigos atribuídos; a vírgula final pertence à lista.
        h.trailing.clear();
    }
}

void BlockDocument::replaceCodes(int lineIndex,
                                 const std::vector<std::wstring>& codes)
{
    if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= m_lines.size()) {
        return;
    }
    Line& l = m_lines[static_cast<size_t>(lineIndex)];
    if (l.kind != Line::Kind::Horario) {
        return;
    }
    Horario& h = l.horario;
    h.codes.clear();
    // A lista é gravada VERBATIM: códigos repetidos fazem parte do formato
    // real (copiar/colar de uma linha do Mapa tem de trazer o COMER cinco
    // vezes, não uma).
    for (const std::wstring& c : codes) {
        std::wstring clean = c;
        trimWs(clean);
        if (clean.empty()) {
            continue; // código em branco não entra
        }
        h.codes.push_back(std::move(clean));
    }
    if (!h.codes.empty() && h.sep.empty()) {
        h.sep = L" ";
    }
    if (h.codes.empty()) {
        h.trailing.clear();
    }
}

bool BlockDocument::removeLine(int lineIndex)
{
    if (lineIndex < 0 || static_cast<size_t>(lineIndex) >= m_lines.size()) {
        return false;
    }
    m_lines.erase(m_lines.begin() + lineIndex);
    return true;
}

void BlockDocument::reschedule(const std::vector<std::wstring>& hhmmList)
{
    // Mantém apenas as linhas cruas (comentários, em branco, desconhecidas) na
    // ordem original e descarta os horários atuais.
    std::vector<Line> kept;
    for (Line& l : m_lines) {
        if (l.kind == Line::Kind::Raw) {
            kept.push_back(std::move(l));
        }
    }

    std::vector<Line> times;
    for (const std::wstring& h : hhmmList) {
        const int minute = parseTime(h);
        if (minute < 0) {
            continue;
        }
        bool duplicate = false;
        for (const Line& t : times) {
            if (parseTime(t.horario.time) == minute) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        Line l;
        l.kind = Line::Kind::Horario;
        l.horario.time = formatTime(minute);
        l.horario.sep = L" ";
        times.push_back(std::move(l));
    }
    std::sort(times.begin(), times.end(), [](const Line& a, const Line& b) {
        return parseTime(a.horario.time) < parseTime(b.horario.time);
    });

    m_lines.clear();
    m_lines.insert(m_lines.end(), kept.begin(), kept.end());
    m_lines.insert(m_lines.end(), times.begin(), times.end());
}

void BlockDocument::replaceFrom(const BlockDocument& other)
{
    m_lines = other.m_lines;
    m_eol = other.m_eol;
    m_hasTrailingEol = other.m_hasTrailingEol;
}

size_t BlockDocument::insertionIndex(int minutes) const
{
    for (size_t i = 0; i < m_lines.size(); ++i) {
        const Line& l = m_lines[i];
        if (l.kind == Line::Kind::Horario &&
            parseTime(l.horario.time) >= minutes) {
            return i;
        }
    }
    return m_lines.size();
}

} // namespace blocos
