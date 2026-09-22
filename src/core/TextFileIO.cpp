#include "core/TextFileIO.h"

#include <windows.h>

#include <cstdint>

namespace {

// Lê o arquivo inteiro como bytes brutos.
bool readFileBytes(const std::filesystem::path& path,
                   std::vector<unsigned char>& outBytes,
                   std::string& outError)
{
    outBytes.clear();
    outError.clear();

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        outError = "CreateFileW falhou (código " +
                   std::to_string(static_cast<unsigned long>(GetLastError())) + ").";
        return false;
    }

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart < 0) {
        outError = "GetFileSizeEx falhou (código " +
                   std::to_string(static_cast<unsigned long>(GetLastError())) + ").";
        CloseHandle(hFile);
        return false;
    }

    if (fileSize.QuadPart > 0) {
        outBytes.resize(static_cast<size_t>(fileSize.QuadPart));
        DWORD totalRead = 0;
        while (totalRead < outBytes.size()) {
            DWORD read = 0;
            if (!ReadFile(hFile, outBytes.data() + totalRead,
                          static_cast<DWORD>(outBytes.size() - totalRead),
                          &read, nullptr)) {
                outError = "ReadFile falhou (código " +
                           std::to_string(static_cast<unsigned long>(GetLastError())) + ").";
                CloseHandle(hFile);
                outBytes.clear();
                return false;
            }
            if (read == 0) {
                break;
            }
            totalRead += read;
        }
        outBytes.resize(totalRead);
    }
    CloseHandle(hFile);
    return true;
}

// Converte bytes para texto largo usando a página de código informada.
bool multibyteToWide(UINT codePage, const unsigned char* data, size_t size,
                     std::wstring& outText, bool strictUtf8)
{
    outText.clear();
    if (size == 0) {
        return true;
    }
    const DWORD flags = strictUtf8 ? MB_ERR_INVALID_CHARS : 0;
    const int needed = MultiByteToWideChar(codePage, flags,
                                           reinterpret_cast<LPCSTR>(data),
                                           static_cast<int>(size), nullptr, 0);
    if (needed <= 0) {
        return false;
    }
    outText.resize(static_cast<size_t>(needed));
    MultiByteToWideChar(codePage, flags, reinterpret_cast<LPCSTR>(data),
                        static_cast<int>(size), outText.data(), needed);
    return true;
}

// Converte texto largo para bytes usando a página de código informada.
bool wideToMultibyte(UINT codePage, const std::wstring& text,
                     std::vector<unsigned char>& outBytes, bool strict)
{
    outBytes.clear();
    if (text.empty()) {
        return true;
    }
    const DWORD flags = strict ? WC_ERR_INVALID_CHARS : 0;
    const int needed = WideCharToMultiByte(codePage, flags, text.c_str(),
                                           static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return false;
    }
    outBytes.resize(static_cast<size_t>(needed));
    WideCharToMultiByte(codePage, flags, text.c_str(),
                        static_cast<int>(text.size()),
                        reinterpret_cast<char*>(outBytes.data()), needed,
                        nullptr, nullptr);
    return true;
}

// Heurística: arquivo grande com muitos bytes zero em posições ímpares
// provavelmente é UTF-16LE sem BOM.
bool looksLikeUtf16Le(const std::vector<unsigned char>& bytes)
{
    if (bytes.size() < 4 || bytes.size() % 2 != 0) {
        return false;
    }
    size_t nullsOnOdd = 0;
    for (size_t i = 1; i < bytes.size(); i += 2) {
        if (bytes[i] == 0) {
            ++nullsOnOdd;
        }
    }
    return nullsOnOdd >= bytes.size() / 4;
}

bool decodeUtf16(const unsigned char* data, size_t sizeInBytes, bool bigEndian,
                 std::wstring& outText)
{
    if (sizeInBytes % 2 != 0) {
        return false;
    }
    const size_t count = sizeInBytes / 2;
    outText.resize(count);
    if (!bigEndian) {
        for (size_t i = 0; i < count; ++i) {
            outText[i] = static_cast<wchar_t>(data[2 * i]) |
                         (static_cast<wchar_t>(data[2 * i + 1]) << 8);
        }
    } else {
        for (size_t i = 0; i < count; ++i) {
            outText[i] = (static_cast<wchar_t>(data[2 * i]) << 8) |
                         static_cast<wchar_t>(data[2 * i + 1]);
        }
    }
    return true;
}

} // namespace

TextFileResult TextFileIO::readWide(const std::filesystem::path& path)
{
    TextFileResult result;

    std::string readError;
    if (!readFileBytes(path, result.originalBytes, readError)) {
        result.technicalError = "Falha ao ler o arquivo: " + readError;
        result.errorMessage = L"Não foi possível ler o arquivo.";
        return result;
    }

    if (result.originalBytes.empty()) {
        // Arquivo vazio é válido: conteúdo vazio.
        result.ok = true;
        result.encoding = TextEncoding::Utf8;
        return result;
    }

    size_t bomLength = 0;
    result.encoding = detectEncoding(result.originalBytes, bomLength);

    if (decodeToWide(result.originalBytes, bomLength, result.encoding, result.text)) {
        result.ok = true;
    } else {
        result.technicalError =
            "Falha na decodificação do conteúdo como texto (" + path.string() + ").";
        result.errorMessage = L"Não foi possível interpretar o conteúdo do arquivo como texto.";
    }
    return result;
}

TextEncoding TextFileIO::detectEncoding(const std::vector<unsigned char>& bytes,
                                        size_t& bomLength)
{
    bomLength = 0;
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        bomLength = 3; // EF BB BF -> UTF-8 com BOM
        return TextEncoding::Utf8Bom;
    }
    if (bytes.size() >= 2) {
        if (bytes[0] == 0xFF && bytes[1] == 0xFE) {
            bomLength = 2; // FF FE -> UTF-16LE com BOM
            return TextEncoding::Utf16Le;
        }
        if (bytes[0] == 0xFE && bytes[1] == 0xFF) {
            bomLength = 2; // FE FF -> UTF-16BE com BOM
            return TextEncoding::Utf16Be;
        }
    }

    // Sem BOM: tenta UTF-8 estrito; se o conteúdo for UTF-8 válido, assume UTF-8.
    std::wstring probe;
    if (multibyteToWide(CP_UTF8, bytes.data(), bytes.size(), probe, /*strictUtf8=*/true)) {
        return TextEncoding::Utf8;
    }

    // Se não for UTF-8 e houver sinais de UTF-16LE, assume UTF-16LE (sem BOM).
    if (looksLikeUtf16Le(bytes)) {
        return TextEncoding::Utf16Le;
    }

    // Caso contrário, assume a página de código local (ANSI/CP_ACP).
    return TextEncoding::Ansi;
}

bool TextFileIO::decodeToWide(const std::vector<unsigned char>& bytes,
                              size_t offset,
                              TextEncoding encoding,
                              std::wstring& outText)
{
    outText.clear();
    if (offset > bytes.size()) {
        return false;
    }
    const unsigned char* data = bytes.data() + offset;
    const size_t size = bytes.size() - offset;

    switch (encoding) {
    case TextEncoding::Utf8:
    case TextEncoding::Utf8Bom:
        return multibyteToWide(CP_UTF8, data, size, outText, /*strictUtf8=*/true);
    case TextEncoding::Utf16Le:
        return decodeUtf16(data, size, /*bigEndian=*/false, outText);
    case TextEncoding::Utf16Be:
        return decodeUtf16(data, size, /*bigEndian=*/true, outText);
    case TextEncoding::Ansi:
        return multibyteToWide(CP_ACP, data, size, outText, /*strictUtf8=*/false);
    }
    return false;
}

bool TextFileIO::encodeFromWide(const std::wstring& text,
                                TextEncoding encoding,
                                std::vector<unsigned char>& outBytes)
{
    outBytes.clear();
    switch (encoding) {
    case TextEncoding::Utf8:
        return wideToMultibyte(CP_UTF8, text, outBytes, /*strict=*/false);
    case TextEncoding::Utf8Bom: {
        std::vector<unsigned char> utf8;
        if (!wideToMultibyte(CP_UTF8, text, utf8, false)) {
            return false;
        }
        outBytes.reserve(utf8.size() + 3);
        outBytes.push_back(0xEF);
        outBytes.push_back(0xBB);
        outBytes.push_back(0xBF);
        outBytes.insert(outBytes.end(), utf8.begin(), utf8.end());
        return true;
    }
    case TextEncoding::Utf16Le: {
        outBytes.reserve(text.size() * 2 + 2);
        outBytes.push_back(0xFF);
        outBytes.push_back(0xFE);
        for (wchar_t ch : text) {
            outBytes.push_back(static_cast<unsigned char>(ch & 0xFF));
            outBytes.push_back(static_cast<unsigned char>((ch >> 8) & 0xFF));
        }
        return true;
    }
    case TextEncoding::Utf16Be: {
        outBytes.reserve(text.size() * 2 + 2);
        outBytes.push_back(0xFE);
        outBytes.push_back(0xFF);
        for (wchar_t ch : text) {
            outBytes.push_back(static_cast<unsigned char>((ch >> 8) & 0xFF));
            outBytes.push_back(static_cast<unsigned char>(ch & 0xFF));
        }
        return true;
    }
    case TextEncoding::Ansi: {
        // WC_ERR_INVALID_CHARS NÃO pode ser usado com CP_ACP (retorna
        // ERROR_INVALID_FLAGS). Como o modo padrão substituiria caracteres não
        // representáveis por '?', a segurança é feita aqui: codifica sem erro
        // e depois decodifica de volta; se o resultado não reproduzir o texto
        // original, algum caractere foi perdido e a gravação é recusada.
        std::vector<unsigned char> ansiBytes;
        if (!wideToMultibyte(CP_ACP, text, ansiBytes, /*strict=*/false)) {
            return false;
        }
        std::wstring roundTrip;
        if (!multibyteToWide(CP_ACP, ansiBytes.data(), ansiBytes.size(),
                             roundTrip, /*strictUtf8=*/false)) {
            return false;
        }
        if (roundTrip != text) {
            return false; // caracteres não representáveis na página de código local
        }
        outBytes = std::move(ansiBytes);
        return true;
    }
    }
    return false;
}

bool TextFileIO::writeWide(const std::filesystem::path& path,
                           const std::wstring& text,
                           TextEncoding encoding,
                           std::string& technicalError)
{
    technicalError.clear();

    std::vector<unsigned char> bytes;
    if (!encodeFromWide(text, encoding, bytes)) {
        technicalError =
            "O texto contém caracteres não representáveis na codificação original "
            "(Ansi/CP_ACP). Nada foi gravado para evitar corromper o arquivo.";
        return false;
    }

    // Gravação atômica: escreve em um temporário e depois substitui o original.
    std::filesystem::path tmpPath = path;
    tmpPath += L".tmp";

    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        technicalError = "Não foi possível criar o arquivo temporário para gravação.";
        return false;
    }

    DWORD written = 0;
    const BOOL writeOk = (bytes.empty() ||
                          WriteFile(hFile, bytes.data(),
                                    static_cast<DWORD>(bytes.size()), &written, nullptr));
    CloseHandle(hFile);

    if (!writeOk || written != bytes.size()) {
        technicalError = "Falha ao gravar o arquivo temporário (escrita incompleta).";
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    if (!MoveFileExW(tmpPath.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        technicalError = "Falha ao substituir o arquivo original pelo temporário.";
        DeleteFileW(tmpPath.c_str());
        return false;
    }
    return true;
}