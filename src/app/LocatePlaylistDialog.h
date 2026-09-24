#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>

class PlaylistLocator;

namespace app {

// Diálogo modal para localizar o Playlist.exe (Etapa 1, recriado em JUCE).
//
// Exibe um campo de texto com o caminho (iniciado com o valor salvo), um
// botão "Procurar..." que abre o seletor de arquivos (*.exe) e os botões
// Confirmar/Cancelar. Retorna false quando o usuário cancela.
void locatePlaylistExe(juce::Component& parent,
                       const std::wstring& initialPath,
                       std::wstring& outPath,
                       bool& confirmed);

} // namespace app