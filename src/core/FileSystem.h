#pragma once

#include <filesystem>

// Consultas de baixo nível sobre o estado de um caminho no disco.
//
// Responsabilidade única: informar se um caminho existe e se ele é um
// arquivo (não uma pasta). Nada aqui interpreta conteúdo ou codificação.
// Centraliza verificações que historicamente estavam duplicadas em
// PlaylistLocator, PlaylistIni e PlaylistInstallation (isFileOnDisk /
// existsOnDisk) para que todas usem a mesma semântica do Windows.
namespace FileSystem {

// Retorna true se o caminho existe no disco (arquivo OU pasta).
bool pathExists(const std::filesystem::path& path);

// Retorna true se o caminho existe E é um arquivo (não uma pasta).
bool isFile(const std::filesystem::path& path);

} // namespace FileSystem