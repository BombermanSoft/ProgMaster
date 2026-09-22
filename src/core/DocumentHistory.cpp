#include "core/DocumentHistory.h"

#include <utility>

DocumentHistory::DocumentHistory(size_t maxStates)
    : m_maxStates(maxStates)
{
}

void DocumentHistory::reset(const std::wstring& initialState)
{
    m_states.clear();
    m_states.push_back(initialState);
    m_index = 0;
}

void DocumentHistory::clear()
{
    m_states.clear();
    m_index = -1;
}

bool DocumentHistory::canUndo() const
{
    return m_index > 0;
}

bool DocumentHistory::canRedo() const
{
    return m_index >= 0 &&
           m_index + 1 < static_cast<int>(m_states.size());
}

bool DocumentHistory::undo(std::wstring& outState)
{
    if (!canUndo()) {
        return false;
    }
    --m_index;
    outState = m_states[m_index];
    return true;
}

bool DocumentHistory::redo(std::wstring& outState)
{
    if (!canRedo()) {
        return false;
    }
    ++m_index;
    outState = m_states[m_index];
    return true;
}

void DocumentHistory::pushState(const std::wstring& state)
{
    // Editar a partir de uma versão intermediária descarta as versões à
    // frente (comportamento padrão de undo/redo).
    std::vector<std::wstring>::iterator eraseFrom = m_states.begin();
    if (m_index >= 0) {
        eraseFrom = m_states.begin() + m_index + 1;
    }
    m_states.erase(eraseFrom, m_states.end());

    m_states.push_back(state);
    while (m_states.size() > m_maxStates) {
        m_states.erase(m_states.begin());
    }
    m_index = static_cast<int>(m_states.size()) - 1;
}

size_t DocumentHistory::maxStates() const
{
    return m_maxStates;
}