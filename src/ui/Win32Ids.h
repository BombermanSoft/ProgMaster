#pragma once

#include <windows.h>

// Conversão de um ID numérico de controle em HMENU, usada no parâmetro hmenu
// de CreateWindowExW quando se cria apenas um controle filho (sem menu real).
//
// É usada por todas as janelas (principal, editor, página de códigos e
// diálogo de localização) para evitar a duplicação do cast C4312 em x64.
// Escopo intencionalmente mínimo: apenas esta utilidade de ID de controle.
namespace Win32Ids {

inline HMENU menuFromId(int id)
{
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

} // namespace Win32Ids