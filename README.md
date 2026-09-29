# ProgMaster

Programa de desktop para **Windows** (C++17) que complementa o sistema
**Playlist**: localiza o Playlist.exe, edita o `playlist.ini` (visualmente e
como bloco de notas), edita visualmente os Mapas, Grades e Relógios, mantém as
seções de programação organizadas e mostra a lista de IDs do `folders.xml` — tudo
com interface gráfica **JUCE** e núcleo lógico em C++ puro.

Interface em português, três guias:

- **Editor** — edição textual mono-espaçada com ícones Salvar / Desfazer /
  Refazer, sub-abas por arquivo e o caminho do arquivo em edição no rodapé;
- **Configuração** — cartões por escopo (`[BLOCO COMERCIAL]`,
  `[BLOCO MUSICAL]`, `[RELOGIO COMERCIAL]`, `[RELOGIO MUSICAL]`) com escolha
  assistida do formato, mais o cartão `[AFILIADAS]` (nome | endereço | porta |
  ativa);
- **Códigos** — listagem (somente leitura) dos IDs do `folders.xml` da
  instalação do Playlist.

O menu **Editar** traz os editores **visuais** (Relógio e Blocos) e o menu
**Avançado** traz a edição **textual** dos mesmos arquivos.

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
- Confirmação **Salvar / Descartar alterações / Cancelar** ao fechar com
  pendências;
- Menus, abas e diálogos totalmente em português, com acentos corretos;

### Editor visual dos Relógios

- Cada horário (`HH:MM`) em uma linha; os parâmetros reconhecidos viram **chips
  coloridos** e o restante da linha (conteúdo preservado) fica em cinza;
- Paleta de parâmetros **grudada no mouse**: clicar num parâmetro o arma, um
  chip fantasma segue o cursor e o clique num horário o adiciona (cancelar:
  clicar no mesmo parâmetro, botão direito ou `ESC`);
- **Início** (00:00), **Intervalo** (horário do meio entre dois vizinhos) e
  **Avulso** (HH:MM digitado);
- **Copiar / Colar** de parâmetros compartilhado entre relógios e sub-abas;
- Modos **Visual | Texto** sobre o **mesmo documento em memória** (alternar não
  perde nada).

### Editor visual de Mapas e Grades (Blocos)

- Lê o **formato real** dos arquivos (`Mapa.txt`, `GRADE.txt` e as versões
  semanais): `HH:MM COD, COD, ... ` — inclusive os códigos **repetidos** (o
  Mapa real repete `COMER` cinco vezes na primeira linha);
- **Round-trip exato**: salvar sem mexer devolve o arquivo **byte a byte igual**,
  preservando a vírgula/espaço depois do último código, a ordem, os códigos
  desconhecidos e as linhas que não são horário (comentários, `[SEÇÃO]`, linhas
  em branco);
- **Painel de Códigos** no topo: os DBFId da Lista de Códigos viram botões
  **coloridos** (visíveis logo ao abrir a guia), com a mesma cor usada nos chips
  dentro dos horários. Mostra **duas fileiras** por padrão, tem navegação `‹ ›`
  quando há mais códigos do que cabe e a altura é **redimensionável** (1 a 8
  fileiras);
- **Código grudado no mouse**: clicar num código o arma; o clique num horário o
  adiciona e ele continua armado (repetir rápido nos vários horários);
- Botão **`+`** cria um **código de sessão** (só nesta sessão, com borda
  tracejada) e a **lixeira** remove apenas códigos que não vêm do `folders.xml`
  (os de sessão e os adotados do arquivo) — o `folders.xml` é
  **sempre somente leitura**;
- **Códigos que o arquivo usa e o `folders.xml` não lista** entram sozinhos na
  Lista de Códigos: antes de mais nada ganham cor, botão na paleta e entrada no
  combo **Aplicar em** (o chip ficava cinza e não dava para usá-los). Caso real
  da instalação oficial: o `SOR`, usado em `pgm\Grades\GRADE - Copia.txt`. A
  dica do botão diz a procedência, e o `folders.xml` continua intocado;
- **Aplicar** envia um código a todos os horários selecionados. **Não há
  limite de um código por horário**: o mesmo código pode ser repetido no mesmo
  horário, como no Mapa real (`COMER` cinco vezes na primeira linha). Essa
  restrição de "um por vez" continua valendo **apenas para os parâmetros dos
  relógios** (um `FIXO`, um `ID`, um `DUR` por horário);
- **Remover** pergunta se apaga os horários inteiros ou só os códigos (o botão
  direito apaga uma ocorrência por clique);
- **Copiar / Colar** de horários + códigos compartilhado entre os arquivos
  (traz a lista de códigos exatamente como está, com as repetições);
- Alternância **Visual | Texto** sobre o mesmo documento, com rodapé mostrando
  caminho e estado (alterações não salvas, arquivo inexistente).

### Lista de Códigos (`folders.xml`)

- Leitura correta do arquivo real: os elementos são `<Folder0>`..`<FolderN>` (não
  `<Folder>`), o bloco `<Shared>` (pastas compartilhadas) **não** é código e o
  `DBFId` é pareado com o `Title` **do mesmo registro**;
- Somente leitura: o arquivo original nunca é alterado.

## Requisitos

- Windows (x64)
- Para **compilar**: MSVC Build Tools 2022, CMake >= 3.16, Git
  (o JUCE 8.0.15 é baixado automaticamente no primeiro configure)

Para **usar**: basta um único executável (CRT estático), sem redistribuível VC.

## Como compilar

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target ProgMaster ProgMasterCoreTests ProgMasterRelogioTests ProgMasterBlocosTests ProgMasterPlaylistTests
```

Resultado:

```
build\ProgMaster_artefacts\Release\ProgMaster.exe
build\Release\ProgMasterCoreTests.exe
build\Release\ProgMasterRelogioTests.exe
build\Release\ProgMasterBlocosTests.exe
build\Release\ProgMasterPlaylistTests.exe
```

Estado esperado: 0 erros, 0 warnings (a suíte de testes roda 446/446).

## Como executar

```
build\ProgMaster_artefacts\Release\ProgMaster.exe
```

Na primeira execução, informe onde está o `Playlist.exe`. O caminho fica em
`%APPDATA%\ProgMaster\config.txt` (arquivos técnicos: `config.txt` UTF-8 sem
BOM e `log.txt` UTF-8 com BOM).

## Testes

```
build\Release\ProgMasterCoreTests.exe       183/183  regras e playlist.ini
build\Release\ProgMasterRelogioTests.exe   102/102  modelo dos Relógios
build\Release\ProgMasterBlocosTests.exe     132/132  modelo de Mapas/Grades + catálogo
build\Release\ProgMasterPlaylistTests.exe   29/29  leitor do folders.xml
```

Total: 446/446.

Os testes cobrem regras de formato, parse/re-serialização do `.ini`,
localização de arquivos por escopo, validação de afiliadas, o round-trip exato
dos Relógios e dos Blocos e a leitura da Lista de Códigos (inclusive a prova, por
comparação de bytes, de que o `folders.xml` não é alterado). Não há suíte de GUI
automatizada — a validação da interface é manual:

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
   ├─ relogio\             Núcleo Etapa 3: modelo dos Relógios
   │                       (+ tests\RelogioTests.cpp) — sem JUCE
   ├─ blocos\              Núcleo Etapa 4: modelo de Mapas/Grades e catálogo
   │                       de códigos (+ tests\BlockTests.cpp) — sem JUCE
   └─ app\                 Interface JUCE: janela, menus, guias e editores
                           visuais (RelogioEditor, BlockEditor, CodePanel)
```

## Documentação

- `docs\CONTEXTO_PROJETO.txt` — visão geral, o que está implementado, decisões
  técnicas, testes realizados e limitações
- `docs\ARQUITETURA_DO_CODIGO.txt` — arquitetura detalhada
- `docs\ROTEIRO_DE_TESTES.txt` — roteiro de testes manuais do executável
