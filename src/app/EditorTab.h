#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <memory>

#include "playlist/PlaylistIni.h"

namespace app {
class PlaylistConfigController;
} // namespace app

class PlaylistInstallation;

// Tab "Editor": bloco de notas textual dos arquivos de programação.
//
// Mantém o comportamento da Etapa 1 (Salvar / Desfazer / Refazer, arquivo
// atual exibido) e estende o menu Editar com Relógio Comercial/Relógio
// Musical (Etapa 2). O playlist.ini, quando aberto aqui, usa o MESMO
// documento em memória do controlador de configuração (sincronização).
class EditorTab final : public juce::Component {
public:
    // Arquivo textual em edição nesta aba.
    enum class FileKind {
        PlaylistIni,
        MapasComercial,
        GradesMusicais,
        RelogioComercial,
        RelogioMusical,
    };

    EditorTab(app::PlaylistConfigController& controller,
              PlaylistInstallation& installation);
    ~EditorTab() override = default;

    // Abre um arquivo na aba (preenche o editor com o conteúdo atual).
    void openFile(FileKind file);
    // Recarrega o arquivo atual do disco (Descartar alterações).
    void reopenFromDisk();
    // Salva o arquivo atual (menu Arquivo/Salvar também).
    bool saveCurrentFile();

    bool hasUnsavedChanges() const;
    std::wstring currentFileName() const;
    FileKind currentKind() const { return m_current; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Chamado pelo JUCE ao exibir/ocultar a aba: mantém o playlist.ini em
    // sincronia com a interface (Configuração).
    void visibilityChanged() override;

private:
    void loadIntoEditor();
    void setDirty(bool dirty);
    void updateButtons();
    void updateStatus();

    // Serviços de arquivo (o playlist.ini vive no controlador, os demais aqui).
    app::PlaylistConfigController& m_controller;
    PlaylistInstallation& m_installation;
    PlaylistIni m_mapas;
    PlaylistIni m_grades;
    PlaylistIni m_relogioComercial;
    PlaylistIni m_relogioMusical;

    FileKind m_current = FileKind::PlaylistIni;
    bool m_hasFile = false;
    bool m_dirty = false;
    bool m_updatingUi = false; // evita marcar "sujo" ao trocar texto pelo programa

    juce::Label m_fileLabel;
    juce::TextButton m_saveButton{ "Salvar" };
    juce::TextButton m_undoButton{ "Desfazer" };
    juce::TextButton m_redoButton{ "Refazer" };
    juce::TextEditor m_editor;
    juce::Label m_statusLabel;
};