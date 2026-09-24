#include "readconf/ReadingConfiguration.h"

#include <cwchar>
#include <set>

#include "readconf/PlaylistFileLocator.h"

namespace readconf {

std::vector<ScopeSnapshot> readScopes(const PlaylistIniDocument& doc,
                                      const std::filesystem::path& installationFolder)
{
    return {
        readScope(doc, ConfigScope::Comercial, installationFolder),
        readScope(doc, ConfigScope::Musical, installationFolder),
        readScope(doc, ConfigScope::RelogioComercial, installationFolder),
        readScope(doc, ConfigScope::RelogioMusical, installationFolder),
    };
}

ScopeSnapshot readScope(const PlaylistIniDocument& doc,
                        ConfigScope scope,
                        const std::filesystem::path& installationFolder)
{
    ScopeSnapshot snap;
    snap.scope = scope;

    const int secIdx = doc.sectionIndex(scope);
    if (secIdx < 0) {
        // Seção ausente: não há configuração para apresentar.
        return snap;
    }
    snap.present = true;

    std::wstring formato;
    if (doc.sectionKeyValue(scope, L"formato", formato)) {
        snap.formatoAsWritten = formato;
    }
    std::wstring arquivo;
    if (doc.sectionKeyValue(scope, L"arquivo", arquivo)) {
        snap.arquivoAsWritten = arquivo;
    }

    const FormatMatch match = interpret(scope, snap.formatoAsWritten,
                                        snap.arquivoAsWritten);
    snap.option = match.option;
    snap.matchedToken = match.matchedToken;

    if (snap.option == FormatOption::Unknown) {
        // Mantém as chaves lidas para a interface exibir "não reconhecido".
        return snap;
    }

    snap.files = locateFiles(scope, snap.option, installationFolder);
    return snap;
}

std::vector<PlaylistIniDocument::Afiliada> readAfiliadas(
    const PlaylistIniDocument& doc)
{
    return doc.afiliadas();
}

ValidationResult validateForSave(const PlaylistIniDocument& doc)
{
    ValidationResult result;

    const auto afiliadas = doc.afiliadas();
    std::set<std::wstring> seenActive;
    for (size_t i = 0; i < afiliadas.size(); ++i) {
        const auto& a = afiliadas[i];
        if (a.address.empty()) {
            result.errorMessage =
                L"A afiliada na posição " + std::to_wstring(i + 1) +
                L" não tem endereço. Preencha Endereço ou remova a afiliada.";
            return result;
        }
        if (a.portText.empty()) {
            result.errorMessage =
                L"A afiliada \"" + a.address + L"\" não tem porta. " +
                L"Preencha a porta (ex.: 3030) ou remova a afiliada.";
            return result;
        }
        bool numeric = !a.portText.empty();
        for (wchar_t c : a.portText) {
            if (c < L'0' || c > L'9') {
                numeric = false;
                break;
            }
        }
        if (!numeric) {
            result.errorMessage =
                L"A porta da afiliada \"" + a.address + L"\" não é numérica " +
                L"(\"" + a.portText + L"\"). Use apenas dígitos.";
            return result;
        }
        const long port = std::wcstol(a.portText.c_str(), nullptr, 10);
        if (port < 1 || port > 65535) {
            result.errorMessage =
                L"A porta da afiliada \"" + a.address + L"\" está fora do " +
                L"intervalo válido (1 a 65535): \"" + a.portText + L"\".";
            return result;
        }
        if (!a.disabled) {
            const std::wstring key = a.address + L":" + a.portText;
            if (!seenActive.insert(key).second) {
                result.errorMessage =
                    L"Afiliada duplicada: \"" + key + L"\". " +
                    L"Remova uma das entradas.";
                return result;
            }
        }
    }

    result.ok = true;
    return result;
}

} // namespace readconf