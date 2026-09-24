#include "readconf/PlaylistFileLocator.h"

#include <algorithm>
#include <ctime>

namespace readconf {
namespace {

// Nome do arquivo para a data de hoje no formato %d (ex.: "23") e %d-%m-%Y
// (ex.: "23-09-2026") e %d-%m-%y (ex.: "23-09-26"), conforme o Windows local.
std::wstring todayDayOfMonth();
std::wstring todayDateFullYear();
std::wstring todayDateShortYear();

std::tm localToday()
{
    const std::time_t now = std::time(nullptr);
    std::tm t{};
#if defined(_MSC_VER)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    return t;
}

std::wstring pad2(int v)
{
    const std::wstring s = std::to_wstring(v);
    return s.size() < 2 ? (L"0" + s) : s;
}

std::wstring todayDayOfMonth() { return pad2(localToday().tm_mday); }
std::wstring todayDateFullYear()
{
    const std::tm t = localToday();
    return pad2(t.tm_mday) + L"-" + pad2(t.tm_mon + 1) + L"-" +
           std::to_wstring(t.tm_year + 1900);
}
std::wstring todayDateShortYear()
{
    const std::tm t = localToday();
    return pad2(t.tm_mday) + L"-" + pad2(t.tm_mon + 1) + L"-" +
           pad2((t.tm_year + 1900) % 100);
}

// True se o nome tem a extensão de texto típica (.txt / .ini / vazio).
bool hasTextExtension(const std::wstring& name)
{
    const size_t dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos) {
        return true;
    }
    const std::wstring ext = name.substr(dot);
    return ext == L".txt" || ext == L".ini";
}

bool isDigits(const std::wstring& s, size_t expectedLength)
{
    if (s.size() != expectedLength) {
        return false;
    }
    return std::all_of(s.begin(), s.end(),
                       [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

// Aparecido no disco: "Mapa<dua>", "Mapa<dd>-<mm>-<yyyy>", "<dd>-<mm>-<yy>".
bool matchDayPattern(const std::wstring& name, const std::wstring& prefix)
{
    std::wstring base = name;
    if (hasTextExtension(name) && base.size() >= 4) {
        base = base.substr(0, base.find_last_of(L'.'));
    }
    if (base.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), base.begin())) {
        base = base.substr(prefix.size());
        return isDigits(base, 2);
    }
    return false;
}

bool matchDatePattern(const std::wstring& name, const std::wstring& prefix,
                      int yearDigits)
{
    std::wstring base = name;
    if (hasTextExtension(name) && base.size() >= 4) {
        base = base.substr(0, base.find_last_of(L'.'));
    }
    if (base.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), base.begin())) {
        base = base.substr(prefix.size());
    } else if (!prefix.empty()) {
        return false;
    }
    // dd-mm-yyyy / dd-mm-yy
    if (base.size() != 10 && base.size() != 8) {
        return false;
    }
    return isDigits(base.substr(0, 2), 2) && base[2] == L'-' &&
           isDigits(base.substr(3, 2), 2) && base[5] == L'-' &&
           isDigits(base.substr(6), static_cast<size_t>(yearDigits));
}

bool matchMakerPattern(const std::wstring& name, const std::wstring& prefix)
{
    return matchDatePattern(name, prefix, 2);
}

bool fileExistsIn(const std::filesystem::path& folder, const std::wstring& name)
{
    return std::filesystem::exists(folder / name);
}

std::vector<FileBinding> scanFor(const std::filesystem::path& folder,
                                 ConfigScope scope, FormatOption option)
{
    std::vector<FileBinding> out;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const std::wstring name = entry.path().filename().wstring();
        bool matches = false;
        const std::wstring prefix = (scope == ConfigScope::Musical) ? L"Grade"
                                                                    : L"Mapa";
        switch (option) {
        case FormatOption::CommercialDay:
            matches = matchDayPattern(name, prefix);
            break;
        case FormatOption::CommercialDate:
            matches = matchDatePattern(name, prefix, 4);
            break;
        case FormatOption::Maker:
            matches = matchMakerPattern(name, L"");
            break;
        default:
            break;
        }
        if (matches) {
            out.push_back({ name, true });
        }
    }
    std::sort(out.begin(), out.end(),
              [](const FileBinding& a, const FileBinding& b) {
                  return a.fileName < b.fileName;
              });
    return out;
}

} // namespace

std::filesystem::path folderFor(ConfigScope scope,
                                const std::filesystem::path& installationFolder)
{
    if (installationFolder.empty()) {
        return {};
    }
    const bool mapas = (scope == ConfigScope::Comercial ||
                        scope == ConfigScope::RelogioComercial);
    const std::wstring folderName = mapas ? L"mapas" : L"grades";

    // Procura por nomes de pasta em variações conhecidas (mapa/mapas etc.).
    for (const std::wstring& name : (mapas
                                         ? std::vector<std::wstring>{ L"mapas",
                                                                      L"mapa" }
                                         : std::vector<std::wstring>{ L"grades",
                                                                      L"grade" })) {
        const std::filesystem::path candidate = installationFolder / name;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return installationFolder / folderName;
}

std::vector<FileBinding> locateFiles(ConfigScope scope,
                                     FormatOption option,
                                     const std::filesystem::path& installationFolder)
{
    if (option == FormatOption::Unknown ||
        !optionAppliesTo(scope, option)) {
        return {};
    }

    const std::filesystem::path folder = folderFor(scope, installationFolder);
    std::vector<FileBinding> result;

    const std::wstring prefix =
        (scope == ConfigScope::Musical) ? L"Grade" : L"Mapa";
    const bool isRelogio = (scope == ConfigScope::RelogioComercial ||
                            scope == ConfigScope::RelogioMusical);
    const std::wstring singleBase =
        isRelogio ? L"Relogio.txt" : (prefix + L".txt");
    const std::wstring weeklyPrefix =
        isRelogio ? L"Relogio" : prefix;

    const auto existsHere = [&folder](const std::wstring& name) {
        if (folder.empty()) {
            return false;
        }
        return fileExistsIn(folder, name);
    };

    switch (option) {
    case FormatOption::Single:
        result.push_back({ singleBase, existsHere(singleBase) });
        break;

    case FormatOption::Weekly:
        for (const std::wstring& day : weekdayFileNames()) {
            const std::wstring name = weeklyPrefix + day + L".txt";
            result.push_back({ name, existsHere(name) });
        }
        break;

    case FormatOption::Auto:
        if (!isRelogio) {
            if (scope == ConfigScope::Comercial) {
                const std::wstring d = todayDateFullYear();
                result.push_back({ prefix + d, existsHere(prefix + d) });
                const std::wstring day = todayDayOfMonth();
                result.push_back({ prefix + day, existsHere(prefix + day) });
            } else { // musical
                const std::wstring d = todayDateFullYear();
                result.push_back({ d, existsHere(d) });
                const std::wstring day = todayDayOfMonth();
                result.push_back({ prefix + day, existsHere(prefix + day) });
            }
            for (const std::wstring& day : weekdayFileNames()) {
                const std::wstring name = prefix + day + L".txt";
                result.push_back({ name, existsHere(name) });
            }
            result.push_back({ singleBase, existsHere(singleBase) });
        }
        break;

    case FormatOption::CommercialDay:
        result = scanFor(folder, scope, option);
        if (result.empty()) {
            result.push_back({ prefix + todayDayOfMonth(), false });
        }
        break;

    case FormatOption::CommercialDate:
        result = scanFor(folder, scope, option);
        if (result.empty()) {
            result.push_back({ prefix + todayDateFullYear(), false });
        }
        break;

    case FormatOption::Maker:
        result = scanFor(folder, scope, option);
        if (result.empty()) {
            result.push_back({ todayDateShortYear(), false });
        }
        break;

    case FormatOption::Unknown:
        break;
    }
    return result;
}

} // namespace readconf