#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "playlist/FoldersXml.h"
#include "models/FolderEntry.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what, const char* file, int line)
{
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cout << "  FALHOU " << file << ":" << line << " - " << what << "\n";
    }
}

#define CHECK_MSG(cond, what) check((cond), what, __FILE__, __LINE__)

void checkEq(const std::wstring& actual, const std::wstring& expected,
             const std::string& what, const char* file, int line)
{
    ++g_checks;
    if (actual != expected) {
        ++g_failures;
        const auto toDisplay = [](const std::wstring& s) {
            std::string r;
            for (wchar_t c : s) {
                r += (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
            }
            return r;
        };
        std::cout << "  FALHOU " << file << ":" << line << " - " << what
                  << "\n      esperado: [" << toDisplay(expected)
                  << "]\n      obtido:   [" << toDisplay(actual) << "]\n";
    }
}

#define CHECK_EQ(actual, expected) \
    checkEq((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)

// Escreve um arquivo temporário e devolve o caminho.
std::filesystem::path writeTemp(const std::wstring& name,
                                const std::wstring& content)
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / L"progmaster_blocos_tests";
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / name;
    std::ofstream out(path, std::ios::binary);
    const std::string narrow(content.begin(), content.end());
    out << narrow;
    out.close();
    return path;
}

// ============================================================================
// Estrutura REAL do Folders.xml do Playlist: raiz <Folders> com contador e
// versão, um bloco <Shared> (pastas compartilhadas, SEM DBFId) e os registros
// de código <Folder0>..<Folder15> com ID/Title/Type/Target/DBFId.
// ============================================================================
const wchar_t* kFoldersXmlReal =
    L"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
    L"<Folders>\r\n"
    L"  <Folders>16</Folders>\r\n"
    L"  <Version>1.2</Version>\r\n"
    L"  <Shared>\r\n"
    L"    <Folders>7</Folders>\r\n"
    L"    <Server>C27</Server>\r\n"
    L"    <Folder0>\r\n"
    L"      <Name>C</Name>\r\n"
    L"      <Path>C:\\</Path>\r\n"
    L"    </Folder0>\r\n"
    L"    <Folder1>\r\n"
    L"      <Name>Grades</Name>\r\n"
    L"      <Path>C:\\Playlist\\pgm\\Grades</Path>\r\n"
    L"    </Folder1>\r\n"
    L"  </Shared>\r\n"
    L"  <Folder0>\r\n"
    L"    <ID>1</ID>\r\n"
    L"    <Title>Chamadas</Title>\r\n"
    L"    <Type></Type>\r\n"
    L"    <Target>C:\\Acervo\\Chamadas</Target>\r\n"
    L"    <DBFId></DBFId>\r\n"
    L"  </Folder0>\r\n"
    L"  <Folder1>\r\n"
    L"    <ID>2</ID>\r\n"
    L"    <Title>Comerciais</Title>\r\n"
    L"    <Type>$</Type>\r\n"
    L"    <Target>C:\\Acervo\\Comerciais</Target>\r\n"
    L"    <DBFId>COMER</DBFId>\r\n"
    L"  </Folder1>\r\n"
    L"  <Folder2>\r\n"
    L"    <ID>3</ID>\r\n"
    L"    <Title>Internacionais</Title>\r\n"
    L"    <Type>M</Type>\r\n"
    L"    <Target>C:\\Acervo\\Musicas\\Internacionais</Target>\r\n"
    L"    <DBFId>INT</DBFId>\r\n"
    L"  </Folder2>\r\n"
    L"  <Folder3>\r\n"
    L"    <ID>4</ID>\r\n"
    L"    <Title>VH Abertura Encerramento</Title>\r\n"
    L"    <Type>V</Type>\r\n"
    L"    <Target>C:\\Acervo\\Vinhetas\\VH Abertura Encerramento</Target>\r\n"
    L"    <DBFId>VHAB</DBFId>\r\n"
    L"  </Folder3>\r\n"
    L"  <Folder4>\r\n"
    L"    <ID>5</ID>\r\n"
    L"    <Title>Nacionais</Title>\r\n"
    L"    <Type>M</Type>\r\n"
    L"    <Target>C:\\Acervo\\Musicas\\Nacionais</Target>\r\n"
    L"    <DBFId>NAC</DBFId>\r\n"
    L"  </Folder4>\r\n"
    L"</Folders>\r\n";

// Conta os registros que têm código (DBFId) — é a "Lista de Códigos" usada
// pelo editor visual de Mapas/Grades.
int countCodes(const std::vector<FolderEntry>& entries)
{
    int n = 0;
    for (const FolderEntry& e : entries) {
        if (!e.dbfId.empty()) {
            ++n;
        }
    }
    return n;
}

const FolderEntry* findByDbfId(const std::vector<FolderEntry>& entries,
                               const std::wstring& dbfId)
{
    for (const FolderEntry& e : entries) {
        if (e.dbfId == dbfId) {
            return &e;
        }
    }
    return nullptr;
}

// Lê o arquivo real da estrutura e devolve os registros.
std::vector<FolderEntry> readReal(std::wstring& userMessage)
{
    const std::filesystem::path path = writeTemp(L"Folders.xml", kFoldersXmlReal);
    std::vector<FolderEntry> entries;
    std::string techErr;
    userMessage = FoldersXml::readAll(path, entries, techErr);
    return entries;
}

void testEstruturaReal()
{
    std::wstring userMessage;
    const std::vector<FolderEntry> entries = readReal(userMessage);

    CHECK_MSG(userMessage.empty(), "leitura sem mensagem de erro");
    // 5 registros de código + os 2 do <Shared> que NÃO podem entrar.
    CHECK_MSG(entries.size() == 5,
              "apenas os 5 registros de codigo (bloco <Shared> ignorado)");
    CHECK_MSG(countCodes(entries) == 4,
              "4 registros com DBFId nao vazio (COMER, INT, VHAB, NAC)");

    // O par DBFId/Title precisa sair do MESMO registro (antes o fallback
    // pareava pela ordem do documento e trocava os nomes).
    const FolderEntry* comer = findByDbfId(entries, L"COMER");
    CHECK_MSG(comer != nullptr, "registro COMER encontrado");
    if (comer) {
        CHECK_EQ(comer->title, L"Comerciais");
        CHECK_EQ(comer->type, L"$");
        CHECK_EQ(comer->target, L"C:\\Acervo\\Comerciais");
    }

    const FolderEntry* vhab = findByDbfId(entries, L"VHAB");
    CHECK_MSG(vhab != nullptr, "registro VHAB encontrado");
    if (vhab) {
        CHECK_EQ(vhab->title, L"VH Abertura Encerramento");
        CHECK_EQ(vhab->type, L"V");
    }

    const FolderEntry* intl = findByDbfId(entries, L"INT");
    CHECK_MSG(intl != nullptr, "registro INT encontrado");
    if (intl) {
        CHECK_EQ(intl->title, L"Internacionais");
        CHECK_EQ(intl->target, L"C:\\Acervo\\Musicas\\Internacionais");
    }

    // Registro sem DBFId continua na lista (a interface mostra "sem codigo").
    bool achouSemCodigo = false;
    for (const FolderEntry& e : entries) {
        if (e.dbfId.empty() && e.title == L"Chamadas") {
            achouSemCodigo = true;
        }
    }
    CHECK_MSG(achouSemCodigo, "registro sem DBFId preservado");
}

// O bloco <Shared> tem <Folder0><Name>/<Path>: nenhum deles pode virar um
// código (eles sao pastas compartilhadas, nao pastas de acervo).
void testSharedIgnorado()
{
    std::wstring userMessage;
    const std::vector<FolderEntry> entries = readReal(userMessage);

    for (const FolderEntry& e : entries) {
        CHECK_MSG(e.title != L"Grades", "pasta compartilhada <Shared> ignorada");
        CHECK_MSG(e.title != L"C", "pasta compartilhada <Shared> ignorada (C)");
    }
}

// Estrutura alternativa com <Folder> (sem dígitos) continua funcionando.
void testFolderSemDigitos()
{
    const std::filesystem::path path = writeTemp(
        L"Folders_antigo.xml",
        L"<?xml version=\"1.0\"?>\r\n"
        L"<Folders>\r\n"
        L"  <Folder>\r\n"
        L"    <Title>Comerciais</Title>\r\n"
        L"    <DBFId>COMER</DBFId>\r\n"
        L"  </Folder>\r\n"
        L"  <Folder>\r\n"
        L"    <Title>Musicais</Title>\r\n"
        L"    <DBFId>MUSI</DBFId>\r\n"
        L"  </Folder>\r\n"
        L"</Folders>\r\n");

    std::vector<FolderEntry> entries;
    std::string techErr;
    const std::wstring userMessage =
        FoldersXml::readAll(path, entries, techErr);

    CHECK_MSG(userMessage.empty(), "leitura do formato <Folder> sem erro");
    CHECK_MSG(entries.size() == 2, "dois registros no formato <Folder>");
    if (entries.size() == 2) {
        CHECK_EQ(entries[0].dbfId, L"COMER");
        CHECK_EQ(entries[0].title, L"Comerciais");
        CHECK_EQ(entries[1].dbfId, L"MUSI");
    }
}

} // namespace

int main()
{
    std::cout << "ProgMaster FoldersXml Tests (Etapa 4)\n\n";

    testEstruturaReal();
    testSharedIgnorado();
    testFolderSemDigitos();

    std::cout << g_checks << "/" << g_checks << " ok"
              << (g_failures == 0 ? "" : " (com falhas)") << "\n";
    if (g_failures == 0) {
        std::cout << "TODOS OS TESTES PASSARAM\n";
        return 0;
    }
    std::cout << g_failures << " FALHA(S)\n";
    return 1;
}
