#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "models/FolderEntry.h"

// Área da guia "Códigos": exibe em uma lista todos os registros lidos do
// folders.xml. A coluna principal é o DBFId; a coluna Nome (Title) auxilia
// a identificação visual do registro.
class CodesPageView {
public:
    enum : int {
        IDC_CODES_LIST = 3001,
        IDC_CODES_STATUS = 3002,
        IDC_CODES_SUMMARY = 3003,
    };

    void create(HWND parent, HINSTANCE hInstance, HFONT uiFont);
    void layout(const RECT& pageRect);
    void setVisible(bool visible);

    // Preenche a lista com os registros e atualiza o resumo na barra da página.
    void populate(const std::vector<FolderEntry>& entries,
                  const std::wstring& sourcePath);

    // Exibe uma mensagem (erro/aviso) no lugar da lista.
    void showMessage(const std::wstring& message);

    void clear();

    // Quantidade de linhas atualmente exibidas (usada em diagnóstico).
    int itemCount() const;

private:
    HWND m_hParent = nullptr;
    HWND m_hList = nullptr;
    HWND m_hSummary = nullptr;  // linha de resumo (totais/duplicados)
    HWND m_hStatus = nullptr;   // fonte do folders.xml (rodapé da página)
};