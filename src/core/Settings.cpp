#include "core/Settings.h"

#include <windows.h>
#include <shlobj.h>

#include "core/TextFileIO.h"

namespace {

// Chave usada no arquivo de configuração ("Chave=valor", uma por linha).
constexpr wchar_t kKeyPlaylistExePath[] = L"PlaylistExePath";

} // namespace

std::filesystem::path Settings::getAppDataDir()
{
    wchar_t buffer[MAX_PATH]{};
    // CSIDL_APPDATA -> %APPDATA% (Roaming). Pasta do usuário, não exige
    // administrador e não polui a pasta do executável.
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer))) {
        return std::filesystem::path(buffer) / L"ProgMaster";
    }
    // Fallback pouco provável: pasta atual.
    return std::filesystem::current_path() / L"ProgMaster";
}

std::filesystem::path Settings::legacyAppDataDir()
{
    wchar_t buffer[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer))) {
        return std::filesystem::path(buffer) / L"PlaylistComplementar";
    }
    return std::filesystem::current_path() / L"PlaylistComplementar";
}

std::filesystem::path Settings::getConfigFilePath()
{
    return getAppDataDir() / L"config.txt";
}

std::wstring Settings::readPlaylistExePath(const std::filesystem::path& file)
{
    const TextFileResult result = TextFileIO::readWide(file);
    if (!result.ok) {
        return L"";
    }

    // Formato simples: "PlaylistExePath=<caminho>" (uma chave por linha).
    size_t pos = 0;
    while (pos <= result.text.size()) {
        const size_t eol = result.text.find(L'\n', pos);
        std::wstring line = result.text.substr(
            pos, (eol == std::wstring::npos ? result.text.size() : eol) - pos);
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        const size_t eq = line.find(L'=');
        if (eq != std::wstring::npos &&
            line.compare(0, eq, kKeyPlaylistExePath) == 0) {
            return line.substr(eq + 1);
        }
        if (eol == std::wstring::npos) {
            break;
        }
        pos = eol + 1;
    }
    return L"";
}

void Settings::load()
{
    m_playlistExePath.clear();

    const std::filesystem::path file = getConfigFilePath();
    if (std::filesystem::exists(file)) {
        m_playlistExePath = readPlaylistExePath(file);
        return;
    }

    // Migração de versões que usavam %APPDATA%\PlaylistComplementar: herda o
    // caminho salvo (se ainda existir) e regrava na pasta nova do ProgMaster.
    const std::filesystem::path legacy = legacyAppDataDir() / L"config.txt";
    if (std::filesystem::exists(legacy)) {
        m_playlistExePath = readPlaylistExePath(legacy);
        if (!m_playlistExePath.empty()) {
            save();
        }
    }
}

void Settings::save() const
{
    CreateDirectoryW(getAppDataDir().c_str(), nullptr);

    const std::wstring content =
        std::wstring(kKeyPlaylistExePath) + L"=" + m_playlistExePath + L"\r\n";
    std::string technicalError;
    TextFileIO::writeWide(getConfigFilePath(), content, TextEncoding::Utf8, technicalError);
}

bool Settings::hasPlaylistExePath() const
{
    return !m_playlistExePath.empty();
}

std::wstring Settings::getPlaylistExePath() const
{
    return m_playlistExePath;
}

void Settings::setPlaylistExePath(const std::wstring& path)
{
    m_playlistExePath = path;
}