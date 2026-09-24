#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "readconf/FormatRules.h"

// ============================================================================
// Localização de ARQUIVOS esperados para uma configuração (Etapa 2, §13).
//
// Dada uma opção de formato de um escopo e a instalação do Playlist, resolve
// quais arquivos de programação o ProgMaster deveria encontrar no disco e
// diz se cada um existe (para a interface marcar ✓/✗).
//
// Regra AUTO (quando fornecida) é expandida na ordem de prioridade:
//   Comercial: MapaDD-MM-AAAA, MapaDD, MapaSeg..MapaDom, Mapa.txt
//   Musical:   DD-MM-AAAA, GradeDD, GradeSeg..GradeDom, Grade.txt
// Os marcadores DD/MM/AAAA são substituídos pela data corrente para verificar
// "o arquivo de hoje". Semanal e Único usam nomes determinísticos.
// ============================================================================

namespace readconf {

// Caminho da pasta onde o escopo procura seus arquivos (não existe -> vazio).
//   Comercial / Relógio Comercial -> pasta dos mapas (mapas)
//   Musical / Relógio Musical     -> pasta das grades (grades)
std::filesystem::path folderFor(ConfigScope scope,
                                const std::filesystem::path& installationFolder);

// Um arquivo aguardado e seu estado no disco.
struct FileBinding {
    std::wstring fileName;  // nome exibido (ex.: "MapaSeg.txt", "Mapa23")
    bool exists = false;    // encontrado na pasta do escopo
};

// Lista os arquivos esperados para a opção do escopo, marcando a existência.
// Para Day/Date/Maker, quando nenhum arquivo é encontrado, devolve UM item
// de exemplo com a data corrente (para a interface indicar o esperado).
std::vector<FileBinding> locateFiles(ConfigScope scope,
                                     FormatOption option,
                                     const std::filesystem::path& installationFolder);

} // namespace readconf