#pragma once

#include <windows.h>

#include <string>
#include <vector>

// Área da guia "Editor": editor de texto simples (semelhante ao Bloco de
// Notas) usado para programar qualquer um dos arquivos de texto do Playlist
// (playlist.ini, mapas\Mapas.txt, Grades\Grades.txt).
//
// Uma barra de ferramentas no topo oferece:
//   - disquete (Salvar): grava o conteúdo atual;
//   - voltar: desfaz a última alteração;
//   - prosseguir: refaz a alteração desfeita.
//
// O histórico de edição é mantido internamente como uma lista de versões do
// texto; "voltar"/"prosseguir" apenas navegam por essa lista. Esta etapa NÃO
// interpreta o conteúdo do arquivo; a leitura/gravação em disco com
// preservação de codificação fica a cargo do serviço de arquivo usado pela
// janela principal.
class IniEditorView {
public:
    enum : int {
        IDC_INI_TOOLBAR = 2000,
        IDC_INI_SAVE = 2001,
        IDC_INI_UNDO = 2002,
        IDC_INI_REDO = 2003,
        IDC_INI_EDIT = 2004,
        IDC_INI_STATUS = 2005,
    };

    void create(HWND parent, HINSTANCE hInstance, HFONT uiFont, HFONT monoFont);
    void layout(const RECT& pageRect);
    void setVisible(bool visible);

    // Carrega um conteúdo no editor (referência inicial / recarga do disco).
    void setContent(const std::wstring& content);

    // Exibe uma mensagem de erro no lugar do conteúdo (ex.: arquivo ausente).
    void setErrorMessage(const std::wstring& message);

    void clear();
    bool hasUnsavedChanges() const;
    bool hasFile() const;
    std::wstring getContent() const;

    // Enquanto o usuário digita, mantém o histórico de edição (voltar/
    // prosseguir). Acionada via EN_CHANGE.
    void onContentEdited();

    // Navegação do histórico: desfazer / refazer a última alteração.
    void onUndo();
    void onRedo();

    // Atualiza a referência "sem alterações" após um salvamento bem-sucedido.
    void markSaved();

    // Define o arquivo atual em edição (exibido na barra da página).
    void setFileName(const std::wstring& fileName);

private:
    void updateStatus();
    void updateUndoRedoButtons();
    void pushHistory(const std::wstring& text);
    void applyHistory(int index);

    HWND m_hParent = nullptr;
    HWND m_hToolbar = nullptr;
    HWND m_hSaveBtn = nullptr;
    HWND m_hUndoBtn = nullptr;
    HWND m_hRedoBtn = nullptr;
    HWND m_hEdit = nullptr;
    HWND m_hStatus = nullptr;

    std::wstring m_lastLoadedText;
    bool m_hasFile = false;             // há um arquivo de texto carregado
    bool m_programmaticChange = false;  // evita marcar "sujo" em alterações do programa
    bool m_dirty = false;               // existem alterações não salvas
    bool m_navigatingHistory = false;   // evita re-empurrar texto durante voltar/prosseguir

    // Histórico de edição (versões sucessivas do texto).
    std::vector<std::wstring> m_history;
    int m_historyIndex = -1;            // posição atual na lista de versões
    static constexpr int kMaxHistory = 100;
};