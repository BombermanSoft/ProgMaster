#include "playlist/PlaylistIni.h"

#include <utility>

#include "core/FileSystem.h"
#include "core/Log.h"

PlaylistIni::PlaylistIni(std::filesystem::path path)
    : m_path(std::move(path))
{
}

void PlaylistIni::setPath(std::filesystem::path path)
{
    m_path = std::move(path);
    // Limpa bytes e reseta codificação para UTF-8 (será redefinida pelo
    // próximo load() se o arquivo existir).
    m_originalBytes.clear();
    m_encoding = TextEncoding::Utf8;
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
    return FileSystem::isFile(m_path);
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

    // Grava na codificação original detectada na leitura.
    if (!TextFileIO::writeWide(m_path, content, m_encoding, technicalError)) {
        userMessage = L"Não foi possível salvar o arquivo \"" + m_displayName +
                      L"\". Verifique se o arquivo não está em uso ou se a "
                      L"codificação permite os caracteres digitados.";
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