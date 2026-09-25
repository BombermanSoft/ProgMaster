#include "readconf/PlaylistIniDocument.h"

#include <algorithm>
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

// Remove acentos comuns do português. Usado na NORMALIZAÇÃO de nomes de
// seção: o arquivo real costuma gravar "RELÓGIO COMERCIAL" (acentuado) e o
// programa compara pelas grafias sem acento. O texto original não muda
// (rawSectionName é preservado na serialização).
wchar_t foldAccent(wchar_t c)
{
    static const wchar_t from[] =
        L"ÀÁÂÃÄÅÇÈÉÊËÌÍÎÏÑÒÓÔÕÖÙÚÛÜÝàáâãäåçèéêëìíîïñòóôõöùúûüý";
    static const wchar_t to[] =
        L"AAAAAACEEEEIIIINOOOOOUUUUYaaaaaaceeeeiiiinooooouuuuy";
    const wchar_t* p = from;
    while (*p != L'\0') {
        if (*p == c) {
            return to[p - from];
        }
        ++p;
    }
    return c;
}

std::wstring foldAscii(const std::wstring& s)
{
    std::wstring r = s;
    for (wchar_t& c : r) {
        c = foldAccent(c);
    }
    return r;
}

// Ordem canônica das seções do playlist.ini (pedido do usuário):
// [BLOCO MUSICAL], [RELÓGIO MUSICAL], [BLOCO COMERCIAL], [RELOGIO COMERCIAL],
// [AFILIADAS]. A ordem vale mesmo com seções ausentes — uma seção adicionada
// depois entra no seu lugar correspondente. Devolve -1 para seções
// desconhecidas (que são preservadas, sem reordenar entre si). Recebe o nome
// já NORMALIZADO (minúsculas, acentos dobrados) — ver normalizedSection().
int canonicalSectionRank(const std::wstring& normalizedName)
{
    if (normalizedName == L"bloco musical")     return 1;
    if (normalizedName == L"relogio musical")   return 2;
    if (normalizedName == L"bloco comercial")   return 3;
    if (normalizedName == L"relogio comercial") return 4;
    if (normalizedName == L"afiliadas")         return 5;
    return -1;
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
    // Tolerância: valores entre aspas ("MAPAS\Mapa.txt") são lidos sem as
    // aspas. A linha ORIGINAL (raw) é preservada; se a linha for reescrita,
    // o valor é normalizado sem aspas.
    if (line.value.size() >= 2 && line.value.front() == L'"' &&
        line.value.back() == L'"') {
        line.value = line.value.substr(1, line.value.size() - 2);
    }
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
                line.sectionName = normalizedSection(name);
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
    // ----------------------------------------------------------------
    // Ordem canônica (consulte canonicalSectionRank): a serialização ordena
    // os blocos das seções CONHECIDAS na ordem fixa, mesmo que o arquivo no
    // disco esteja em outra ordem; as seções desconhecidas são preservadas
    // DEPOIS, na ordem original, junto com o seu conteúdo. Se não há nenhuma
    // seção conhecida, a serialização é fiel (nenhum reordenamento).
    // ----------------------------------------------------------------
    struct SectionBlock {
        int begin;
        int end;
        int rank; // 1..5 conhecidas; -1 desconhecida
    };
    std::vector<SectionBlock> blocks;
    int firstHeader = -1;
    for (int i = 0; i < static_cast<int>(m_lines.size()); ++i) {
        if (m_lines[static_cast<size_t>(i)].kind == IniLineKind::Section) {
            if (firstHeader < 0) {
                firstHeader = i;
            }
            const int end = sectionEnd(i);
            const int rank = canonicalSectionRank(
                m_lines[static_cast<size_t>(i)].sectionName);
            blocks.push_back({ i, end, rank });
            i = end - 1; // pula o corpo do bloco
        }
    }

    bool anyKnown = false;
    for (const SectionBlock& b : blocks) {
        if (b.rank >= 1) {
            anyKnown = true;
            break;
        }
    }

    std::wstring out;
    auto appendRange = [this, &out](int begin, int end) {
        for (int i = begin; i < end; ++i) {
            out += m_lines[static_cast<size_t>(i)].raw;
            out += m_eol;
        }
    };

    if (!anyKnown) {
        appendRange(0, static_cast<int>(m_lines.size()));
    } else {
        appendRange(0, firstHeader);
        std::stable_sort(
            blocks.begin(), blocks.end(),
            [](const SectionBlock& a, const SectionBlock& b) {
                if (a.rank == b.rank) {
                    return false; // mantém a ordem original
                }
                if (a.rank < 0) {
                    return false; // desconhecidas depois das conhecidas
                }
                if (b.rank < 0) {
                    return true;
                }
                return a.rank < b.rank;
            });
        for (const SectionBlock& b : blocks) {
            appendRange(b.begin, b.end);
        }
    }

    if (!m_hasTrailingEol && !m_lines.empty()) {
        // Remove o EOL final adicionado acima.
        out.resize(out.size() - m_eol.size());
    }
    return out;
}

std::wstring PlaylistIniDocument::normalizedSection(const std::wstring& rawName)
{
    // Primeiro remove os acentos (fold de ambas as grafias), DEPOIS minúscula:
    // no locale "C" o towlower não abaixa "Ó" por exemplo, e o fold deixaria
    // "O" maiúsculo, divergindo da consulta.
    return toLower(foldAscii(trim(rawName)));
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
    // Insere uma nova chave logo após o cabeçalho da seção — a PRIMEIRA
    // adicionada fica adjacente ao cabeçalho; as seguintes são encadeadas na
    // ordem de chegada (FORMATO é sempre aplicado antes de ARQUIVO).
    IniLine line;
    line.kind = IniLineKind::Key;
    line.rawKey = rawKeyName;
    line.key = normalizedKey;
    line.value = value;
    line.raw = rebuildKeyLine(line);

    int insertAt = sectionIdx + 1;
    // Acha a última chave ativa já existente da seção para encadear depois
    // dela (assim FORMATO vem antes de ARQUIVO, na ordem aplicada).
    const int end = sectionEnd(sectionIdx);
    for (int i = sectionIdx + 1; i < end; ++i) {
        if (m_lines[static_cast<size_t>(i)].kind == IniLineKind::Key &&
            !m_lines[static_cast<size_t>(i)].disabled) {
            insertAt = i + 1;
        }
    }
    if (insertAt < static_cast<int>(m_lines.size())) {
        m_lines.insert(m_lines.begin() + insertAt, line);
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

    // Posição canônica (ver canonicalSectionRank): a seção entra DEPOIS da
    // última seção conhecida de rank menor presente; senão ANTES da primeira
    // de rank maior; senão no fim do documento.
    const int rank = canonicalSectionRank(normalizedSection(sectionNameFor(scope)));
    int insertAt = -1;
    if (rank >= 1) {
        int lowerRankEnd = -1;
        int higherRankHeader = -1;
        for (size_t i = 0; i < m_lines.size(); ++i) {
            if (m_lines[i].kind == IniLineKind::Section) {
                const int r = canonicalSectionRank(m_lines[i].sectionName);
                if (r < 0) {
                    continue;
                }
                if (r < rank) {
                    lowerRankEnd = sectionEnd(static_cast<int>(i));
                } else if (r > rank && higherRankHeader < 0) {
                    higherRankHeader = static_cast<int>(i);
                }
            }
        }
        if (lowerRankEnd >= 0) {
            insertAt = lowerRankEnd;
        } else if (higherRankHeader >= 0) {
            insertAt = higherRankHeader;
        }
    }
    if (insertAt < 0) {
        insertAt = static_cast<int>(m_lines.size());
    }

    // Separador em branco antes do cabeçalho, se necessário.
    if (insertAt > 0 &&
        m_lines[static_cast<size_t>(insertAt - 1)].kind != IniLineKind::Blank) {
        IniLine blank;
        m_lines.insert(m_lines.begin() + insertAt, blank);
        ++insertAt;
    }

    IniLine header;
    header.kind = IniLineKind::Section;
    header.sectionName = normalizedSection(sectionNameFor(scope));
    header.rawSectionName = sectionNameFor(scope);
    header.raw = L"[" + sectionNameFor(scope) + L"]";
    m_lines.insert(m_lines.begin() + insertAt, header);
    return insertAt;
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

bool PlaylistIniDocument::removeSection(ConfigScope scope)
{
    const int sec = sectionIndex(scope);
    if (sec < 0) {
        return false;
    }
    const int end = sectionEnd(sec);
    m_lines.erase(m_lines.begin() + sec,
                  m_lines.begin() + static_cast<ptrdiff_t>(end));
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
        // Toda CHAVE=VALOR dentro de [AFILIADAS] é uma afiliada; a chave é o
        // NOME configurável (o exemplo do manual usa "AFILIADA", mas qualquer
        // nome funciona — "TESTE = 192.168.0.2:3030").
        if (line.kind == IniLineKind::Key) {
            Afiliada a;
            a.disabled = line.disabled;
            a.name = line.rawKey.empty() ? line.key : line.rawKey;
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

void PlaylistIniDocument::addAfiliada(const std::wstring& name,
                                      const std::wstring& address,
                                      const std::wstring& portText,
                                      bool disabled)
{
    IniLine line;
    line.kind = IniLineKind::Key;
    line.rawKey = name.empty() ? L"AFILIADA" : name;
    line.key = toLower(name.empty() ? L"AFILIADA" : name);
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
                                         const std::wstring& name,
                                         const std::wstring& address,
                                         const std::wstring& portText,
                                         bool disabled)
{
    const std::vector<Afiliada> list = afiliadas();
    if (position >= list.size()) {
        return;
    }
    IniLine& line = m_lines[static_cast<size_t>(list[position].lineIndex)];
    line.rawKey = name.empty() ? L"AFILIADA" : name;
    line.key = toLower(name.empty() ? L"AFILIADA" : name);
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