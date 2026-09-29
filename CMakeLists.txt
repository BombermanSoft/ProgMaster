cmake_minimum_required(VERSION 3.16)

project(ProgMaster
    VERSION 0.3.0
    DESCRIPTION "ProgMaster — complemento do sistema Playlist (Etapa 3)"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# O código-fonte é mantido em UTF-8. É essencial forçar o MSVC a interpretar a
# origem como UTF-8, caso contrário literais com acentos ("Códigos",
# "Não foi possível...") seriam decodificados incorretamente.
if(MSVC)
    add_compile_options(/utf-8)

    # Vinca a CRT estaticamente (/MT e /MTd), de modo que o executável não
    # dependa do redistribuível VC — meta de distribuição em um único arquivo.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-Wall -Wextra)
endif()

# ----------------------------------------------------------------------------
# JUCE (obtido via FetchContent, GIT_TAG 8.0.15 — ver prompt da Etapa 2, §41)
# ----------------------------------------------------------------------------
include(FetchContent)

# Não queremos exemplos/extras do JUCE — apenas os módulos que usaremos.
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_SHOWCASE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------
# Núcleo (independe de JUCE) + camada Etapa 2 (leitura dos formatos)
# ----------------------------------------------------------------------------
set(PROGMASTER_CORE
    src/core/Log.cpp
    src/core/Settings.cpp
    src/core/TextFileIO.cpp

    src/playlist/PlaylistLocator.cpp
    src/playlist/PlaylistInstallation.cpp
    src/playlist/PlaylistIni.cpp
    src/playlist/FoldersXml.cpp

    src/models/FolderEntry.cpp

    src/readconf/FormatRules.cpp
    src/readconf/PlaylistIniDocument.cpp
    src/readconf/PlaylistFileLocator.cpp
    src/readconf/ReadingConfiguration.cpp

    src/relogio/RelogioDocument.cpp

    src/blocos/BlockDocument.cpp
    src/blocos/CodeCatalogue.cpp
)

# ----------------------------------------------------------------------------
# Aplicação (interface JUCE da Etapa 2)
# ----------------------------------------------------------------------------
juce_add_gui_app(ProgMaster
    COMPANY_NAME "BombermanSoft"
    PRODUCT_NAME "ProgMaster"
    VERSION 0.3.0
    ICON_BIG   resources/ProgMasterIcon.png
    ICON_SMALL resources/ProgMasterIcon.png
    NEEDS_WEB_BROWSER  OFF
    NEEDS_MIDI_INPUT   OFF
    NEEDS_MIDI_OUTPUT  OFF
)

target_sources(ProgMaster PRIVATE
    ${PROGMASTER_CORE}
    src/app/ProgMasterApplication.cpp
    src/app/MainWindow.cpp
    src/app/ConfiguratorTab.cpp
    src/app/EditorTab.cpp
    src/app/RelogioEditorTab.cpp
    src/app/RelogioEditor.cpp
    src/app/BlockEditorTab.cpp
    src/app/BlockEditor.cpp
    src/app/CodePanel.cpp
    src/app/CodesTab.cpp
    src/app/LocatePlaylistDialog.cpp
    src/app/PlaylistConfigController.cpp
)

# Recurso embutido: ícone da janela (PNG) e rótulo da aplicação.
juce_add_binary_data(ProgMasterResources
    SOURCES resources/ProgMasterIcon.png
)
target_link_libraries(ProgMaster PRIVATE ProgMasterResources)

target_include_directories(ProgMaster PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Módulo de interface do JUCE (depende de core/events/graphics).
target_link_libraries(ProgMaster PRIVATE juce_gui_basics)

# O JUCE 8.0.15 desativa janelas modais por padrão (JUCE_MODAL_LOOPS_PERMITTED
# = 0). Na área de trabalho queremos o comportamento clássico (dialogos e
# seletores de arquivo modais), então reativamos a flag.
target_compile_definitions(ProgMaster PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

if(MSVC)
    # /W4 apenas no nosso código: os módulos do JUCE são compilados com a
    # configuração própria deles e gerariam ruído desnecessário aqui.
    target_compile_options(ProgMaster PRIVATE /W4)
endif()

# ----------------------------------------------------------------------------
# Testes do núcleo (sem JUCE — são regras puras)
# ----------------------------------------------------------------------------

add_executable(ProgMasterCoreTests
    ${PROGMASTER_CORE}
    src/readconf/tests/RulesTests.cpp
)
target_include_directories(ProgMasterCoreTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(MSVC)
    target_compile_options(ProgMasterCoreTests PRIVATE /W4)
endif()

# ----------------------------------------------------------------------------
# Testes do modelo do Relógio (também sem JUCE). Separados do ProgMasterCoreTests
# porque cada arquivo de teste possui seu próprio main().
# ----------------------------------------------------------------------------

add_executable(ProgMasterRelogioTests
    ${PROGMASTER_CORE}
    src/relogio/tests/RelogioTests.cpp
)
target_include_directories(ProgMasterRelogioTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(MSVC)
    target_compile_options(ProgMasterRelogioTests PRIVATE /W4)
endif()

# ----------------------------------------------------------------------------
# Testes do modelo dos Blocos (Mapas Comerciais e Grades Musicais) — sem JUCE.
# ----------------------------------------------------------------------------

add_executable(ProgMasterBlocosTests
    ${PROGMASTER_CORE}
    src/blocos/tests/BlockTests.cpp
)
target_include_directories(ProgMasterBlocosTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(MSVC)
    target_compile_options(ProgMasterBlocosTests PRIVATE /W4)
endif()

# ----------------------------------------------------------------------------
# Testes do leitor da Lista de Códigos (folders.xml — somente leitura).
# ----------------------------------------------------------------------------

add_executable(ProgMasterPlaylistTests
    ${PROGMASTER_CORE}
    src/playlist/tests/FoldersXmlTests.cpp
)
target_include_directories(ProgMasterPlaylistTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(MSVC)
    target_compile_options(ProgMasterPlaylistTests PRIVATE /W4)
endif()

