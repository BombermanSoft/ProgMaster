#include "playlist/FoldersXml.h"

#include <algorithm>

#include <cwctype>

#include <utility>

#include "core/Log.h"
#include "core/TextFileIO.h"

namespace {

enum class TagKind {
    Open,         // <Nome ...>
    Close,        // </Nome>
    SelfClosing,  // <Nome .../>
    Other,        // comentário, CDATA, declaração etc.
};

struct ParsedTag {
    TagKind kind = TagKind::Other;
    std::wstring name;  // nome local (prefixo de namespace removido)
    std::vector<std::pair<std::wstring, std::wstring>> attributes;  // nome=valor
    size_t after = 0;   // índice após o '>' do marcador
};

// Remove o prefixo de namespace de um nome de elemento (ex.: "f:Folder" -> "Folder").
std::wstring localName(const std::wstring& name)
{
    const size_t colon = name.find_last_of(L':');
    return (colon == std::wstring::npos) ? name : name.substr(colon + 1);
}

bool icaseEquals(const std::wstring& a, const std::wstring& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    return std::equal(a.begin(), a.end(), b.begin(),
                      [](wchar_t x, wchar_t y) {
                          return std::towlower(x) == std::towlower(y);
                      });
}

std::wstring trim(const std::wstring& s)
{
    const size_t begin = s.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) {
        return std::wstring();
    }
    const size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

bool isNameChar(wchar_t c)
{
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
           (c >= L'0' && c <= L'9') || c == L'_' || c == L'-' ||
           c == L':' || c == L'.';
}

// Extrai pares atributo=valor de uma sequência de atributos XML.
// Ex.: "Id=\"100\" Type=\"Estatica\"" -> [Id, 100], [Type, Estatica].
void parseAttributes(const std::wstring& s,
                     std::vector<std::pair<std::wstring, std::wstring>>& out)
{
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        while (i < n && (s[i] == L' ' || s[i] == L'\t' ||
                         s[i] == L'\r' || s[i] == L'\n')) {
            ++i;
        }
        if (i >= n) {
            break;
        }

        const size_t keyStart = i;
        while (i < n && isNameChar(s[i])) {
            ++i;
        }
        const std::wstring key = s.substr(keyStart, i - keyStart);
        if (key.empty()) {
            ++i;
            continue;
        }

        while (i < n && (s[i] == L' ' || s[i] == L'\t')) {
            ++i;
        }
        if (i >= n || s[i] != L'=') {
            continue;
        }
        ++i;
        while (i < n && (s[i] == L' ' || s[i] == L'\t')) {
            ++i;
        }
        if (i >= n) {
            break;
        }

        const wchar_t quote = s[i];
        if (quote != L'"' && quote != L'\'') {
            continue;
        }
        ++i;
        const size_t close = s.find(quote, i);
        if (close == std::wstring::npos) {
            i = n; // marcador malformado: interrompe a leitura de atributos
            break;
        }
        out.emplace_back(key, s.substr(i, close - i));
        i = close + 1;
    }
}

// Nome de elemento de REGISTRO de código: "Folder" seguido de dígitos
// opcionais (o Folders.xml real usa <Folder0>..<Folder15>). Não casa com
// "<Folders>" (que é a raiz/ contador) nem com os elementos do <Shared>.
bool isFolderElementName(const std::wstring& name)
{
    static const wchar_t kPrefix[] = L"folder";
    constexpr size_t kPrefixLen = 6;

    if (name.size() < kPrefixLen) {
        return false;
    }
    for (size_t i = 0; i < kPrefixLen; ++i) {
        if (std::towlower(static_cast<unsigned short>(name[i])) != kPrefix[i]) {
            return false;
        }
    }
    for (size_t i = kPrefixLen; i < name.size(); ++i) {
        if (name[i] < L'0' || name[i] > L'9') {
            return false;
        }
    }
    return true;
}

// Preenche o campo correspondente de um registro. A primeira ocorrência de
// cada campo dentro do mesmo <Folder> vence (as demais são ignoradas).
void assignField(FolderEntry& entry, const std::wstring& field, const std::wstring& value)
{
    if (value.empty()) {
        return;
    }
    // O DBFId é o identificador principal usado na programação do Playlist.
    if (icaseEquals(field, L"DBFId") && entry.dbfId.empty()) {
        entry.dbfId = value;
    } else if (icaseEquals(field, L"Title") && entry.title.empty()) {
        entry.title = value;
    } else if (icaseEquals(field, L"ID") && entry.id.empty()) {
        entry.id = value;
    } else if (icaseEquals(field, L"Type") && entry.type.empty()) {
        entry.type = value;
    } else if (icaseEquals(field, L"Target") && entry.target.empty()) {
        entry.target = value;
    }
}

// Interpreta o marcador XML que começa em xml[from] (deve ser '<').
// Preenche contentStart com a posição logo após o '>' quando o marcador é um
// elemento aberto (onde o texto do elemento começa).
ParsedTag parseTag(const std::wstring& xml, size_t from, size_t& contentStart)
{
    ParsedTag tag;
    contentStart = std::wstring::npos;
    const size_t n = xml.size();

    if (from >= n || xml[from] != L'<') {
        tag.after = std::min(from, n);
        return tag;
    }

    // Comentário <!-- ... -->
    if (xml.compare(from, 4, L"<!--") == 0) {
        const size_t end = xml.find(L"-->", from + 4);
        tag.after = (end == std::wstring::npos) ? n : end + 3;
        return tag;
    }

    // Seção CDATA <![CDATA[ ... ]]>
    if (xml.compare(from, 9, L"<![CDATA[") == 0) {
        const size_t end = xml.find(L"]]>", from + 9);
        tag.after = (end == std::wstring::npos) ? n : end + 3;
        return tag;
    }

    // Instrução de processamento <? ... ?> (inclui a declaração <?xml ...?>).
    if (from + 1 < n && xml[from + 1] == L'?') {
        size_t end = xml.find(L"?>", from + 2);
        if (end == std::wstring::npos) {
            end = xml.find(L'>', from + 2);
        }
        tag.after = (end == std::wstring::npos) ? n : end + 1;
        return tag;
    }

    // Declarações <!DOCTYPE ...> e afins (sem validação de conteúdo).
    if (from + 1 < n && xml[from + 1] == L'!') {
        const size_t end = xml.find(L'>', from + 2);
        tag.after = (end == std::wstring::npos) ? n : end + 1;
        return tag;
    }

    // Elemento normal.
    const size_t gt = xml.find(L'>', from + 1);
    if (gt == std::wstring::npos) {
        tag.after = n; // arquivo malformado: não há mais o que interpretar
        return tag;
    }

    std::wstring body = xml.substr(from + 1, gt - (from + 1));
    tag.after = gt + 1;

    bool closing = !body.empty() && body.front() == L'/';
    if (closing) {
        body.erase(0, 1);
    }

    const size_t nameEnd = body.find_first_of(L" \t\r\n");
    tag.name = localName(body.substr(0, nameEnd));

    // Atributos (ex.: <Folder Id="x" Type="y">): texto restante após o nome.
    std::wstring attrsPart;
    if (!closing && nameEnd != std::wstring::npos) {
        attrsPart = body.substr(nameEnd);
    }

    bool selfClosing = false;
    if (!closing && !attrsPart.empty()) {
        const size_t last = attrsPart.find_last_not_of(L" \t\r\n");
        if (last != std::wstring::npos && attrsPart[last] == L'/') {
            // <Nome .../> -> auto-fechado; remove o '/' antes de ler atributos.
            selfClosing = true;
            attrsPart.erase(last);
        }
        parseAttributes(attrsPart, tag.attributes);
    }

    if (closing) {
        tag.kind = TagKind::Close;
    } else if (selfClosing) {
        tag.kind = TagKind::SelfClosing;
    } else {
        tag.kind = TagKind::Open;
        contentStart = tag.after;
    }
    return tag;
}

} // namespace

std::wstring FoldersXml::readAll(const std::filesystem::path& path,
                                 std::vector<FolderEntry>& outEntries,
                                 std::string& technicalError)
{
    technicalError.clear();
    outEntries.clear();

    if (!std::filesystem::exists(path)) {
        technicalError = "folders.xml não encontrado: " + path.string();
        Log::info(L"folders.xml não encontrado em: " + path.wstring());
        return L"Não foi possível localizar o arquivo folders.xml na instalação do Playlist.";
    }

    TextFileResult result = TextFileIO::readWide(path);
    if (!result.ok) {
        technicalError = "Falha ao ler folders.xml: " + result.technicalError;
        Log::error(technicalError);
        return L"Não foi possível ler o arquivo folders.xml.";
    }

    // Parser simples, somente leitura, construído especificamente para a
    // estrutura esperada do folders.xml.
    //
    // Regras:
    //  - cada registro <Folder> / <Folder0> / <Folder1>... que abre cria um
    //    registro interno (no Folders.xml real os registros são <Folder0>..
    //    <Folder15>, e não <Folder>);
    //  - o bloco <Shared> contém as pastas COMPARTILHADAS da instalação
    //    (<Folder0><Name>..</Name><Path>..</Path>) e NÃO é um código: é
    //    ignorado por completo;
    //  - os campos são preenchidos com o texto direto dos elementos filhos;
    //  - registros sem <DBFId> continuam na lista (a interface os trata como
    //    "sem código", sem inventar um valor).
    std::vector<FolderEntry> stack;
    const std::wstring& xml = result.text;
    const size_t n = xml.size();

    // Diagnóstico: conta as ocorrências de marcadores relevantes, para que o
    // log mostre o que foi encontrado mesmo quando a estrutura esperada não
    // casar (ex.: arquivo com outro elemento raiz além de <Folder>).
    size_t folderOpenCount = 0;
    size_t folderSelfClosingCount = 0;
    size_t dbfIdTagCount = 0;
    size_t titleTagCount = 0;
    // Profundidade do bloco <Shared> (pastas compartilhadas, ignorado).
    size_t sharedDepth = 0;

    size_t pos = 0;
    while (pos < n) {
        const size_t lt = xml.find(L'<', pos);
        if (lt == std::wstring::npos) {
            break;
        }

        size_t contentStart = std::wstring::npos;
        ParsedTag tag = parseTag(xml, lt, contentStart);

        // Dentro de <Shared> nada é código: apenas conta a profundidade para
        // saber quando o bloco termina.
        if (sharedDepth > 0) {
            if (tag.kind == TagKind::Open) {
                ++sharedDepth;
            } else if (tag.kind == TagKind::Close) {
                --sharedDepth;
            }
            if (tag.after <= lt) {
                break;
            }
            pos = tag.after;
            continue;
        }

        if (tag.kind == TagKind::Open && icaseEquals(tag.name, L"Shared")) {
            sharedDepth = 1;
            if (tag.after <= lt) {
                break;
            }
            pos = tag.after;
            continue;
        }

        if (tag.kind == TagKind::Open || tag.kind == TagKind::Close ||
            tag.kind == TagKind::SelfClosing) {
            // Contagem diagnóstica (só tags de abertura).
            if (tag.kind != TagKind::Close) {
                if (icaseEquals(tag.name, L"DBFId")) {
                    ++dbfIdTagCount;
                } else if (icaseEquals(tag.name, L"Title")) {
                    ++titleTagCount;
                }
            }

            if (isFolderElementName(tag.name)) {
                if (tag.kind == TagKind::Open) {
                    ++folderOpenCount;
                    stack.push_back(FolderEntry());
                    // Atributos no próprio <Folder ...> (ex.: Id/Type/Target)
                    // são aplicados primeiro; valores em elementos filhos têm
                    // precedência igual apenas se o campo ainda estiver vazio
                    // (primeira ocorrência vence).
                    for (const auto& pair : tag.attributes) {
                        assignField(stack.back(), pair.first, pair.second);
                    }
                } else if (tag.kind == TagKind::Close && !stack.empty()) {
                    outEntries.push_back(stack.back());
                    stack.pop_back();
                } else if (tag.kind == TagKind::SelfClosing) {
                    ++folderSelfClosingCount;
                    // <Folder DBFId="..." Title="..."/> : registro completo e
                    // sem descendentes (os atributos são a única fonte).
                    FolderEntry entry;
                    for (const auto& pair : tag.attributes) {
                        assignField(entry, pair.first, pair.second);
                    }
                    outEntries.push_back(entry);
                }
            } else if (tag.kind == TagKind::Open && !stack.empty()) {
                // Captura o texto direto entre o marcador aberto e o próximo '<'.
                std::wstring value;
                if (contentStart != std::wstring::npos) {
                    const size_t nextLt = xml.find(L'<', contentStart);
                    value = (nextLt == std::wstring::npos)
                                ? xml.substr(contentStart)
                                : xml.substr(contentStart, nextLt - contentStart);
                }
                assignField(stack.back(), tag.name, trim(value));
            } else if (tag.kind == TagKind::SelfClosing && !stack.empty()) {
                // <DBFId/> ou <DBFId Value="x"/> — tenta atributos e texto.
                for (const auto& pair : tag.attributes) {
                    assignField(stack.back(), tag.name, pair.second);
                }
            }
        }

        if (tag.after <= lt) {
            // Proteção contra parser malformado (loop infinito).
            break;
        }
        pos = tag.after;
    }

    Log::info(L"[folders-xml] <Folder> abertos: " +
              std::to_wstring(folderOpenCount) +
              L"; auto-fechados: " + std::to_wstring(folderSelfClosingCount) +
              L"; tags <DBFId>: " + std::to_wstring(dbfIdTagCount) +
              L"; tags <Title>: " + std::to_wstring(titleTagCount) +
              L"; registros emitidos: " + std::to_wstring(outEntries.size()));

    // Fallback para estruturas que NÃO usam <Folder> como contêiner: se nada
    // foi emitido mas o documento contém <DBFId>, associa cada código ao
    // <Title> seguinte na ordem do documento (i-ésimo DBFId com i-ésimo Title).
    if (outEntries.empty() && dbfIdTagCount > 0) {
        std::vector<std::wstring> dbfIds;
        std::vector<std::wstring> titles;
        pos = 0;
        while (pos < n) {
            const size_t lt = xml.find(L'<', pos);
            if (lt == std::wstring::npos) {
                break;
            }
            size_t contentStartFallback = std::wstring::npos;
            ParsedTag tag = parseTag(xml, lt, contentStartFallback);
            if (tag.kind == TagKind::Open &&
                contentStartFallback != std::wstring::npos) {
                const size_t nextLt = xml.find(L'<', contentStartFallback);
                const std::wstring value = trim(
                    (nextLt == std::wstring::npos)
                        ? xml.substr(contentStartFallback)
                        : xml.substr(contentStartFallback,
                                     nextLt - contentStartFallback));
                if (!value.empty()) {
                    if (icaseEquals(tag.name, L"DBFId")) {
                        dbfIds.push_back(value);
                    } else if (icaseEquals(tag.name, L"Title")) {
                        titles.push_back(value);
                    }
                }
            }
            if (tag.after <= lt) {
                break;
            }
            pos = tag.after;
        }
        for (size_t i = 0; i < dbfIds.size(); ++i) {
            FolderEntry entry;
            entry.dbfId = dbfIds[i];
            if (i < titles.size()) {
                entry.title = titles[i];
            }
            outEntries.push_back(entry);
        }
        Log::info(L"[folders-xml] fallback de pareamento DBFId/Title ativado: " +
                  std::to_wstring(outEntries.size()) + L" registro(s).");
    }

    return std::wstring(); // sucesso
}