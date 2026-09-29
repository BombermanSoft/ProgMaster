#pragma once

#include <filesystem>
#include <string>
#include <vector>

// ============================================================================
// Catálogo de CÓDIGOS de bloco (Etapa 4) — a "Lista de Códigos" que o editor
// visual oferece para atribuir aos horários de Mapas e Grades.
//
// A fonte É o folders.xml da instalação (campo DBFId de cada registro), lido
// pelo FoldersXml em MODO SOMENTE LEITURA. Nenhuma função desta classe grava o
// folders.xml — é regra do projeto desde a Etapa 1.
//
// Códigos NOVOS criados pelo usuário no editor visual NÃO são gravados no
// folders.xml: não existe sintaxe confirmada para isso e o arquivo original
// deve permanecer intacto. Eles existem apenas como "códigos de sessão",
// disponíveis no editor enquanto o programa estiver aberto.
//
// A cor NÃO faz parte do catálogo: cores são configuração visual do ProgMaster
// (o arquivo do Playlist não guarda cores), então ficam na camada de interface.
// ============================================================================

namespace blocos {

struct CodeEntry {
    std::wstring code;   // DBFId: "COMER", "MUSI", "VHAB"...
    std::wstring title;  // Title do registro (auxiliar, pode vir vazio)
    bool sessionOnly = false; // true = criado nesta sessão, não está no arquivo
};

class CodeCatalogue {
public:
    // Lê a Lista de Códigos do folders.xml (somente leitura) e substitui o
    // conteúdo atual. Os códigos de sessão são mantidos (são do usuário).
    // Devolve string vazia em sucesso; em erro, uma mensagem amigável.
    std::wstring loadFromFoldersXml(const std::filesystem::path& foldersXmlPath,
                                    std::string& technicalError);

    // Cria um código de sessão. Devolve false (e explica em `error`) quando o
    // código é inválido ou já existe.
    bool addSessionCode(const std::wstring& code, const std::wstring& title,
                        std::wstring& error);

    // Remove um código de SESSÃO. Devolve false para códigos do arquivo
    // (folders.xml nunca é alterado por esta classe).
    bool removeSessionCode(const std::wstring& code);

    const std::vector<CodeEntry>& entries() const { return m_entries; }
    size_t size() const { return m_entries.size(); }
    bool empty() const { return m_entries.empty(); }

    // Índice do código (comparação sem diferenciar maiúsculas/minúsculas),
    // ou -1 quando não existe. Serve para escolher a cor/posição na interface.
    int indexOf(const std::wstring& code) const;

    // Verdadeiro quando o código existe no catálogo (do arquivo ou de sessão).
    bool contains(const std::wstring& code) const;

    // Um código é válido quando tem 1..16 caracteres e SOMENTE letras, números
    // ou underscore, sem espaços. É a única regra de formato imposta aqui, e
    // existe para o usuário não digitar coisas que o Playlist não aceitaria
    // como DBFId (o manual descreve o código como "letras e números").
    static bool isValidCode(const std::wstring& code, std::wstring& error);

private:
    // Procura (sem diferenciar caixa) o código na lista informada.
    static int indexOfIn(const std::vector<CodeEntry>& list,
                         const std::wstring& code);

    std::vector<CodeEntry> m_entries;
};

} // namespace blocos
