#include "playlist/PlaylistLocator.h"

#include <windows.h>

#include <filesystem>

#include <cwctype>

#include "core/Log.h"
#include "core/Settings.h"

namespace {

// Verifica se o caminho é um arquivo real no disco.
bool isFileOnDisk(const std::filesystem::path& path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool icaseEquals(const std::wstring& a, const std::wstring& b)
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

// Exige que o arquivo seja exatamente o "Playlist.exe" (ignorando maiúsculas).
bool isPlaylistExeName(const std::wstring& fileName)
{
    return icaseEquals(fileName, L"Playlist.exe");
}

} // namespace

PlaylistLocator::PlaylistLocator(Settings& settings)
    : m_settings(settings)
{
}

bool PlaylistLocator::tryLoadSavedPath()
{
    m_settings.load();
    if (!m_settings.hasPlaylistExePath()) {
        return false;
    }

    if (!isFileOnDisk(m_settings.getPlaylistExePath())) {
        Log::info(L"Caminho salvo deixou de existir: " + m_settings.getPlaylistExePath());
        m_settings.setPlaylistExePath(L"");
        m_settings.save();
        return false;
    }

    m_exePath = m_settings.getPlaylistExePath();
    Log::info(L"Playlist.exe recuperado do caminho salvo: " + m_exePath);
    return true;
}

bool PlaylistLocator::validatePath(const std::wstring& path, std::wstring& userMessage) const
{
    if (path.empty()) {
        userMessage = L"Informe o caminho do Playlist.exe.";
        return false;
    }

    const std::filesystem::path candidate(path);
    if (!isFileOnDisk(candidate)) {
        userMessage = L"O arquivo informado não existe ou não é um arquivo válido.";
        return false;
    }

    if (!isPlaylistExeName(candidate.filename().wstring())) {
        userMessage = L"O arquivo selecionado não é o Playlist.exe. "
                      L"Selecione exatamente o arquivo Playlist.exe da instalação.";
        return false;
    }
    return true;
}

bool PlaylistLocator::setPlaylistExePath(const std::wstring& path, std::wstring& userMessage)
{
    if (!validatePath(path, userMessage)) {
        return false;
    }

    m_exePath = path;
    m_settings.setPlaylistExePath(path);
    m_settings.save();
    Log::info(L"Playlist.exe definido: " + m_exePath);
    return true;
}

void PlaylistLocator::clear()
{
    m_exePath.clear();
    m_settings.setPlaylistExePath(L"");
    m_settings.save();
}

bool PlaylistLocator::hasValidInstallation() const
{
    if (m_exePath.empty()) {
        return false;
    }
    return isFileOnDisk(m_exePath);
}

const std::wstring& PlaylistLocator::getPlaylistExePath() const
{
    return m_exePath;
}