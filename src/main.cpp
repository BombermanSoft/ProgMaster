#include <windows.h>
#include <commctrl.h>

#include "core/Application.h"

// Habilita os controles comuns versão 6.0 (temas modernos do Windows para
// botões, abas, ListView e caixas de edição). Sem essa cláusula o sistema
// usa o visual clássico e os controles ficam com aparência antiga.
#if defined(_MSC_VER)
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// Ponto de entrada da aplicação (GUI, sem janela de console).
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                    PWSTR /*lpCmdLine*/, int nCmdShow)
{
    // Sem DPI awareness, em monitores com escala 125%/150% o Windows
    // virtualiza as coordenadas da janela e o layout dos controles pode
    // ficar desalinhado. Ativa o modo por-monitor (compatível Win10/11).
    {
        using DpiCtxFn = BOOL(WINAPI*)(void*);
        const DpiCtxFn setDpi = reinterpret_cast<DpiCtxFn>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"),
                           "SetProcessDpiAwarenessContext"));
        const void* const perMonitorV2 = reinterpret_cast<const void*>(-4);
        if (!setDpi || !setDpi(const_cast<void*>(perMonitorV2))) {
            SetProcessDPIAware();
        }
    }

    // Inicializa as classes de controles comuns (Tab control, ListView etc.).
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES |
                ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    Application app;
    return app.run(hInstance, nCmdShow);
}