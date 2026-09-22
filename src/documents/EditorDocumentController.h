#pragma once

#include <filesystem>
#include <string>

#include "playlist/PlaylistIni.h"

class PlaylistInstallation;

// Identifica os documentos de texto editáveis pelo editor interno.
enum class EditorDocument {
    PlaylistIni,
    MapasComercial,
    GradesMusicais,
};

// Resultado de um carregamento de documento para o editor.
enum class DocumentLoadOutcome {
    Ok,           // conteúdo lido com sucesso
    MissingFile,  // o arquivo ainda não existe no disco (criação ao salvar)
    LoadError,    // falha ao ler o arquivo existente
    Unavailable,  // nenhuma instalação do Playlist configurada (caminho vazio)
};

struct DocumentLoadResult {
    DocumentLoadOutcome outcome = DocumentLoadOutcome::Unavailable;
    std::wstring content;
    std::wstring displayName;   // nome exibido ao usuário
    std::wstring userMessage;   // mensagem amigável de erro (quando houver)
    std::string technicalError; // detalhe técnico para o log (quando houver)
};

// Resultado de um salvamento do documento atual.
struct DocumentSaveResult {
    bool ok = false;
    bool created = false;               // o arquivo não existia e foi criado
    std::wstring displayName;           // nome exibido ao usuário
    std::wstring userMessage;           // mensagem amigável de erro (quando houver)
    std::string technicalError;         // detalhe técnico para o log (quando houver)
};

// Gerencia os documentos de texto editáveis (playlist.ini, mapa.txt,
// grade.txt), agindo como fachada para a janela principal sobre os três
// serviços PlaylistIni e sobre a resolução de caminhos do PlaylistInstallation.
//
// Responsabilidades:
//   - identificar qual documento está em edição;
//   - resolver o caminho (busca flexível) e o nome exibido de cada documento;
//   - carregar conteúdo do disco e salvar a codificação original;
//   - preparar a criação de arquivos novos (criação da pasta-pai).
//
// O QUE ESTA CLASSE NÃO FAZ: não manipula janelas, mensagens de confirmação
// nem o estado de "sujo" do editor — esses decisões de apresentação ficam na
// MainWindow, que combina controller + IniEditorView.
class EditorDocumentController {
public:
    EditorDocumentController() = default;
    EditorDocumentController(const EditorDocumentController&) = delete;
    EditorDocumentController& operator=(const EditorDocumentController&) = delete;

    // Referência à instalação de onde os caminhos são derivados. NÃO assume a
    // posse; a instalação pertence à janela principal.
    void setInstallation(PlaylistInstallation* installation);

    // Documento atualmente em edição.
    EditorDocument current() const;
    void setCurrent(EditorDocument doc);

    // Caminho resolvido para o documento (com a busca flexível) e o nome
    // canônico de fallback usado nas mensagens.
    std::filesystem::path pathFor(EditorDocument doc) const;
    std::wstring fileNameFor(EditorDocument doc) const;

    // Serviço de arquivo (PlaylistIni) associado ao documento.
    PlaylistIni* serviceFor(EditorDocument doc);
    const PlaylistIni* serviceFor(EditorDocument doc) const;

    // Prepara e carrega o documento: define o documento atual, resolve o
    // caminho/nome exibido e lê o conteúdo do disco. MissingFile indica um
    // arquivo que ainda não existe (o editor deve abrir vazio e apoiar a
    // criação ao salvar).
    DocumentLoadResult load(EditorDocument doc);

    // Salva o conteúdo fornecido no documento atual, usando a codificação
    // original e criando a pasta-pai e o arquivo quando necessário.
    // created=true indica que o arquivo não existia antes.
    DocumentSaveResult saveCurrent(const std::wstring& content);

private:
    PlaylistInstallation* m_installation = nullptr;
    EditorDocument m_current = EditorDocument::PlaylistIni;
    PlaylistIni m_ini;
    PlaylistIni m_mapas;
    PlaylistIni m_grades;
};