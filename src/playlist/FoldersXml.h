#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "models/FolderEntry.h"

// Leitor do folders.xml (SOMENTE LEITURA).
//
// Responsável por extrair os registros <Folder> e seus campos principais
// (DBFId, Title, ID, Type, Target) SEM jamais modificar o arquivo.
//
// Não existe nenhuma função de edição/gravação para o folders.xml, por regra
// deste projeto: o arquivo original deve permanecer intacto.
class FoldersXml {
public:
    // Lê todos os registros. Em caso de sucesso retorna uma string vazia;
    // em caso de erro, uma explicação amigável para exibir ao usuário.
    static std::wstring readAll(const std::filesystem::path& path,
                                std::vector<FolderEntry>& outEntries,
                                std::string& technicalError);
};