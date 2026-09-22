cmake_minimum_required(VERSION 3.16)

project(ProgMaster
    VERSION 0.1.0
    DESCRIPTION "ProgMaster — complemento do sistema Playlist (Etapa 1)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# O código-fonte é mantido em UTF-8. É essencial forçar o MSVC a interpretar
# a origem como UTF-8, caso contrário literais com acentos ("Códigos",
# "Não foi possível...") seriam decodificados incorretamente.
if(MSVC)
    add_compile_options(/W4 /utf-8)

    # Vincula a CRT estaticamente (/MT e /MTd), de modo que o executável não
    # dependa do redistribuível VC (vcruntime140.dll) — meta de distribuição
    # em um único arquivo.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-Wall -Wextra)
endif()

# WIN32 -> aplicação de interface gráfica (subsistema Windows), sem janela de console.
add_executable(ProgMaster WIN32
    src/main.cpp

    src/core/Application.cpp
    src/core/Settings.cpp
    src/core/Log.cpp
    src/core/TextFileIO.cpp
    src/core/DocumentHistory.cpp
    src/core/FileSystem.cpp

    src/documents/EditorDocumentController.cpp

    src/playlist/PlaylistLocator.cpp
    src/playlist/PlaylistInstallation.cpp
    src/playlist/PlaylistIni.cpp
    src/playlist/FoldersXml.cpp

    src/models/FolderEntry.cpp

    src/ui/MainWindow.cpp
    src/ui/IniEditorView.cpp
    src/ui/CodesPageView.cpp
    src/ui/LocatePlaylistDialog.cpp

    # Recurso do ícone da aplicação (bomba), embutido no executável.
    resources/ProgMaster.rc
)

target_compile_definitions(ProgMaster PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Permite os includes relativos à pasta src (ex.: "playlist/FoldersXml.h").
target_include_directories(ProgMaster PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

# Dependências de sistema (todas presentes em qualquer Windows moderno,
# o que mantém a meta de distribuição em um único executável).
target_link_libraries(ProgMaster PRIVATE
    comctl32    # Tab control e ListView
    comdlg32    # GetOpenFileNameW (seleção do Playlist.exe)
    shell32     # SHGetFolderPathW (pasta AppData)
)

# No MinGW (GCC) é necessário informar o ponto de entrada wWinMain.
if(MINGW)
    target_link_options(ProgMaster PRIVATE -municode)
endif()