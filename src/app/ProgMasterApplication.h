#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

class Settings;
class PlaylistLocator;

namespace app {
class MainWindow;
}

// Aplicação JUCE (Etapa 2). A Inter-Face (janela, abas, ComboBox, botões,
// editor de texto) passa a ser implementada com JUCE, conforme item 41 do
// prompt. O NÚCLEO (Settings, PlaylistLocator, documentos e regras de
// formato em readconf) permanece em C++ puro, fora da dependência do JUCE.
class ProgMasterApplication final : public juce::JUCEApplication {
public:
    ProgMasterApplication() = default;
    ~ProgMasterApplication() override = default;

    const juce::String getApplicationName() override { return "ProgMaster"; }
    const juce::String getApplicationVersion() override { return "0.2.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;

private:
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<PlaylistLocator> m_locator;
    std::unique_ptr<app::MainWindow> m_mainWindow;
};