#pragma once

#include <filesystem>
#include <string>

// Guarda as preferências persistentes do programa.
// Neste momento a única preferência é o caminho do Playlist.exe.
//
// Os dados ficam em %APPDATA%\ProgMaster\config.txt (UTF-8), fora da pasta
// do executável: isso evita depender de arquivos ao lado do .exe e mantém a
// direção de distribuição como um único binário.
class Settings {
public:
    // Pasta de dados da aplicação (%APPDATA%\ProgMaster).
    static std::filesystem::path getAppDataDir();

    // Caminho completo do arquivo de configuração.
    static std::filesystem::path getConfigFilePath();

    // Lê o arquivo de configuração do disco (com migração da pasta antiga
    // %APPDATA%\PlaylistComplementar, usada antes do rebranding).
    void load();

    // Grava o arquivo de configuração no disco.
    void save() const;

    bool hasPlaylistExePath() const;
    std::wstring getPlaylistExePath() const;
    void setPlaylistExePath(const std::wstring& path);

private:
    // Pasta de dados da versão legada (antes do ProgMaster).
    static std::filesystem::path legacyAppDataDir();

    // Extrai o valor da chave PlaylistExePath de um arquivo no formato
    // "Chave=valor" (vazio se não houver).
    static std::wstring readPlaylistExePath(const std::filesystem::path& file);

    std::wstring m_playlistExePath;
};