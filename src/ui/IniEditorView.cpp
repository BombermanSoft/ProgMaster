#include "ui/IniEditorView.h"

#include <commctrl.h>

#include <string>

#include <utility>

namespace {

// Converte um ID numérico de controle em HMENU (evita o aviso C4312 em x64).
HMENU menuFromId(int id)
{
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

} // namespace

void IniEditorView::create(HWND parent, HINSTANCE hInstance, HFONT uiFont, HFONT monoFont)
{
    m_hParent = parent;

    // Barra de ferramentas com os ícones padrão do sistema (disquete de
    // salvar, seta para trás, seta para frente) — evita arquivos de recurso.
    m_hToolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, L"",
                                 WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT |
                                     CCS_TOP | CCS_NORESIZE,
                                 0, 0, 0, 0, parent,
                                 menuFromId(IDC_INI_TOOLBAR),
                                 hInstance, nullptr);
    SendMessageW(m_hToolbar, TB_BUTTONSTRUCTSIZE,
                 static_cast<WPARAM>(sizeof(TBBUTTON)), 0);

    TBADDBITMAP addBmp{};
    addBmp.hInst = HINST_COMMCTRL;
    addBmp.nID = IDB_STD_SMALL_COLOR;
    SendMessageW(m_hToolbar, TB_ADDBITMAP, 0, reinterpret_cast<LPARAM>(&addBmp));

    TBBUTTON buttons[3]{};
    buttons[0].iBitmap = STD_FILESAVE;
    buttons[0].idCommand = IDC_INI_SAVE;
    buttons[0].fsStyle = TBSTYLE_BUTTON;
    buttons[0].fsState = TBSTATE_ENABLED;
    buttons[0].iString = 0;

    buttons[1].iBitmap = STD_UNDO;
    buttons[1].idCommand = IDC_INI_UNDO;
    buttons[1].fsStyle = TBSTYLE_BUTTON;
    buttons[1].fsState = TBSTATE_ENABLED;
    buttons[1].iString = 0;

    buttons[2].iBitmap = STD_REDOW;
    buttons[2].idCommand = IDC_INI_REDO;
    buttons[2].fsStyle = TBSTYLE_BUTTON;
    buttons[2].fsState = TBSTATE_ENABLED;
    buttons[2].iString = 0;

    SendMessageW(m_hToolbar, TB_ADDBUTTONS,
                 static_cast<WPARAM>(3), reinterpret_cast<LPARAM>(buttons));

    m_hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                  ES_MULTILINE | ES_WANTRETURN |
                                  ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                                  WS_VSCROLL | WS_HSCROLL,
                              0, 0, 0, 0, parent,
                              menuFromId(IDC_INI_EDIT), hInstance, nullptr);
    m_hStatus = CreateWindowExW(0, L"STATIC", L"",
                                WS_CHILD | WS_VISIBLE,
                                0, 0, 0, 0, parent,
                                menuFromId(IDC_INI_STATUS), hInstance, nullptr);

    SendMessageW(m_hToolbar, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
    SendMessageW(m_hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont), TRUE);
    SendMessageW(m_hStatus, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);

    SendMessageW(m_hToolbar, TB_AUTOSIZE, 0, 0);
}

void IniEditorView::layout(const RECT& pageRect)
{
    const int margin = 6;
    const int toolbarH = 32;
    const int statusH = 18;
    const int contentLeft = pageRect.left + margin;
    const int contentW = (pageRect.right - margin) - contentLeft;

    MoveWindow(m_hToolbar, contentLeft, pageRect.top + margin,
               contentW, toolbarH, TRUE);
    SendMessageW(m_hToolbar, TB_AUTOSIZE, 0, 0);

    const int statusTop = pageRect.bottom - margin - statusH;
    MoveWindow(m_hStatus, contentLeft, statusTop, contentW, statusH, TRUE);

    const int editY = pageRect.top + margin + toolbarH + 6;
    const int editH = statusTop - 6 - editY;
    if (editH > 0) {
        MoveWindow(m_hEdit, contentLeft, editY, contentW, editH, TRUE);
    }

    updateUndoRedoButtons();
}

void IniEditorView::setVisible(bool visible)
{
    const int showState = visible ? SW_SHOW : SW_HIDE;
    ShowWindow(m_hToolbar, showState);
    ShowWindow(m_hEdit, showState);
    ShowWindow(m_hStatus, showState);
}

void IniEditorView::setContent(const std::wstring& content)
{
    // SetWindowTextW dispara EN_CHANGE; a guarda evita marcar como "sujo"
    // alterações feitas pelo próprio programa.
    m_programmaticChange = true;
    SetWindowTextW(m_hEdit, content.c_str());
    m_programmaticChange = false;

    m_lastLoadedText = content;
    m_hasFile = true;
    m_dirty = false;

    // Reinicia o histórico de edição com a versão recém-carregada.
    m_history.clear();
    m_history.push_back(content);
    m_historyIndex = 0;

    updateUndoRedoButtons();
    updateStatus();
}

void IniEditorView::setErrorMessage(const std::wstring& message)
{
    m_programmaticChange = true;
    SetWindowTextW(m_hEdit, L"");
    m_programmaticChange = false;

    m_hasFile = false;
    m_dirty = false;
    m_lastLoadedText.clear();
    m_history.clear();
    m_historyIndex = -1;
    SetWindowTextW(m_hStatus, message.c_str());
    updateUndoRedoButtons();
}

void IniEditorView::clear()
{
    m_programmaticChange = true;
    SetWindowTextW(m_hEdit, L"");
    m_programmaticChange = false;

    m_hasFile = false;
    m_dirty = false;
    m_lastLoadedText.clear();
    m_history.clear();
    m_historyIndex = -1;
    SetWindowTextW(m_hStatus, L"");
    updateUndoRedoButtons();
}

bool IniEditorView::hasUnsavedChanges() const
{
    return m_dirty;
}

bool IniEditorView::hasFile() const
{
    return m_hasFile;
}

std::wstring IniEditorView::getContent() const
{
    if (!m_hEdit) {
        return std::wstring();
    }
    const int length = GetWindowTextLengthW(m_hEdit);
    std::wstring text(static_cast<size_t>(length), L'\0');
    if (length > 0) {
        GetWindowTextW(m_hEdit, text.data(), length + 1);
    }
    return text;
}

void IniEditorView::onContentEdited()
{
    if (m_programmaticChange || m_navigatingHistory) {
        return;
    }
    m_dirty = true;
    pushHistory(getContent());
    updateStatus();
}

void IniEditorView::onUndo()
{
    if (m_historyIndex > 0) {
        applyHistory(m_historyIndex - 1);
    }
}

void IniEditorView::onRedo()
{
    if (m_historyIndex >= 0 &&
        m_historyIndex + 1 < static_cast<int>(m_history.size())) {
        applyHistory(m_historyIndex + 1);
    }
}

void IniEditorView::markSaved()
{
    m_lastLoadedText = getContent();
    m_dirty = false;
    updateStatus();
    updateUndoRedoButtons();
}

void IniEditorView::setFileName(const std::wstring& fileName)
{
    // A barra de status já mostra se há alterações; o nome do arquivo fica
    // à disposição da janela principal para a barra de status externa.
    SetWindowTextW(m_hStatus, (L"Arquivo atual: " + fileName).c_str());
}

void IniEditorView::pushHistory(const std::wstring& text)
{
    m_navigatingHistory = true;
    const int current = m_historyIndex;
    // Descarta as versões "à frente" quando o usuário edita a partir de um
    // ponto do meio do histórico (comportamento padrão de undo/redo).
    m_history.erase(m_history.begin() + current + 1, m_history.end());

    m_history.push_back(text);
    if (static_cast<int>(m_history.size()) > kMaxHistory) {
        m_history.erase(m_history.begin());
    }
    m_historyIndex = static_cast<int>(m_history.size()) - 1;
    m_navigatingHistory = false;
    updateUndoRedoButtons();
}

void IniEditorView::applyHistory(int index)
{
    if (index < 0 || index >= static_cast<int>(m_history.size())) {
        return;
    }
    m_navigatingHistory = true;
    SetWindowTextW(m_hEdit, m_history[index].c_str());
    m_navigatingHistory = false;
    m_historyIndex = index;
    m_dirty = true; // voltou a uma versão anterior: ainda é alteração não salva
    updateUndoRedoButtons();
    updateStatus();
}

void IniEditorView::updateUndoRedoButtons()
{
    if (!m_hToolbar) {
        return;
    }
    const bool canUndo = (m_historyIndex > 0);
    const bool canRedo = (m_historyIndex >= 0 &&
                          m_historyIndex + 1 < static_cast<int>(m_history.size()));
    SendMessageW(m_hToolbar, TB_ENABLEBUTTON,
                 static_cast<WPARAM>(IDC_INI_UNDO),
                 static_cast<LPARAM>(canUndo ? TRUE : FALSE));
    SendMessageW(m_hToolbar, TB_ENABLEBUTTON,
                 static_cast<WPARAM>(IDC_INI_REDO),
                 static_cast<LPARAM>(canRedo ? TRUE : FALSE));
}

void IniEditorView::updateStatus()
{
    if (!m_hasFile) {
        return;
    }
    SetWindowTextW(m_hStatus,
                   m_dirty ? L"Alterações não salvas no arquivo atual."
                           : L"Sem alterações não salvas.");
}