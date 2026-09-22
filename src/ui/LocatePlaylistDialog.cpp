#include "ui/LocatePlaylistDialog.h"

#include <windows.h>
#include <commdlg.h>

#include <string>

#include "playlist/PlaylistLocator.h"
#include "ui/Win32Ids.h"

namespace {

enum : int {
    IDC_PATH_EDIT = 4001,
    IDC_BROWSE_BTN = 4002,
    IDC_CONFIRM_BTN = 4003,
    IDC_CANCEL_BTN = 4004,
};

constexpr wchar_t kDialogClass[] = L"PlaylistLocatorDialogClass";

struct DialogData {
    PlaylistLocator* locator = nullptr;
    HINSTANCE hInstance = nullptr;
    HWND hEditPath = nullptr;
    bool confirmed = false;
    bool done = false;
};

void setGuiFont(HWND hwnd)
{
    SendMessageW(hwnd, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

LRESULT CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    DialogData* data = reinterpret_cast<DialogData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!data && msg == WM_NCCREATE) {
        data = reinterpret_cast<DialogData*>(
            reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
    }
    if (!data) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    switch (msg) {
    case WM_CREATE: {
        HWND hLabel = CreateWindowExW(0, L"STATIC", L"Caminho do Playlist.exe:",
                                      WS_CHILD | WS_VISIBLE,
                                      14, 14, 320, 18, hwnd, nullptr,
                                      data->hInstance, nullptr);
        data->hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                          14, 34, 400, 24, hwnd,
                                          Win32Ids::menuFromId(IDC_PATH_EDIT),
                                          data->hInstance, nullptr);
        HWND hBrowse = CreateWindowExW(0, L"BUTTON", L"Procurar...",
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                       426, 33, 122, 26, hwnd,
                                       Win32Ids::menuFromId(IDC_BROWSE_BTN),
                                       data->hInstance, nullptr);
        HWND hConfirm = CreateWindowExW(0, L"BUTTON", L"Confirmar",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                        336, 94, 100, 28, hwnd,
                                        Win32Ids::menuFromId(IDC_CONFIRM_BTN),
                                        data->hInstance, nullptr);
        HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Cancelar",
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                       444, 94, 100, 28, hwnd,
                                       Win32Ids::menuFromId(IDC_CANCEL_BTN),
                                       data->hInstance, nullptr);

        setGuiFont(hLabel);
        setGuiFont(data->hEditPath);
        setGuiFont(hBrowse);
        setGuiFont(hConfirm);
        setGuiFont(hCancel);

        // Pré-preenche com o caminho atual, se houver um salvo.
        if (data->locator->hasValidInstallation()) {
            SetWindowTextW(data->hEditPath, data->locator->getPlaylistExePath().c_str());
        }
        SetFocus(data->hEditPath);
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wp);
        if (id == IDC_BROWSE_BTN) {
            // Seletor de arquivo com filtro voltado ao Playlist.exe.
            wchar_t file[MAX_PATH]{};
            wchar_t filter[] = L"Playlist executável (Playlist.exe)\0Playlist.exe\0"
                               L"Programas executáveis (*.exe)\0*.exe\0"
                               L"Todos os arquivos (*.*)\0*.*\0\0";
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = filter;
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
            if (GetOpenFileNameW(&ofn)) {
                SetWindowTextW(data->hEditPath, file);
            }
        } else if (id == IDC_CONFIRM_BTN) {
            wchar_t buffer[4096]{};
            GetWindowTextW(data->hEditPath, buffer, 4096);

            std::wstring userMessage;
            if (data->locator->setPlaylistExePath(buffer, userMessage)) {
                data->confirmed = true;
                data->done = true;
                DestroyWindow(hwnd);
            } else {
                MessageBoxW(hwnd, userMessage.c_str(), L"Playlist.exe inválido",
                            MB_ICONWARNING);
            }
        } else if (id == IDC_CANCEL_BTN) {
            data->done = true;
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_CLOSE:
        data->done = true;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        data->done = true;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

bool LocatePlaylistDialog::show(HWND owner, HINSTANCE hInstance, PlaylistLocator& locator)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &dialogProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kDialogClass;
    RegisterClassExW(&wc);

    DialogData data;
    data.locator = &locator;
    data.hInstance = hInstance;

    const int width = 560;
    const int height = 152;
    RECT windowRect{ 0, 0, width, height };
    AdjustWindowRectEx(&windowRect, WS_POPUP | WS_CAPTION | WS_SYSMENU,
                       FALSE, WS_EX_DLGMODALFRAME);

    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    const int x = (screenWidth - (windowRect.right - windowRect.left)) / 2;
    const int y = (screenHeight - (windowRect.bottom - windowRect.top)) / 2;

    // IMPORTANTE: o estilo precisa incluir WS_VISIBLE e o diálogo deve ser
    // exibido explicitamente. Sem isso a janela dona (desabilitada pelo loop
    // modal) fica bloqueada e o diálogo invisível — a interface parece
    // "travada" e nenhum clique funciona.
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kDialogClass,
                                L"Localizar Playlist.exe",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                x, y,
                                windowRect.right - windowRect.left,
                                windowRect.bottom - windowRect.top,
                                owner, nullptr, hInstance, &data);
    if (!hwnd) {
        return false;
    }

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    // Loop modal: desabilita a janela dona enquanto o diálogo estiver aberto.
    if (owner) {
        EnableWindow(owner, FALSE);
    }
    if (data.hEditPath) {
        SetFocus(data.hEditPath);
    }

    MSG msg{};
    while (!data.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    return data.confirmed;
}