#pragma once

#include <string>

class Settings;

// Responsável por localizar e validar o Playlist.exe e por manter, durante
// a execução, o caminho escolhido pelo usuário (persistido pelo Settings).
class PlaylistLocator {
public:
    explicit PlaylistLocator(Settings& settings);

    // Tenta recuperar o caminho salvo em execuções anteriores.
    // Retorna true se existir um caminho salvo e o arquivo ainda existir.
    bool tryLoadSavedPath();

    // Verifica se o caminho aponta realmente para um Playlist.exe existente.
    // Em caso de invalidez preenche userMessage com uma explicação amigável.
    bool validatePath(const std::wstring& path, std::wstring& userMessage) const;

    // Valida e, se válido, salva o caminho permanentemente.
    bool setPlaylistExePath(const std::wstring& path, std::wstring& userMessage);

    // Esquece o caminho salvo (usado quando o caminho não é mais válido).
    void clear();

    bool hasValidInstallation() const;
    const std::wstring& getPlaylistExePath() const;

private:
    Settings& m_settings;
    std::wstring m_exePath;
};