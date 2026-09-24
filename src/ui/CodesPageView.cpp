#include "ui/CodesPageView.h"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>

namespace {

// Converte um ID numérico de controle em HMENU (evita o aviso C4312 em x64).
HMENU menuFromId(int id)
{
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

} // namespace

void CodesPageView::create(HWND parent, HINSTANCE hInstance, HFONT uiFont)
{
    m_hParent = parent;

    m_hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                  LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                              0, 0, 0, 0, parent,
                              menuFromId(IDC_CODES_LIST),
                              hInstance, nullptr);
    m_hStatus = CreateWindowExW(0, L"STATIC", L"",
                                WS_CHILD | WS_VISIBLE,
                                0, 0, 0, 0, parent,
                                menuFromId(IDC_CODES_STATUS),
                                hInstance, nullptr);
    m_hSummary = CreateWindowExW(0, L"STATIC", L"",
                                 WS_CHILD | WS_VISIBLE,
                                 0, 0, 0, 0, parent,
                                 menuFromId(IDC_CODES_SUMMARY),
                                 hInstance, nullptr);

    SendMessageW(m_hList, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
    SendMessageW(m_hStatus, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
    SendMessageW(m_hSummary, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);

    SendMessageW(m_hList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                 static_cast<LPARAM>(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES));

    // Colunas: a primeira é o código (DBFId), o identificador de programação.
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.cx = 150;
    wchar_t codeHeader[] = L"Código (DBFId)";
    column.pszText = codeHeader;
    column.iSubItem = 0;
    SendMessageW(m_hList, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>(&column));

    column.cx = 300;
    wchar_t nameHeader[] = L"Nome (Title)";
    column.pszText = nameHeader;
    column.iSubItem = 1;
    SendMessageW(m_hList, LVM_INSERTCOLUMNW, 1, reinterpret_cast<LPARAM>(&column));
}

void CodesPageView::layout(const RECT& pageRect)
{
    const int margin = 6;
    const int summaryH = 18;
    const int statusH = 18;
    const int contentLeft = pageRect.left + margin;
    const int contentW = (pageRect.right - margin) - contentLeft;

    MoveWindow(m_hSummary, contentLeft, pageRect.top + margin,
               contentW, summaryH, TRUE);

    const int listTop = pageRect.top + margin + summaryH + 6;
    const int statusTop = pageRect.bottom - margin - statusH;
    const int listH = statusTop - 6 - listTop;
    if (listH > 0) {
        MoveWindow(m_hList, contentLeft, listTop, contentW, listH, TRUE);
    }
    MoveWindow(m_hStatus, contentLeft, statusTop, contentW, statusH, TRUE);
}

void CodesPageView::setVisible(bool visible)
{
    const int showState = visible ? SW_SHOW : SW_HIDE;
    ShowWindow(m_hList, showState);
    ShowWindow(m_hStatus, showState);
    ShowWindow(m_hSummary, showState);
}

void CodesPageView::populate(const std::vector<FolderEntry>& entries,
                             const std::wstring& sourcePath)
{
    SendMessageW(m_hList, LVM_DELETEALLITEMS, 0, 0);

    // Estatísticas (apenas para o resumo da página; nenhum dado é alterado).
    size_t withoutDbfId = 0;
    std::vector<std::wstring> dbfIds;
    dbfIds.reserve(entries.size());
    for (const FolderEntry& entry : entries) {
        if (entry.hasDbfId()) {
            dbfIds.push_back(entry.dbfId);
        } else {
            ++withoutDbfId;
        }
    }

    // Conta duplicados apenas para informar o usuário. A exibição MANTÉM
    // todos os registros, fiel ao conteúdo original do folders.xml.
    size_t duplicated = 0;
    {
        std::vector<std::wstring> sorted = dbfIds;
        std::sort(sorted.begin(), sorted.end());
        size_t i = 0;
        while (i < sorted.size()) {
            size_t j = i;
            while (j < sorted.size() && sorted[j] == sorted[i]) {
                ++j;
            }
            if (j - i > 1) {
                duplicated += (j - i);
            }
            i = j;
        }
    }

    int rowIndex = 0;
    for (const FolderEntry& entry : entries) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = rowIndex;
        item.iSubItem = 0;
        std::wstring code = entry.dbfId; // vazio quando o registro não possui DBFId
        item.pszText = code.data();
        const int inserted = static_cast<int>(
            SendMessageW(m_hList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item)));

        if (inserted >= 0) {
            item.mask = LVIF_TEXT;
            item.iItem = inserted;
            item.iSubItem = 1;
            item.pszText = const_cast<wchar_t*>(entry.title.c_str());
            SendMessageW(m_hList, LVM_SETITEMTEXTW, inserted, reinterpret_cast<LPARAM>(&item));
        }
        ++rowIndex;
    }

    std::wstring summary =
        L"Registros no folders.xml: " + std::to_wstring(entries.size()) +
        L"   |   com DBFId: " + std::to_wstring(dbfIds.size()) +
        L"   |   sem DBFId: " + std::to_wstring(withoutDbfId);
    if (duplicated > 0) {
        summary += L"   |   DBFId repetidos: " + std::to_wstring(duplicated) +
                   L" (preservados conforme o arquivo original)";
    }
    SetWindowTextW(m_hSummary, summary.c_str());

    if (!sourcePath.empty()) {
        SetWindowTextW(m_hStatus, (L"Arquivo: " + sourcePath).c_str());
    } else {
        SetWindowTextW(m_hStatus, L"");
    }

    // Garante que a lista seja redesenhada imediatamente, mesmo que esta
    // página não seja a aba ativa no momento.
    RedrawWindow(m_hList, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}

void CodesPageView::showMessage(const std::wstring& message)
{
    SendMessageW(m_hList, LVM_DELETEALLITEMS, 0, 0);
    SetWindowTextW(m_hSummary, message.c_str());
    SetWindowTextW(m_hStatus, L"");
}

void CodesPageView::clear()
{
    SendMessageW(m_hList, LVM_DELETEALLITEMS, 0, 0);
    SetWindowTextW(m_hSummary, L"");
    SetWindowTextW(m_hStatus, L"");
}

int CodesPageView::itemCount() const
{
    if (!m_hList) {
        return 0;
    }
    return static_cast<int>(SendMessageW(m_hList, LVM_GETITEMCOUNT, 0, 0));
}