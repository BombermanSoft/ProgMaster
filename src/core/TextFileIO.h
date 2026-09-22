#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Codificações de texto suportadas para leitura/escrita de arquivos.
// A preservação da codificação original é uma exigência desta etapa,
// especialmente para o playlist.ini.
enum class TextEncoding {
    Utf8,    // UTF-8 sem BOM
    Utf8Bom, // UTF-8 com BOM
    Utf16Le, // UTF-16 little-endian (com BOM)
    Utf16Be, // UTF-16 big-endian (com BOM)
    Ansi,    // página de código local do Windows (CP_ACP)
};

struct TextFileResult {
    bool ok = false;
    std::wstring text;                          // conteúdo decodificado
    std::vector<unsigned char> originalBytes;   // bytes exatos do disco
    TextEncoding encoding = TextEncoding::Utf8; // codificação detectada
    std::wstring errorMessage;                  // mensagem amigável
    std::string technicalError;                 // detalhe para o log
};

// Utilitário de leitura/escrita de arquivos de texto com detecção e
// preservação de codificação. Usado por Settings, PlaylistIni e FoldersXml.
class TextFileIO {
public:
    // Lê o arquivo inteiro e decodifica para texto largo (UTF-16 nativo).
    static TextFileResult readWide(const std::filesystem::path& path);

    // Grava o texto usando a codificação informada. A gravação é feita em um
    // arquivo temporário e depois substitui o original (gravação atômica).
    static bool writeWide(const std::filesystem::path& path,
                          const std::wstring& text,
                          TextEncoding encoding,
                          std::string& technicalError);

private:
    static TextEncoding detectEncoding(const std::vector<unsigned char>& bytes,
                                       size_t& bomLength);
    static bool decodeToWide(const std::vector<unsigned char>& bytes,
                             size_t offset,
                             TextEncoding encoding,
                             std::wstring& outText);
    static bool encodeFromWide(const std::wstring& text,
                               TextEncoding encoding,
                               std::vector<unsigned char>& outBytes);
};