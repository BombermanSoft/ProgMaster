#pragma once

#include <string>
#include <vector>

// Histórico de versões sucessivas de um texto, usado para desfazer/refazer
// edições (padrão Memento). Não conhece janelas nem controles: apenas guarda
// estados e navega por eles.
//
// Regras (preservadas a partir do comportamento original do editor):
//   - o número de versões retidas é limitado (100 por padrão; as mais antigas
//     são descartadas);
//   - editar a partir de uma versão intermediária descarta as versões "à
//     frente" (comportamento padrão de undo/redo);
//   - reset() substitui todo o histórico por uma única versão de referência;
//   - clear() esvazia o histórico (nenhum undo/redo disponível).
class DocumentHistory {
public:
    explicit DocumentHistory(size_t maxStates = 100);

    // Substitui o histórico pela versão informada (ex.: após carregar o
    // arquivo do disco).
    void reset(const std::wstring& initialState);

    // Esvazia o histórico (ex.: quando não há arquivo em edição).
    void clear();

    bool canUndo() const;
    bool canRedo() const;

    // Desfaz/refaz uma edição. Em caso de sucesso, devolve a versão de
    // destino em outState e retorna true.
    bool undo(std::wstring& outState);
    bool redo(std::wstring& outState);

    // Registra uma nova versão do texto (chamada a cada edição do usuário).
    void pushState(const std::wstring& state);

    size_t maxStates() const;

private:
    std::vector<std::wstring> m_states;
    int m_index = -1;
    size_t m_maxStates = 100;
};