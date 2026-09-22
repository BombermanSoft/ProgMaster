#pragma once

#include <windows.h>

class PlaylistLocator;

// Diálogo inicial/alternativo para localizar o Playlist.exe.
//
// Permite digitar o caminho diretamente ou usar o seletor de arquivos,
// exibe o caminho escolhido e solicita a confirmação antes de salvar.
namespace LocatePlaylistDialog {

// Abre o diálogo modal sobre a janela "owner".
// Retorna true somente se o usuário confirmou um caminho válido
// (nesse caso o caminho já foi validado e salvo pelo PlaylistLocator).
bool show(HWND owner, HINSTANCE hInstance, PlaylistLocator& locator);

} // namespace LocatePlaylistDialog