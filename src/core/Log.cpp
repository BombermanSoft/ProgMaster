#include "core/Log.h"

#include <windows.h>

#include <cwchar>

#include "core/Settings.h"

namespace {

// Data/hora atual no formato "aaaa-mm-dd hh:mm:ss".
std::wstring timestamp()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);

    const int size = std::swprintf(nullptr, 0,
                                   L"%04d-%02d-%02d %02d:%02d:%02d",
                                   st.wYear, st.wMonth, st.wDay,
                                   st.wHour, st.wMinute, st.wSecond);
    if (size <= 0) {
        return L"????-??-?? ??:??:??";
    }
    std::wstring out(static_cast<size_t>(size), L'\0');
    std::swprintf(out.data(), out.size() + 1,
                  L"%04d-%02d-%02d %02d:%02d:%02d",
                  st.wYear, st.wMonth, st.wDay,
                  st.wHour, st.wMinute, st.wSecond);
    return out;
}

std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return std::wstring();
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) {
        return std::wstring();
    }
    std::wstring wide(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                        static_cast<int>(text.size()), wide.data(), needed);
    return wide;
}

} // namespace

void Log::init()
{
    CreateDirectoryW(Settings::getAppDataDir().c_str(), nullptr);

    // Grava o BOM UTF-8 apenas quando o arquivo é criado (CREATE_NEW falha
    // silenciosamente se já existir), para que editores legados exibam os
    // acentos corretamente.
    HANDLE hNew = CreateFileW(filePath().c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hNew != INVALID_HANDLE_VALUE) {
        static const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        DWORD ignored = 0;
        WriteFile(hNew, bom, sizeof(bom), &ignored, nullptr);
        CloseHandle(hNew);
    }

    append(L"=== Sessão iniciada ===");
}

std::filesystem::path Log::filePath()
{
    return Settings::getAppDataDir() / L"log.txt";
}

void Log::append(const std::wstring& line)
{
    // Sempre abre em modo append; grava a linha em UTF-8.
    const std::wstring full = L"[" + timestamp() + L"] " + line + L"\r\n";

    HANDLE hFile = CreateFileW(filePath().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, full.c_str(),
                                             static_cast<int>(full.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (utf8Size > 0) {
        std::string utf8(static_cast<size_t>(utf8Size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, full.c_str(),
                            static_cast<int>(full.size()), utf8.data(), utf8Size,
                            nullptr, nullptr);
        DWORD written = 0;
        WriteFile(hFile, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }
    CloseHandle(hFile);
}

void Log::info(const std::wstring& message)
{
    append(L"INFO: " + message);
}

void Log::error(const std::wstring& message)
{
    append(L"ERRO: " + message);
}

void Log::info(const std::string& message)
{
    append(L"INFO: " + utf8ToWide(message));
}

void Log::error(const std::string& message)
{
    append(L"ERRO: " + utf8ToWide(message));
}