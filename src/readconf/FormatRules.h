#pragma once

#include <string>
#include <vector>

// ============================================================================
// Regras de interpretação e geração dos formatos do playlist.ini (Etapa 2).
//
// Este componente é o ÚNICO do programa que conhece o significado real das
// chaves FORMATO= e ARQUIVO= do playlist.ini. Ele traduz as opções amigáveis
// da interface (AUTO, Mapa/Grade (único), Semanal, Commercial Dia,
// Commercial Data, Maker, Único) para a sintaxe real e vice-versa.
//
// NENHUMA outra camada (interface, documentos, testes de arquivo) deve repetir
// estas regras: tudo passa por aqui. O componente é PURO: não acessa disco,
// não depende de Windows, não conhece a instalação.
//
// Tabela de mapeamento (sintaxe que o Playlist entende, conforme a instalação
// real de referência — C:\Playlist_oficial\pgm\PLAYLIST.ini — e os ajustes
// pedidos pelo usuário na Fase 2):
//
//   BLOCO COMERCIAL:
//     AUTO              ->  FORMATO=AUTO
//     Mapa/Grade (único)->  FORMATO=TXT1 ARQUIVO=MAPAS\Mapa.txt
//     Semanal           ->  FORMATO=TXT1 ARQUIVO=MAPAS\Mapa%a.txt
//     Commercial Dia    ->  FORMATO=TXT1 ARQUIVO=MAPAS\Mapa%d
//     Commercial Data   ->  FORMATO=TXT1 ARQUIVO=MAPAS\Mapa%d-%m-%Y
//     Planner           ->  FORMATO=TXT1 ARQUIVO=MAPAS\%d-%m-%Y.TXT
//   BLOCO MUSICAL:
//     AUTO              ->  FORMATO=AUTO
//     Mapa/Grade (único)->  FORMATO=TXT1 ARQUIVO=grades\Grade.txt
//     Semanal           ->  FORMATO=TXT1 ARQUIVO=grades\Grade%a.txt
//     Maker             ->  FORMATO=TXT1 ARQUIVO=GRADES\%d-%m-%Y.TXT
//   RELÓGIO COMERCIAL:
//     Único             ->  FORMATO=TXT1 ARQUIVO=Mapas\Relogio.txt
//     Semanal           ->  FORMATO=TXT1 ARQUIVO=Mapas\Relogio%a.txt
//   RELÓGIO MUSICAL:
//     Único             ->  FORMATO=TXT1 ARQUIVO=GRADES\Relogio.txt
//     Semanal           ->  FORMATO=TXT1 ARQUIVO=GRADES\Relogio%a.txt
//
// Em TODAS as seções a ordem das chaves é FORMATO antes de ARQUIVO (abaixo do
// cabeçalho da seção, acima do arquivo), como no arquivo real de referência.
//
// Leitura (interpretação) é tolerante: além dos padrões acima, reconhece
// também Mapa%a (semana abreviado), a grafia de dias por extenso
// (MapaSeg.txt..MapaDom.txt), o padrão de data antigo sem hífen "%d-%m%Y" e o
// Maker antigo "%d-%m-%y". O que não reconhece NUNCA é apagado — é preservado
// no documento (retornado como "não reconhecido").
//
// Afiliadas ([AFILIADAS]): a chave é o NOME configurável da afiliada
// (AFILIADA é apenas o exemplo do manual). Sintaxe: NOME = endereco:porta.
// ============================================================================

namespace readconf {

// Escopo de configuração: qual seção do playlist.ini está sendo lida/escrita.
enum class ConfigScope {
    Comercial,           // [BLOCO COMERCIAL]
    Musical,             // [BLOCO MUSICAL]
    RelogioComercial,    // [RELOGIO COMERCIAL]
    RelogioMusical,      // [RELOGIO MUSICAL]
    Afiliadas,           // [AFILIADAS]  (sem formato)
};

// Nome real e canônico da seção no playlist.ini para o escopo.
std::wstring sectionNameFor(ConfigScope scope);

// Rótulo amigável do escopo (cabeçalho do cartão na interface).
std::wstring scopeDisplayName(ConfigScope scope);

// Opções amigáveis de formato selecionáveis na interface.
enum class FormatOption {
    Auto,             // "AUTO"
    Single,           // "Mapa/Grade (único)" / "Único" (relógios)
    Weekly,           // "Semanal"
    CommercialDay,    // "Commercial Dia"   (somente Comercial)
    CommercialDate,   // "Commercial Data"  (somente Comercial)
    Planner,          // "Planner"          (somente Comercial)
    Maker,            // "Maker"            (somente Musical)
    Unknown,          // texto atual do playlist.ini não reconhecido
};

// Opções de formato oferecidas para o escopo (ordem de exibição).
std::vector<FormatOption> optionsForFormat(ConfigScope scope);

// Rótulo amigável da opção (texto no ComboBox). O nome do formato "único"
// depende do escopo (Mapa Único, Grade Única ou Relógio Único).
std::wstring displayName(ConfigScope scope, FormatOption option);

// True se a opção é aplicável ao escopo.
bool optionAppliesTo(ConfigScope scope, FormatOption option);

// ---------------------------------------------------------------------------
// INTERPRETAÇÃO (playlist.ini -> opção amigável)
// ---------------------------------------------------------------------------

struct FormatMatch {
    FormatOption option = FormatOption::Unknown;
    // Trecho real reconhecido (ex.: L"%w", L"Mapa%d-%m%Y", L"MapaSeg").
    std::wstring matchedToken;
    // True quando a opção foi identificada por heurística (por exemplo o dia
    // do mês) e não por um padrão explícito; sujeita a conferência.
    bool inferred = false;

    bool valid() const { return option != FormatOption::Unknown; }
};

// Interpreta o par (FORMATO, ARQUIVO) lido de uma seção e devolve a opção
// amigável. Valores vazios (seção sem as chaves) devolvem Unknown.
FormatMatch interpret(ConfigScope scope,
                      const std::wstring& formatoValue,
                      const std::wstring& arquivoValue);

// Interpreta APENAS o valor do ARQUIVO (para escopos que não usam
// FORMATO=AUTO, ex.: relógios que sempre são TXT1).
FormatMatch interpretArquivo(ConfigScope scope, const std::wstring& arquivoValue);

// ---------------------------------------------------------------------------
// GERAÇÃO (opção amigável -> texto real a gravar)
// ---------------------------------------------------------------------------

// Linhas CHAVE=VALOR (sem a seção) que representam a opção no playlist.ini.
struct GeneratedFormat {
    std::vector<std::wstring> lines;
    // true quando a opção (AUTO) deve REMOVER a chave ARQUIVO da seção.
    bool removesArquivo = false;
};

// Gera as linhas para gravar a opção escolhida no escopo. Se a opção não for
// aplicável ao escopo, retorna lista vazia.
GeneratedFormat generate(ConfigScope scope, FormatOption option);

// ---------------------------------------------------------------------------
// Localização de arquivos (regras de busca do ProgMaster)
// ---------------------------------------------------------------------------

// Nomes-de-dia da semana em PT-BR, na ordem real e com as grafias usadas
// pelo Playlist para arquivos semanais (MapaSeg, MapaTer, MapaQua, MapaQui,
// MapaSex, MapaSáb, MapaDom). A grafia canônica do sábado é "Sáb" (Manual);
// a comparação/tolerância também enxerga a grafia antiga "Sab" (ASCII).
const std::vector<std::wstring>& weekdayFileNames();

// Nomes de arquivo esperados para a opção, em ordem de prioridade da regra
// AUTO ("MapaDD-MM-AAAA, MapaDD, MapaSeg..MapaDom, Mapa.txt"). A lista usa
// os marcadores DD e MM (não substitui pela data de hoje) para que a camada
// de localização resolva contra o disco.
std::vector<std::wstring> expectedFileBases(ConfigScope scope,
                                            FormatOption option);

} // namespace readconf