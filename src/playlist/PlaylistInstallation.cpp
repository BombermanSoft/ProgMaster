#include "playlist/PlaylistInstallation.h"

#include <algorithm>

#include <cwctype>

#include "core/FileSystem.h"

PlaylistInstallation::PlaylistInstallation(std::filesystem::path exePath)
    : m_exePath(std::move(exePath))
{
}

void PlaylistInstallation::setExecutablePath(std::filesystem::path exePath)
{
    m_exePath = std::move(exePath);
}

const std::filesystem::path& PlaylistInstallation::executablePath() const
{
    return m_exePath;
}

std::filesystem::path PlaylistInstallation::installFolder() const
{
    const std::filesystem::path exeDir = m_exePath.parent_path();

    // Regra de descoberta (heurística): se o executável estiver numa pasta
    // chamada "pgm" (qualquer maiúsculas/minúsculas), a raiz da instalação é
    // a pasta pai. Caso contrário, considera a própria pasta do exe.
    // (Exemplo: C:\Playlist\pgm\Playlist.exe -> C:\Playlist)
    if (icaseFolderNameEquals(exeDir, L"pgm")) {
        return exeDir.parent_path();
    }
    return exeDir;
}

std::filesystem::path PlaylistInstallation::playlistIniPath() const
{
    const std::filesystem::path root = installFolder();
    const std::filesystem::path exeDir = m_exePath.parent_path();
    return firstExisting({ root / L"playlist.ini", exeDir / L"playlist.ini" });
}

std::filesystem::path PlaylistInstallation::foldersXmlPath() const
{
    const std::filesystem::path root = installFolder();
    const std::filesystem::path exeDir = m_exePath.parent_path();
    return firstExisting({ root / L"folders.xml", exeDir / L"folders.xml" });
}

std::filesystem::path PlaylistInstallation::configManagerPath() const
{
    const std::filesystem::path root = installFolder();
    const std::filesystem::path exeDir = m_exePath.parent_path();

    // Conforme o padrão documentado, o ConfigManager.exe fica em
    // <instalação>\Pgm\ConfigManager.exe. As demais candidatas são alternativas
    // defensivas para instalações com disposição diferente.
    return firstExisting({
        root / L"Pgm" / L"ConfigManager.exe",
        root / L"ConfigManager.exe",
        exeDir / L"ConfigManager.exe",
    });
}

std::filesystem::path PlaylistInstallation::mapasTxtPath() const
{
    // Busca flexível entre variantes de nome de pasta e arquivo. Se não
    // encontrar nenhum no disco, retorna o caminho-alvo canônico para
    // criação ao salvar.
    if (m_exePath.empty()) return {};
    const std::filesystem::path exeDir = m_exePath.parent_path();
    const std::filesystem::path root  = installFolder();

    return firstExisting({
        // Variantes na pasta do exe (comum em instalações pgm)
        exeDir / L"mapas" / L"mapa.txt",
        exeDir / L"mapas" / L"Mapas.txt",
        exeDir / L"mapa"  / L"mapa.txt",
        // Variantes na raiz da instalação
        root  / L"mapas" / L"mapa.txt",
        root  / L"mapas" / L"Mapas.txt",
        root  / L"mapa"  / L"mapa.txt",
    }, exeDir / L"mapas" / L"mapa.txt");
}

std::filesystem::path PlaylistInstallation::gradesTxtPath() const
{
    if (m_exePath.empty()) return {};
    const std::filesystem::path exeDir = m_exePath.parent_path();
    const std::filesystem::path root  = installFolder();

    return firstExisting({
        // Pasta "grades" + nomes de arquivo variados
        exeDir / L"grades" / L"grade.txt",
        exeDir / L"grades" / L"grades.txt",
        exeDir / L"grade"  / L"grade.txt",
        exeDir / L"grade"  / L"grades.txt",
        // Pasta "Grades" (case-insensitive; mantida por clareza)
        exeDir / L"Grades" / L"grade.txt",
        exeDir / L"Grades" / L"grades.txt",
        // Variantes na raiz
        root  / L"grades" / L"grade.txt",
        root  / L"grades" / L"grades.txt",
    }, exeDir / L"grades" / L"grade.txt");
}

std::filesystem::path PlaylistInstallation::firstExisting(
    const std::vector<std::filesystem::path>& candidates,
    const std::filesystem::path& fallback)
{
    for (const std::filesystem::path& candidate : candidates) {
        if (FileSystem::pathExists(candidate)) {
            return candidate;
        }
    }
    return fallback;
}

bool PlaylistInstallation::icaseFolderNameEquals(const std::filesystem::path& folder,
                                                 const std::wstring& name)
{
    const std::wstring folderName = folder.filename().wstring();
    if (folderName.size() != name.size()) {
        return false;
    }
    return std::equal(folderName.begin(), folderName.end(), name.begin(),
                      [](wchar_t a, wchar_t b) {
                          return std::towlower(a) == std::towlower(b);
                      });
}