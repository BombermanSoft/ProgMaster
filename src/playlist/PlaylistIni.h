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
//   - salva usando a MESMA codificação detectada na leitura.
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

    // Salva o texto fornecido na codificação original do arquivo.
    bool save(const std::wstring& content,
              std::wstring& userMessage,
              std::string& technicalError);

    TextEncoding encoding() const;
    const std::vector<unsigned char>& originalBytes() const;

private:
    std::filesystem::path m_path;
    std::wstring m_displayName = L"playlist.ini";
    TextEncoding m_encoding = TextEncoding::Utf8;
    std::vector<unsigned char> m_originalBytes;
};