#pragma once

#include <filesystem>

#include <string>

// Registro técnico da aplicação em %APPDATA%\ProgMaster\log.txt.
//
// Objetivo: reter o detalhe técnico de erros para diagnóstico, enquanto a
// interface apresenta ao usuário apenas mensagens claras e amigáveis.
class Log {
public:
    // Cria a pasta e o arquivo de log se necessário.
    static void init();

    static void info(const std::wstring& message);
    static void error(const std::wstring& message);

    // Sobrecargas para mensagens curtas codificadas em UTF-8.
    static void info(const std::string& message);
    static void error(const std::string& message);

private:
    static std::filesystem::path filePath();
    static void append(const std::wstring& line);
};