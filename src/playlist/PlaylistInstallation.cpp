#include "playlist/PlaylistInstallation.h"

#include <windows.h>

#include <algorithm>

#include <cwctype>

#include "core/Log.h"

namespace {

bool existsOnDisk(const std::filesystem::path& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

} // namespace

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
    if (m_exePath.empty()) {
        return {};
    }

    const std::filesystem::path root = installFolder();
    const std::filesystem::path exeDir = m_exePath.parent_path();

    // Candidatas em ordem de prioridade: exeDir primeiro (valor documentado:
    // o ini fica na MESMA pasta do Playlist.exe), depois a raiz da instalação.
    // Ex.: C:\Playlist\pgm\Playlist.exe + playlist.ini na mesma pasta.
    std::vector<std::filesystem::path> candidates;
    const auto pushIfNew = [&candidates](const std::filesystem::path& p) {
        const auto it = std::find(candidates.begin(), candidates.end(), p);
        if (it == candidates.end()) {
            candidates.push_back(p);
        }
    };
    pushIfNew(exeDir / L"playlist.ini");
    pushIfNew(exeDir / L"Playlist.ini");
    pushIfNew(root / L"playlist.ini");

    // Fallback defensivo: sobe na árvore a partir da pasta do exe procurando
    // o playlist.ini (ex.: exe em C:\Playlist\sistema\pgm e ini em
    // C:\Playlist). Para nunca varrer o disco inteiro, paramos no pai do
    // exeDir (primeiro nível acima basta para as instalações reais).
    if (exeDir != exeDir.parent_path()) {
        pushIfNew(exeDir.parent_path() / L"playlist.ini");
    }

    for (const auto& c : candidates) {
        Log::info(L"playlistIniPath: testando [" + c.wstring() + L"]");
    }

    const std::filesystem::path found = firstExisting(candidates);
    if (found.empty()) {
        Log::info(L"playlistIniPath: nenhum playlist.ini encontrado "
                  L"(exeDir=" + exeDir.wstring() + L")");
    } else {
        Log::info(L"playlistIniPath: encontrado [" + found.wstring() + L"]");
    }
    return found;
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

    const std::filesystem::path found = firstExisting({
        // Variantes na pasta do exe (comum em instalações pgm)
        exeDir / L"mapas" / L"mapa.txt",
        exeDir / L"mapas" / L"Mapas.txt",
        exeDir / L"mapa"  / L"mapa.txt",
        // Variantes na raiz da instalação
        root  / L"mapas" / L"mapa.txt",
        root  / L"mapas" / L"Mapas.txt",
        root  / L"mapa"  / L"mapa.txt",
    });
    if (!found.empty()) return found;
    return exeDir / L"mapas" / L"mapa.txt";
}

std::filesystem::path PlaylistInstallation::gradesTxtPath() const
{
    if (m_exePath.empty()) return {};
    const std::filesystem::path exeDir = m_exePath.parent_path();
    const std::filesystem::path root  = installFolder();

    const std::filesystem::path found = firstExisting({
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
    });
    if (!found.empty()) return found;
    return exeDir / L"grades" / L"grade.txt";
}

std::filesystem::path PlaylistInstallation::firstExisting(
    const std::vector<std::filesystem::path>& candidates)
{
    for (const std::filesystem::path& candidate : candidates) {
        if (existsOnDisk(candidate)) {
            return candidate;
        }
    }
    return std::filesystem::path();
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