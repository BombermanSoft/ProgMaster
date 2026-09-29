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

// De onde veio o código da Lista de Códigos. A distinção é só de APRESENTAÇÃO
// (a Lista mostra a procedência na dica e na borda do botão); o comportamento
// de gravação é o mesmo para os dois casos de sessão: nada vai para o
// folders.xml.
enum class CodeOrigin {
    FoldersXml,     // veio da Lista de Códigos da instalação (registro DBFId)
    FromBlockFile,  // o ARQUIVO DE BLOCO usa, mas o folders.xml não lista
    CreatedByUser   // criado pelo usuário com o "+" da paleta
};

struct CodeEntry {
    std::wstring code;   // DBFId: "COMER", "MUSI", "VHAB"...
    std::wstring title;  // Title do registro (auxiliar, pode vir vazio)
    CodeOrigin origin = CodeOrigin::FoldersXml;
    // true = NÃO está no folders.xml — criado pelo usuário OU adotado de um
    // arquivo de bloco. Só estes podem ser removidos pela lixeira.
    bool sessionOnly = false;
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

    // Inclui na Lista de Códigos os códigos que o ARQUIVO DE BLOCO usa mas que o
    // folders.xml NÃO lista. É o caso do SOR, usado no "GRADE - Copia.txt" da
    // instalação oficial: o editor desenhava o chip CINZA (sem cor, porque o
    // catálogo não tinha o código) e não deixava armar nem escolher o código no
    // combo "Aplicar em". Depois desta chamada ele ganha cor, botão na paleta e
    // entrada no combo.
    //
    // O código NÃO é validado pelo formato de digitação: quem está falando é o
    // arquivo real, não o usuário, e o arquivo manda. Só entradas em branco são
    // ignoradas. Códigos repetidos na lista não viram botões duplicados, e os que
    // já existem no catálogo são preservados como estão (nada é sobrescrito).
    // O folders.xml nunca é alterado. Devolve quantos códigos foram incluídos.
    int adoptCodesFromFile(const std::vector<std::wstring>& codes);

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
