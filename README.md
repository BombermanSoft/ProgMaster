# ProgMaster

Programa de desktop para **Windows** (C++17) que complementa o sistema
**Playlist**: localiza o Playlist.exe, edita o `playlist.ini` (visualmente e
como bloco de notas), mantém as seções de programação organizadas e mostra a
lista de IDs do folders.xml — tudo com interface gráfica **JUCE** e núcleo
lógico em C++ puro.

Interface em português, três guias:

- **Editor** — edição textual mono-espaçada com ícones Salvar / Desfazer /
  Refazer, sub-abas por arquivo e o caminho do arquivo em edição no rodapé;
- **Configuração** — cartões por escopo (`[BLOCO COMERCIAL]`,
  `[BLOCO MUSICAL]`, `[RELOGIO COMERCIAL]`, `[RELOGIO MUSICAL]`) com escolha
  assistida do formato, mais o cartão `[AFILIADAS]` (nome | endereço | porta |
  ativa);
- **Códigos** — listagem (somente leitura) dos IDs do folders.xml da
  instalação do Playlist.

## Funcionalidades

- Localiza e valida o **Playlist.exe** (diálogo "Procurar...") e persiste o
  caminho em `%APPDATA%\ProgMaster\config.txt`;
- Edita o `playlist.ini` preservando a **codificação original** (o formato que
  o Playlist lê é ANSI/CP-ACP; arquivos UTF-8 são convertidos ao salvar e a
  gravação é recusada se algum caractere não couber — nada é corrompido);
- Lê e interpreta as seções de programação, **tolerante** a variações reais do
  arquivo (seções com acento, valores entre aspas, grafias antigas de dia da
  semana, `%a`/`%d`/`%d-%m-%Y`...);
- Reconhece o formato e padroniza `ARQUIVO=` (ex.: `GRADES\Grade.txt`,
  `MAPAS\Mapa%d.txt`, `GRADES\Grade%a.txt`), gravando com os nomes canônicos
  `PLAYLIST.ini` e `Sáb`;
- Ordena as seções na **ordem canônica** ao serializar e preserva seções
  desconhecidas;
- Altera todas as configurações **somente em memória** até clicar em Salvar
  (validação antes de gravar; erro claro quando falha);
- `✎ Visualizar como texto` grava e abre o `playlist.ini` no Bloco de Notas do
  Windows (volta à guia relendo o disco);
- Menu Editar abre os arquivos de programação (Programação / Mapa Comercial /
  Grades Musicais / Relógio Comercial / Relógio Musical);
- Confirmação **Salvar / Descartar alterações / Cancelar** ao fechar com
  pendências;
- Menus, abas e diálogos totalmente em português, com acentos corretos;

## Requisitos

- Windows (x64)
- Para **compilar**: MSVC Build Tools 2022, CMake >= 3.16, Git
  (o JUCE 8.0.15 é baixado automaticamente no primeiro configure)

Para **usar**: basta um único executável (CRT estático), sem redistribuível VC.

## Como compilar

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target ProgMaster ProgMasterCoreTests
```

Resultado:

```
build\ProgMaster_artefacts\Release\ProgMaster.exe
build\Release\ProgMasterCoreTests.exe
```

Estado esperado: 0 erros, 0 warnings (a suíte de testes roda 165/165).

## Como executar

```
build\ProgMaster_artefacts\Release\ProgMaster.exe
```

Na primeira execução, informe onde está o `Playlist.exe`. O caminho fica em
`%APPDATA%\ProgMaster\config.txt` (arquivos técnicos: `config.txt` UTF-8 sem
BOM e `log.txt` UTF-8 com BOM).

## Testes

```
build\Release\ProgMasterCoreTests.exe
```

Os testes cobrem regras de formato, parse/re-serialização do `.ini`,
localização de arquivos por escopo e validação de afiliadas. Manual:

- `docs\ROTEIRO_DE_TESTES.txt` — roteiro de testes manuais
- `docs\CONTEXTO_PROJETO.txt` — histórico, decisões e lista do que está
  implementado
- `docs\ARQUITETURA_DO_CODIGO.txt` — arquitetura e detalhes técnicos

## Estrutura do projeto

```
├─ CMakeLists.txt          Build: núcleo + app JUCE + testes + binários
├─ resources\              Ícone e recursos do executável
├─ docs\                   Contexto, arquitetura e roteiro de testes
├─ build\                  Artefatos (não versionar)
└─ src\
   ├─ core\                Núcleo Etapa 1 (Settings, Log, TextFileIO) — sem JUCE
   ├─ models\              FolderEntry (registro do folders.xml)
   ├─ playlist\            PlaylistLocator, PlaylistInstallation, PlaylistIni,
   │                       FoldersXml (somente leitura)
   ├─ readconf\            Núcleo Etapa 2: regras/leitura dos formatos do
   │                       playlist.ini (+ tests\RulesTests.cpp)
   └─ app\                 Interface JUCE: janela, menus e as 3 guias
```

## Documentação

- `docs\CONTEXTO_PROJETO.txt` — visão geral, o que está implementado, decisões
  técnicas, testes realizados e limitações
- `docs\ARQUITETURA_DO_CODIGO.txt` — arquitetura detalhada
- `docs\ROTEIRO_DE_TESTES.txt` — roteiro de testes manuais do executável