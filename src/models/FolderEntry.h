#pragma once

#include <string>

// Representa um registro <Folder> encontrado no folders.xml.
//
// O campo dbfId é o IDENTIFICADOR PRINCIPAL usado na programação do Playlist
// (a guia "Códigos" da interface exibe esse valor em destaque).
// Os demais campos são mantidos para uso em etapas futuras e para permitir
// identificar visualmente o registro (ex.: Title).
struct FolderEntry {
    std::wstring dbfId;   // tag <DBFId>
    std::wstring title;   // tag <Title>
    std::wstring id;      // tag <ID>
    std::wstring type;    // tag <Type>
    std::wstring target;  // tag <Target>

    bool hasDbfId() const
    {
        return !dbfId.empty();
    }
};