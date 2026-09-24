#include "readconf/PlaylistIniDocument.h"

#include <cwctype>

namespace readconf {
namespace {

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

std::wstring trim(const std::wstring& s)
{
    size_t b = 0;
    while (b < s.size() && (s[b] == L' ' || s[b] == L'\t')) {
        ++b;
    }
    size_t e = s.size();
    while (e > b && (s[e - 1] == L' ' || s[e - 1] == L'\t')) {
        --e;
    }
    return s.substr(b, e - b);
}

std::wstring toLower(const std::wstring& s)
{
    std::wstring r = s;
    for (wchar_t& c : r) {
        c = static_cast<wchar_t>(std::towlower(c));
    }
    return r;
}

bool isBlank(const std::wstring& s)
{
    for (wchar_t c : s) {
        if (c != L' ' && c != L'\t' && c != L'\r') {
            return false;
        }
    }
    return true;
}

// Lê a codificação de fim de linha dominante do arquivo (primeira ocorrência).
std::wstring detectEol(const std::wstring& text)
{
    const size_t crlf = text.find(L"\r\n");
    const size_t lf = text.find(L'\n');
    const size_t cr = text.find(L'\r');
    if (crlf != std::wstring::npos) {
        return L"\r\n";
    }
    if (lf != std::wstring::npos) {
        return L"\n";
    }
    if (cr != std::wstring::npos) {
        return L"\r";
    }
    return L"\r\n";
}

// Constrói o campo "value" de linhas CHAVE=VALOR já existentes, preservando o
// nome e o valor originais quanto possível na REGENERAÇÃO posterior.
IniLine parseKeyLine(const std::wstring& raw)
{
    IniLine line;
    line.raw = raw;

    std::wstring work = raw;
    bool disabled = false;
    if (!work.empty() && (work[0] == L';' || work[0] == L'#')) {
        disabled = true;
        work = trim(work.substr(1));
    } else {
        work = trim(work);
    }

    const size_t eq = work.find(L'=');
    if (eq == std::wstring::npos || eq == 0) {
        line.kind = IniLineKind::Comment;
        return line;
    }

    line.kind = IniLineKind::Key;
    line.disabled = disabled;
    line.rawKey = trim(work.substr(0, eq));
    line.key = toLower(line.rawKey);
    line.value = trim(work.substr(eq + 1));
    return line;
}

// Reconstrói o texto CHAVE=VALOR a partir de campos, colocando à frente o
// prefixo de desativação quando a linha for "comentada".
std::wstring rebuildKeyLine(const IniLine& line)
{
    std::wstring value = line.value;
    // O valor pode conter o separador ':' (afiliadas); nada a fazer aqui.
    std::wstring text;
    if (line.disabled) {
        text = L";";
    }
    text += line.rawKey.empty() ? line.key : line.rawKey;
    text += L"=";
    text += value;
    return text;
}

} // namespace

void PlaylistIniDocument::setText(const std::wstring& text)
{
    m_lines.clear();
    m_eol = detectEol(text);
    m_hasTrailingEol = !text.empty() &&
                       (text.back() == L'\n' || text.back() == L'\r');

    // Quebra as linhas preservando o conteúdo (sem o EOL).
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find(L'\n', pos);
        size_t len;
        if (end == std::wstring::npos) {
            len = text.size() - pos;
        } else {
            len = end - pos;
            if (len > 0 && text[end - 1] == L'\r') {
                --len; // remove \r do fim (CRLF)
            }
        }
        parseLine(text.substr(pos, len));
        pos = (end == std::wstring::npos) ? text.size() : end + 1;
    }
}

void PlaylistIniDocument::parseLine(const std::wstring& raw)
{
    IniLine line;
    line.raw = raw;

    if (isBlank(raw)) {
        line.kind = IniLineKind::Blank;
        m_lines.push_back(line);
        return;
    }

    // Tenta identificar seção: primeiro caractere não-branco é "[".
    const std::wstring trimmed = trim(raw);
    if (!trimmed.empty() && trimmed[0] == L'[') {
        const size_t close = trimmed.find(L']');
        if (close != std::wstring::npos) {
            const std::wstring name = trim(trimmed.substr(1, close - 1));
            if (!name.empty()) {
                line.kind = IniLineKind::Section;
                line.sectionName = toLower(name);
                line.rawSectionName = name;
                m_lines.push_back(line);
                return;
            }
        }
    }

    // CHAVE=VALOR (ou ;CHAVE=VALOR desativada, ou comentário).
    IniLine keyLine = parseKeyLine(raw);
    if (keyLine.kind == IniLineKind::Key) {
        m_lines.push_back(keyLine);
        return;
    }

    // Comentário puro.
    line.kind = IniLineKind::Comment;
    m_lines.push_back(line);
}

std::wstring PlaylistIniDocument::text() const
{
    std::wstring out;
    for (const IniLine& line : m_lines) {
        out += line.raw;
        out += m_eol;
    }
    if (!m_hasTrailingEol && !m_lines.empty()) {
        // Remove o EOL final adicionado acima.
        out.resize(out.size() - m_eol.size());
    }
    return out;
}

std::wstring PlaylistIniDocument::normalizedSection(const std::wstring& rawName)
{
    return toLower(trim(rawName));
}

int PlaylistIniDocument::findSectionLine(const std::wstring& normalizedName) const
{
    for (size_t i = 0; i < m_lines.size(); ++i) {
        const IniLine& line = m_lines[i];
        if (line.kind == IniLineKind::Section &&
            line.sectionName == normalizedName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int PlaylistIniDocument::sectionIndex(ConfigScope scope) const
{
    return findSectionLine(normalizedSection(sectionNameFor(scope)));
}

int PlaylistIniDocument::sectionEnd(int sectionIdx) const
{
    if (sectionIdx < 0 || static_cast<size_t>(sectionIdx) >= m_lines.size()) {
        return -1;
    }
    for (size_t i = static_cast<size_t>(sectionIdx) + 1; i < m_lines.size(); ++i) {
        if (m_lines[i].kind == IniLineKind::Section) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(m_lines.size());
}

int PlaylistIniDocument::findKeyLine(int sectionIdx,
                                     const std::wstring& normalizedKey) const
{
    if (sectionIdx < 0) {
        return -1;
    }
    const int end = sectionEnd(sectionIdx);
    for (int i = sectionIdx + 1; i < end; ++i) {
        if (m_lines[static_cast<size_t>(i)].kind == IniLineKind::Key &&
            !m_lines[static_cast<size_t>(i)].disabled &&
            m_lines[static_cast<size_t>(i)].key == normalizedKey) {
            return i;
        }
    }
    return -1;
}

bool PlaylistIniDocument::sectionKeyValue(ConfigScope scope,
                                          const std::wstring& normalizedKey,
                                          std::wstring& outValue) const
{
    outValue.clear();
    const int sec = sectionIndex(scope);
    const int key = findKeyLine(sec, normalizedKey);
    if (key < 0) {
        return false;
    }
    outValue = m_lines[static_cast<size_t>(key)].value;
    return true;
}

void PlaylistIniDocument::setKeyValue(int sectionIdx,
                                      const std::wstring& rawKeyName,
                                      const std::wstring& normalizedKey,
                                      const std::wstring& value)
{
    const int keyIdx = findKeyLine(sectionIdx, normalizedKey);
    if (keyIdx >= 0) {
        IniLine& line = m_lines[static_cast<size_t>(keyIdx)];
        line.value = value;
        line.raw = rebuildKeyLine(line);
        return;
    }
    insertKeyAfterHeader(sectionIdx, rawKeyName, normalizedKey, value);
}

void PlaylistIniDocument::insertKeyAfterHeader(int sectionIdx,
                                               const std::wstring& rawKeyName,
                                               const std::wstring& normalizedKey,
                                               const std::wstring& value)
{
    // Insere uma nova chave logo após o cabeçalho da seção.
    IniLine line;
    line.kind = IniLineKind::Key;
    line.rawKey = rawKeyName;
    line.key = normalizedKey;
    line.value = value;
    line.raw = rebuildKeyLine(line);
    if (sectionIdx + 1 < static_cast<int>(m_lines.size())) {
        m_lines.insert(m_lines.begin() + (sectionIdx + 1), line);
    } else {
        m_lines.push_back(line);
    }
}

void PlaylistIniDocument::eraseLineAt(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < m_lines.size()) {
        m_lines.erase(m_lines.begin() + index);
    }
}

int PlaylistIniDocument::ensureSection(ConfigScope scope)
{
    const int existing = sectionIndex(scope);
    if (existing >= 0) {
        return existing;
    }

    // Adiciona a seção no fim do documento.
    if (!m_lines.empty() && m_lines.back().kind != IniLineKind::Blank) {
        IniLine blank;
        m_lines.push_back(blank);
    }
    IniLine header;
    header.kind = IniLineKind::Section;
    header.sectionName = normalizedSection(sectionNameFor(scope));
    header.rawSectionName = sectionNameFor(scope);
    header.raw = L"[" + sectionNameFor(scope) + L"]";
    m_lines.push_back(header);
    return static_cast<int>(m_lines.size()) - 1;
}

bool PlaylistIniDocument::applyFormat(ConfigScope scope, FormatOption option)
{
    const GeneratedFormat gen = generate(scope, option);
    if (gen.lines.empty()) {
        return false; // opção não aplicável ao escopo
    }

    const int sec = ensureSection(scope);

    // A primeira linha gerada é sempre FORMATO=...
    const size_t eq = gen.lines[0].find(L'=');
    if (eq == std::wstring::npos) {
        return false;
    }
    const std::wstring rawKey = gen.lines[0].substr(0, eq);
    const std::wstring value = gen.lines[0].substr(eq + 1);
    setKeyValue(sec, rawKey, toLower(rawKey), value);

    if (gen.removesArquivo) {
        const int arquivoIdx = findKeyLine(sec, L"arquivo");
        eraseLineAt(arquivoIdx);
        return true;
    }

    // Segunda linha: ARQUIVO=...
    if (gen.lines.size() >= 2) {
        const size_t eq2 = gen.lines[1].find(L'=');
        if (eq2 != std::wstring::npos) {
            setKeyValue(sec, gen.lines[1].substr(0, eq2),
                        L"arquivo", gen.lines[1].substr(eq2 + 1));
        }
    }
    return true;
}

std::vector<PlaylistIniDocument::Afiliada> PlaylistIniDocument::afiliadas() const
{
    std::vector<Afiliada> result;
    const int sec = sectionIndex(ConfigScope::Afiliadas);
    if (sec < 0) {
        return result;
    }
    const int end = sectionEnd(sec);
    for (int i = sec + 1; i < end; ++i) {
        const IniLine& line = m_lines[static_cast<size_t>(i)];
        if (line.kind == IniLineKind::Key && line.key == L"afiliada") {
            Afiliada a;
            a.disabled = line.disabled;
            a.rawValue = line.value;
            a.lineIndex = i;
            const size_t colon = line.value.find(L':');
            if (colon != std::wstring::npos) {
                a.address = trim(line.value.substr(0, colon));
                a.portText = trim(line.value.substr(colon + 1));
            } else {
                a.address = trim(line.value);
            }
            result.push_back(a);
        }
    }
    return result;
}

bool PlaylistIniDocument::hasAfiliadasSection() const
{
    return sectionIndex(ConfigScope::Afiliadas) >= 0;
}

bool PlaylistIniDocument::ensureAfiliadasSection()
{
    return ensureSection(ConfigScope::Afiliadas) >= 0;
}

void PlaylistIniDocument::addAfiliada(const std::wstring& address,
                                      const std::wstring& portText,
                                      bool disabled)
{
    IniLine line;
    line.kind = IniLineKind::Key;
    line.rawKey = L"AFILIADA";
    line.key = L"afiliada";
    line.disabled = disabled;
    line.value = address;
    if (!portText.empty()) {
        line.value += L":";
        line.value += portText;
    }
    line.raw = rebuildKeyLine(line);

    const int sec = ensureSection(ConfigScope::Afiliadas);
    const int end = sectionEnd(sec);
    m_lines.insert(m_lines.begin() + end, line);
}

void PlaylistIniDocument::updateAfiliada(size_t position,
                                         const std::wstring& address,
                                         const std::wstring& portText,
                                         bool disabled)
{
    const std::vector<Afiliada> list = afiliadas();
    if (position >= list.size()) {
        return;
    }
    IniLine& line = m_lines[static_cast<size_t>(list[position].lineIndex)];
    line.value = address;
    if (!portText.empty()) {
        line.value += L":";
        line.value += portText;
    }
    line.disabled = disabled;
    line.raw = rebuildKeyLine(line);
}

void PlaylistIniDocument::removeAfiliada(size_t position)
{
    const std::vector<Afiliada> list = afiliadas();
    if (position >= list.size()) {
        return;
    }
    eraseLineAt(list[position].lineIndex);
}

} // namespace readconf