#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>

// Pequenos helpers de conversão entre std::wstring (usado pelo NÚCLEO, não
// dependente de JUCE) e juce::String (a interface). No Windows, wchar_t é
// UTF-16; juce::CharPointer_UTF16 corresponde a essa representação.
//
// O NÚCLEO (src/core, src/playlist, src/readconf) continua trabalhando com
// std::wstring; a conversão acontece só na fronteira com a interface.

namespace app {

inline juce::String jstr(const std::wstring& ws)
{
    if (ws.empty()) {
        return juce::String();
    }
    return juce::String(juce::CharPointer_UTF16(ws.c_str()));
}

inline std::wstring wstr(const juce::String& s)
{
    auto p = s.getCharPointer();
    if (p.isEmpty()) {
        return std::wstring();
    }
    std::wstring out;
    while (!p.isEmpty()) {
        out.push_back(static_cast<wchar_t>(*p));
        ++p;
    }
    return out;
}

} // namespace app