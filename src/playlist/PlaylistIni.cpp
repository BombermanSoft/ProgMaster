#include "playlist/PlaylistIni.h"

#include <windows.h>

#include <cstring>
#include <utility>

#include "core/Log.h"

namespace {

bool isFileOnDisk(const std::filesystem::path& path)
{
    if (path.empty()) {
        return false;
    }
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

} // namespace

PlaylistIni::PlaylistIni(std::filesystem::path path)
    : m_path(std::move(path))
{
}

void PlaylistIni::setPath(std::filesystem::path path)
{
    m_path = std::move(path);
    // Limpa bytes e reseta codificação para ANSI (CP_ACP): arquivos NOVOS
    // (que não existem ainda) são criados na página de código local, igual
    // aos arquivos do Playlist. Se o arquivo existir, o próximo load() redefinirá.
    m_originalBytes.clear();
    m_encoding = TextEncoding::Ansi;
}

void PlaylistIni::setDisplayName(std::wstring displayName)
{
    m_displayName = std::move(displayName);
}

const std::wstring& PlaylistIni::displayName() const
{
    return m_displayName;
}

bool PlaylistIni::hasPath() const
{
    return !m_path.empty();
}

const std::filesystem::path& PlaylistIni::path() const
{
    return m_path;
}

bool PlaylistIni::exists() const
{
    return isFileOnDisk(m_path);
}

TextEncoding PlaylistIni::encoding() const
{
    return m_encoding;
}

const std::vector<unsigned char>& PlaylistIni::originalBytes() const
{
    return m_originalBytes;
}

bool PlaylistIni::load(std::wstring& content,
                       std::wstring& userMessage,
                       std::string& technicalError)
{
    content.clear();
    m_originalBytes.clear();

    if (!exists()) {
        technicalError = "Arquivo de texto não encontrado em: " + m_path.string();
        userMessage = L"Não foi possível localizar o arquivo \"" + m_displayName +
                      L"\" na pasta da instalação do Playlist.";
        Log::error(technicalError);
        return false;
    }

    TextFileResult result = TextFileIO::readWide(m_path);
    if (!result.ok) {
        technicalError = result.technicalError;
        userMessage = result.errorMessage.empty()
                          ? (L"Falha ao abrir o arquivo \"" + m_displayName + L"\".")
                          : result.errorMessage;
        Log::error(technicalError);
        return false;
    }

    content = result.text;
    m_encoding = result.encoding;
    m_originalBytes = std::move(result.originalBytes);

    const char* encName = "?";
    switch (m_encoding) {
    case TextEncoding::Utf8:    encName = "Utf8"; break;
    case TextEncoding::Utf8Bom: encName = "Utf8Bom"; break;
    case TextEncoding::Utf16Le: encName = "Utf16Le"; break;
    case TextEncoding::Utf16Be: encName = "Utf16Be"; break;
    case TextEncoding::Ansi:    encName = "Ansi(CP_ACP)"; break;
    }
    Log::info(m_displayName +
              L": codificacao=" + std::wstring(encName, encName + std::strlen(encName)) +
              L" em " + m_path.wstring());
    return true;
}

bool PlaylistIni::save(const std::wstring& content,
                       std::wstring& userMessage,
                       std::string& technicalError)
{
    userMessage.clear();
    technicalError.clear();

    if (!hasPath()) {
        technicalError = "PlaylistIni::save() chamado sem caminho configurado.";
        userMessage = L"O caminho do arquivo de texto não está definido.";
        Log::error(technicalError);
        return false;
    }

    // Grava na codificação que o Playlist lê (ANSI/CP_ACP) — mesmo que o
    // arquivo original tenha sido criado como UTF-8 (por versões antigas ou
    // outras ferramentas), o conteúdo é convertido para ANSI; se houver
    // caracteres que não couberem na página de código local, a gravação é
    // recusada (TextFileIO confere por round-trip) e nada é corrompido.
    if (!TextFileIO::writeWide(m_path, content, effectiveWriteEncoding(),
                               technicalError)) {
        userMessage = L"Não foi possível salvar o arquivo \"" + m_displayName +
                      L"\". O texto contém caracteres que a codificação ANSI "
                      L"do Playlist (página de código local) não suporta — "
                      L"troque os caracteres especiais (emoji, aspas/acentos "
                      L"estrangeiros) e tente novamente. Nada foi gravado.";
        Log::error(technicalError);
        return false;
    }

    // Após salvar, trata o novo conteúdo como a referência.
    TextFileResult reread = TextFileIO::readWide(m_path);
    if (reread.ok) {
        m_originalBytes = std::move(reread.originalBytes);
        m_encoding = reread.encoding;
    }

    Log::info(m_displayName + L" salvo em: " + m_path.wstring());
    return true;
}

TextEncoding PlaylistIni::effectiveWriteEncoding() const
{
    switch (m_encoding) {
    case TextEncoding::Utf8:
    case TextEncoding::Utf8Bom:
    case TextEncoding::Ansi:
        return TextEncoding::Ansi;
    case TextEncoding::Utf16Le:
    case TextEncoding::Utf16Be:
        return m_encoding;
    }
    return TextEncoding::Ansi;
}