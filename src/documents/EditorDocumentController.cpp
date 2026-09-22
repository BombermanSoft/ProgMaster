#include "documents/EditorDocumentController.h"

#include <filesystem>

#include <string>

#include <system_error>

#include "core/Log.h"
#include "playlist/PlaylistInstallation.h"

void EditorDocumentController::setInstallation(PlaylistInstallation* installation)
{
    m_installation = installation;
}

EditorDocument EditorDocumentController::current() const
{
    return m_current;
}

void EditorDocumentController::setCurrent(EditorDocument doc)
{
    m_current = doc;
}

std::filesystem::path EditorDocumentController::pathFor(EditorDocument doc) const
{
    if (!m_installation) {
        return std::filesystem::path();
    }
    switch (doc) {
    case EditorDocument::PlaylistIni:
        return m_installation->playlistIniPath();
    case EditorDocument::MapasComercial:
        return m_installation->mapasTxtPath();
    case EditorDocument::GradesMusicais:
        return m_installation->gradesTxtPath();
    }
    return std::filesystem::path();
}

std::wstring EditorDocumentController::fileNameFor(EditorDocument doc) const
{
    switch (doc) {
    case EditorDocument::PlaylistIni:
        return L"playlist.ini";
    case EditorDocument::MapasComercial:
        return L"mapa.txt";
    case EditorDocument::GradesMusicais:
        return L"grade.txt";
    }
    return L"arquivo de texto";
}

PlaylistIni* EditorDocumentController::serviceFor(EditorDocument doc)
{
    switch (doc) {
    case EditorDocument::PlaylistIni:
        return &m_ini;
    case EditorDocument::MapasComercial:
        return &m_mapas;
    case EditorDocument::GradesMusicais:
        return &m_grades;
    }
    return &m_ini;
}

const PlaylistIni* EditorDocumentController::serviceFor(EditorDocument doc) const
{
    return const_cast<EditorDocumentController*>(this)->serviceFor(doc);
}

DocumentLoadResult EditorDocumentController::load(EditorDocument doc)
{
    DocumentLoadResult r;
    m_current = doc;
    PlaylistIni* svc = serviceFor(doc);

    const std::filesystem::path path = pathFor(doc);
    svc->setPath(path);

    if (path.empty()) {
        // Nenhuma instalação configurada: não há o que carregar.
        r.outcome = DocumentLoadOutcome::Unavailable;
        r.userMessage = L"Nenhuma instalação do Playlist configurada.";
        return r;
    }

    // Nome exibido: usa o nome real do arquivo encontrado; se o arquivo não
    // existir, usa o nome canônico.
    const std::wstring actualName = path.filename().wstring();
    const std::wstring displayName = actualName.empty()
                                         ? fileNameFor(doc)
                                         : actualName;
    svc->setDisplayName(displayName);

    Log::info(L"loadFile: " + displayName + L" -> " + path.wstring());

    if (!svc->exists()) {
        r.outcome = DocumentLoadOutcome::MissingFile;
        r.displayName = displayName;
        return r;
    }

    std::wstring content;
    std::wstring userMessage;
    std::string technicalError;
    if (!svc->load(content, userMessage, technicalError)) {
        r.outcome = DocumentLoadOutcome::LoadError;
        r.displayName = displayName;
        r.userMessage = userMessage;
        r.technicalError = technicalError;
        return r;
    }

    r.outcome = DocumentLoadOutcome::Ok;
    r.content = std::move(content);
    r.displayName = displayName;
    return r;
}

DocumentSaveResult EditorDocumentController::saveCurrent(const std::wstring& content)
{
    DocumentSaveResult r;
    PlaylistIni* svc = serviceFor(m_current);
    const std::wstring fileName = fileNameFor(m_current);
    const std::wstring displayName = svc->displayName().empty()
                                         ? fileName
                                         : svc->displayName();
    r.displayName = displayName;

    if (!svc->hasPath()) {
        r.ok = false;
        r.userMessage = L"Não há um arquivo de texto carregado para salvar (\"" +
                        displayName + L"\").";
        return r;
    }

    const bool wasMissing = !svc->exists();

    // Garante que a pasta do arquivo exista (necessário para criar novos).
    if (!svc->path().parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(svc->path().parent_path(), ec);
        if (ec) {
            r.ok = false;
            r.userMessage = L"Não foi possível criar a pasta do arquivo \"" +
                            displayName + L"\". Verifique as permissões em " +
                            svc->path().parent_path().wstring() + L".";
            r.technicalError = "create_directories falhou (" +
                               std::to_string(ec.value()) + "): " +
                               svc->path().parent_path().string();
            return r;
        }
    }

    std::wstring userMessage;
    std::string technicalError;
    if (!svc->save(content, userMessage, technicalError)) {
        r.ok = false;
        r.userMessage = userMessage;
        r.technicalError = technicalError;
        return r;
    }

    r.ok = true;
    r.created = wasMissing;
    return r;
}