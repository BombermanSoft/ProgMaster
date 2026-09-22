#pragma once

#include <windows.h>

#include <memory>

// Núcleo da aplicação: cria os componentes, configura o estado inicial e
// executa o loop de mensagens do Windows. Mantido pequeno de propósito; as
// responsabilidades ficam nos componentes (Settings, PlaylistLocator,
// MainWindow).
class Application {
public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run(HINSTANCE hInstance, int nCmdShow);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};