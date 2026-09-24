#include "app/PlaylistConfigController.h"

#include <utility>

#include "core/Log.h"
#include "readconf/PlaylistFileLocator.h"
#include "readconf/ReadingConfiguration.h"

namespace app {

PlaylistConfigController::PlaylistConfigController(std::filesystem::path playlistIniPath)
    : m_ini(std::move(playlistIniPath))
{
    m_ini.setDisplayName(L"playlist.ini");
}

void PlaylistConfigController::setPlaylistIniPath(std::filesystem::path playlistIniPath)
{
    m_ini.setPath(std::move(playlistIniPath));
}

void PlaylistConfigController::setInstallationFolder(std::filesystem::path installFolder)
{
    m_installFolder = std::move(installFolder);
}

void PlaylistConfigController::load()
{
    m_loaded = true;
    m_loadMessage.clear();

    if (!m_ini.exists()) {
        m_doc.setText(L"");
        m_loadMessage = std::wstring(L"Não existe um playlist.ini na instalação ainda: ") +
                        L"será criado ao salvar.";
        m_dirty = false;
        return;
    }

    std::wstring content;
    std::wstring userMessage;
    std::string technicalError;
    if (m_ini.load(content, userMessage, technicalError)) {
        m_doc.setText(content);
        m_loadMessage.clear();
    } else {
        m_doc.setText(L"");
        m_loadMessage = userMessage;
        Log::error(technicalError);
    }
    m_dirty = false;
}

const std::filesystem::path& PlaylistConfigController::path() const
{
    return m_ini.path();
}

const std::wstring& PlaylistConfigController::displayName() const
{
    return m_ini.displayName();
}

std::vector<readconf::ScopeSnapshot> PlaylistConfigController::scopes() const
{
    return readconf::readScopes(m_doc, m_installFolder);
}

std::vector<readconf::PlaylistIniDocument::Afiliada> PlaylistConfigController::afiliadas() const
{
    return m_doc.afiliadas();
}

bool PlaylistConfigController::applyOption(readconf::ConfigScope scope,
                                           readconf::FormatOption option)
{
    if (m_doc.applyFormat(scope, option)) {
        setDirty();
        return true;
    }
    return false;
}

bool PlaylistConfigController::addMissingConfiguration(readconf::ConfigScope scope)
{
    if (m_doc.sectionIndex(scope) >= 0) {
        return false; // já presente
    }
    switch (scope) {
    case readconf::ConfigScope::Comercial:
    case readconf::ConfigScope::Musical:
        if (m_doc.applyFormat(scope, readconf::FormatOption::Auto)) {
            setDirty();
            return true;
        }
        return false;
    case readconf::ConfigScope::RelogioComercial:
    case readconf::ConfigScope::RelogioMusical:
        if (m_doc.applyFormat(scope, readconf::FormatOption::Single)) {
            setDirty();
            return true;
        }
        return false;
    case readconf::ConfigScope::Afiliadas:
        if (m_doc.ensureAfiliadasSection()) {
            setDirty();
            return true;
        }
        return false;
    }
    return false;
}

void PlaylistConfigController::addAfiliada(const std::wstring& address,
                                           const std::wstring& portText,
                                           bool disabled)
{
    m_doc.addAfiliada(address, portText, disabled);
    setDirty();
}

void PlaylistConfigController::updateAfiliada(size_t position,
                                              const std::wstring& address,
                                              const std::wstring& portText,
                                              bool disabled)
{
    m_doc.updateAfiliada(position, address, portText, disabled);
    setDirty();
}

void PlaylistConfigController::removeAfiliada(size_t position)
{
    m_doc.removeAfiliada(position);
    setDirty();
}

std::wstring PlaylistConfigController::currentText() const
{
    return m_doc.text();
}

void PlaylistConfigController::setTextFromEditor(const std::wstring& text)
{
    m_doc.setText(text);
    setDirty();
}

bool PlaylistConfigController::save(std::wstring& userMessage,
                                    std::string& technicalError)
{
    userMessage.clear();
    technicalError.clear();

    // 1) Validação (nada é gravado com problema).
    const auto validation = readconf::validateForSave(m_doc);
    if (!validation.ok) {
        userMessage = validation.errorMessage;
        technicalError =
            "Validação bloqueou a gravação: " + std::string(validation.errorMessage.begin(),
                                                           validation.errorMessage.end());
        Log::error(technicalError);
        return false;
    }

    // 2) Gravação com a codificação original (PlaylistIni já cuida disso).
    const std::wstring text = m_doc.text();
    if (!m_ini.save(text, userMessage, technicalError)) {
        Log::error(technicalError);
        return false;
    }

    m_dirty = false;
    Log::info(L"playlist.ini salvo (Etapa 2).");
    return true;
}

} // namespace app