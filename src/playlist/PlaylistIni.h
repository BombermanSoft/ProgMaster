#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/TextFileIO.h"

// Serviço de acesso a um arquivo de texto da programação do Playlist
// (playlist.ini, mapas\Mapas.txt, Grades\Grades.txt) como TEXTO PURO.
//
// Etapa 1: o conteúdo NÃO é interpretado. Este componente apenas:
//   - localiza o arquivo;
//   - carrega o conteúdo preservando bytes e codificação originais;
//   - salva NA CODIFICAÇÃO QUE O PLAYLIST LÊ: os arquivos de programação
//     (UTF-8, UTF-8 com BOM, ANSI) são gravados como ANSI (CP_ACP); apenas
//     arquivos UTF-16 (BOM) têm a codificação original preservada, pois
//     são produzidos por outras ferramentas.
class PlaylistIni {
public:
    PlaylistIni() = default;
    explicit PlaylistIni(std::filesystem::path path);
    PlaylistIni(const PlaylistIni&) = delete;
    PlaylistIni& operator=(const PlaylistIni&) = delete;

    void setPath(std::filesystem::path path);

    // Nome exibido nas mensagens (ex.: "playlist.ini", "Mapas.txt").
    void setDisplayName(std::wstring displayName);
    const std::wstring& displayName() const;

    bool hasPath() const;
    const std::filesystem::path& path() const;
    bool exists() const;

    // Carrega o conteúdo atual do disco. Preenche mensagens para o usuário
    // e o detalhe técnico para o log em caso de falha.
    bool load(std::wstring& content,
              std::wstring& userMessage,
              std::string& technicalError);

    // Salva o texto fornecido na codificação adequada ao Playlist
    // (ver comentário da classe).
    bool save(const std::wstring& content,
              std::wstring& userMessage,
              std::string& technicalError);

    TextEncoding encoding() const;
    const std::vector<unsigned char>& originalBytes() const;

private:
    // Codificação efetiva de GRAVAÇÃO: ANSI (CP_ACP) para UTF-8/UTF-8
    // BOM/ANSI; preserva UTF-16LE/BE. Se algum caractere não couber na
    // página de código local, encodeFromWide recusa a gravação.
    TextEncoding effectiveWriteEncoding() const;

    std::filesystem::path m_path;
    std::wstring m_displayName = L"playlist.ini";
    TextEncoding m_encoding = TextEncoding::Ansi;
    std::vector<unsigned char> m_originalBytes;
};