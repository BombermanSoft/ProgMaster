Pasta de recursos visuais do projeto (ícones, imagens, textos auxiliares).

Conteúdo atual:
  - ProgMaster.ico  Ícone do aplicativo (bomba, gerado por script). Embutido
                     no executável via recursos RC.
  - resource.h      Identificadores de recursos (IDI_APP = 100).
  - ProgMaster.rc   Script de recursos (agraga o ícone ao executável).

Nota: o script de recursos (.rc) é incluído no CMakeLists.txt dentro de
add_executable; a pasta resources/ está no include path do alvo.