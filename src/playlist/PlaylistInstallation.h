#pragma once

#include <filesystem>
#include <vector>

// Deriva a estrutura de pastas da instalação do Playlist a partir do caminho
// do Playlist.exe. Nunca assume um caminho fixo de instalação.
//
// Exemplo documentado:
//   C:\Playlist\pgm\Playlist.exe   ->  instalação em  C:\Playlist
//   C:\Playlist\Pgm\ConfigManager.exe  (dentro da pasta Pgm da instalação)
class PlaylistInstallation {
public:
    PlaylistInstallation() = default;
    explicit PlaylistInstallation(std::filesystem::path exePath);

    void setExecutablePath(std::filesystem::path exePath);
    const std::filesystem::path& executablePath() const;

    // Pasta raiz da instalação do Playlist.
    std::filesystem::path installFolder() const;

    // Caminhos dos arquivos relacionados. Retornam caminho vazio quando o
    // arquivo não for encontrado no disco.
    std::filesystem::path playlistIniPath() const;
    std::filesystem::path foldersXmlPath() const;
    std::filesystem::path configManagerPath() const;

    // Arquivos de programação. A busca é flexível: testa nome de pasta e de
    // arquivo no singular e plural (o usuário relatou variações reais entre
    // "mapa/mapas" e "grade/grades.txt" em instalações diferentes). Retorna
    // o primeiro que existir no disco; se nenhum existir, retorna o
    // caminho-alvo canônico relativo à pasta do exe
    // (pgm\mapas\mapa.txt e pgm\grades\grade.txt) para apoiar a criação
    // do arquivo ao salvar.
    std::filesystem::path mapasTxtPath() const;
    std::filesystem::path gradesTxtPath() const;

private:
    static std::filesystem::path firstExisting(
        const std::vector<std::filesystem::path>& candidates);
    static bool icaseFolderNameEquals(const std::filesystem::path& folder,
                                      const std::wstring& name);

    std::filesystem::path m_exePath;
};