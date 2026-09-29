#include "blocos/CodeCatalogue.h"

#include <algorithm>
#include <cwctype>

#include "playlist/FoldersXml.h"

namespace blocos {
namespace {

bool codeEqualsCi(const std::wstring& a, const std::wstring& b)
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

void trimWs(std::wstring& s)
{
    while (!s.empty() && (s.front() == L' ' || s.front() == L'\t')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == L' ' || s.back() == L'\t')) {
        s.pop_back();
    }
}

} // namespace

bool CodeCatalogue::isValidCode(const std::wstring& code, std::wstring& error)
{
    if (code.empty()) {
        error = L"Digite um código.";
        return false;
    }
    if (code.size() > 16) {
        error = L"O código deve ter no máximo 16 caracteres.";
        return false;
    }
    for (const wchar_t c : code) {
        const bool letter =
            (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
        const bool digit = (c >= L'0' && c <= L'9');
        if (!letter && !digit && c != L'_') {
            error = L"O código aceita apenas letras, números e underscore (_).";
            return false;
        }
    }
    return true;
}

std::wstring CodeCatalogue::loadFromFoldersXml(
    const std::filesystem::path& foldersXmlPath, std::string& technicalError)
{
    // Os códigos de sessão do usuário sobrevivem a um novo carregamento.
    std::vector<CodeEntry> session;
    for (const CodeEntry& e : m_entries) {
        if (e.sessionOnly) {
            session.push_back(e);
        }
    }

    std::vector<FolderEntry> fromFile;
    const std::wstring message =
        FoldersXml::readAll(foldersXmlPath, fromFile, technicalError);
    if (!message.empty()) {
        return message; // mantém o conteúdo anterior intacto
    }

    std::vector<CodeEntry> fresh;
    fresh.reserve(fromFile.size() + session.size());
    for (const FolderEntry& e : fromFile) {
        CodeEntry entry;
        entry.code = e.dbfId;
        trimWs(entry.code);
        entry.title = e.title;
        trimWs(entry.title);
        if (entry.code.empty()) {
            continue; // registro sem DBFId não é um código utilizável
        }
        if (indexOfIn(fresh, entry.code) >= 0) {
            continue; // o mesmo DBFId repetido não gera botão duplicado
        }
        fresh.push_back(std::move(entry));
    }
    for (CodeEntry& e : session) {
        if (indexOfIn(fresh, e.code) < 0) {
            fresh.push_back(std::move(e));
        }
    }

    m_entries = std::move(fresh);
    return std::wstring();
}

bool CodeCatalogue::addSessionCode(const std::wstring& code,
                                   const std::wstring& title, std::wstring& error)
{
    std::wstring clean = code;
    trimWs(clean);
    if (!isValidCode(clean, error)) {
        return false;
    }
    if (contains(clean)) {
        error = L"O código " + clean + L" já existe.";
        return false;
    }
    CodeEntry entry;
    entry.code = clean;
    entry.title = title;
    trimWs(entry.title);
    entry.sessionOnly = true;
    m_entries.push_back(std::move(entry));
    return true;
}

bool CodeCatalogue::removeSessionCode(const std::wstring& code)
{
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].sessionOnly && codeEqualsCi(m_entries[i].code, code)) {
            m_entries.erase(m_entries.begin() + static_cast<long>(i));
            return true;
        }
    }
    return false; // códigos do arquivo não são removidos
}

int CodeCatalogue::indexOf(const std::wstring& code) const
{
    return indexOfIn(m_entries, code);
}

bool CodeCatalogue::contains(const std::wstring& code) const
{
    return indexOfIn(m_entries, code) >= 0;
}

int CodeCatalogue::indexOfIn(const std::vector<CodeEntry>& list,
                             const std::wstring& code)
{
    for (size_t i = 0; i < list.size(); ++i) {
        if (codeEqualsCi(list[i].code, code)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace blocos
