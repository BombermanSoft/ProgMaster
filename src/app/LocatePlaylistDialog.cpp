#include "app/LocatePlaylistDialog.h"

#include "app/JuceHelpers.h"

namespace app {

// Conteúdo do diálogo modal (lógica própria, sem dependência do PlaylistLocator
// para poder ser desenvolvida/ajustada de forma isolada).
class LocateDialogContent final : public juce::Component {
public:
    explicit LocateDialogContent(const std::wstring& initialPath)
    {
        m_pathEditor.setText(jstr(initialPath), juce::dontSendNotification);
        m_pathEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3c3c3c));
        m_pathEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        m_pathEditor.onEscapeKey = [this] { closeDialog(false); };
        m_pathEditor.onReturnKey = [this] { confirm(); };
        addAndMakeVisible(m_pathEditor);

        m_browseButton.setButtonText("Procurar...");
        m_browseButton.onClick = [this] { browse(); };
        addAndMakeVisible(m_browseButton);

        m_confirmButton.setButtonText("Confirmar");
        m_confirmButton.onClick = [this] { confirm(); };
        addAndMakeVisible(m_confirmButton);

        m_cancelButton.setButtonText("Cancelar");
        m_cancelButton.onClick = [this] { closeDialog(false); };
        addAndMakeVisible(m_cancelButton);

        setSize(560, 150);
    }

    void resized() override
    {
        const int margin = 10;
        const int bTop = 30;
        m_pathEditor.setBounds(margin, margin, getWidth() - 2 * margin - 110,
                               30);
        m_browseButton.setBounds(m_pathEditor.getRight() + 8, margin, 102, 30);

        const int bw = 100;
        const int gap = 8;
        m_confirmButton.setBounds(getWidth() - margin - bw, bTop, bw, 30);
        m_cancelButton.setBounds(m_confirmButton.getX() - gap - bw, bTop, bw, 30);
    }

    std::wstring chosenPath() const { return wstr(m_pathEditor.getText()); }

private:
    void browse()
    {
        juce::FileChooser chooser("Localize o Playlist.exe",
                                  juce::File::getSpecialLocation(
                                      juce::File::userHomeDirectory),
                                  "*.exe", true);
        if (chooser.browseForFileToOpen()) {
            m_pathEditor.setText(chooser.getResult().getFullPathName(),
                                 juce::dontSendNotification);
        }
    }

    void confirm() { closeDialog(true); }

    void closeDialog(bool result)
    {
        auto* dialog = findParentComponentOfClass<juce::DialogWindow>();
        if (dialog != nullptr) {
            dialog->exitModalState(result ? 1 : 0);
        }
    }

    juce::TextEditor m_pathEditor;
    juce::TextButton m_browseButton;
    juce::TextButton m_confirmButton;
    juce::TextButton m_cancelButton;
};

void locatePlaylistExe(juce::Component& parent,
                       const std::wstring& initialPath,
                       std::wstring& outPath,
                       bool& confirmed)
{
    confirmed = false;

    LocateDialogContent content(initialPath);

    const int result = juce::DialogWindow::showModalDialog(
        "Localizar o Playlist.exe",
        &content,
        &parent,
        juce::Colour(0xff2b2b2b),
        true,   // escapeKeyTriggersCloseButton
        false,  // shouldBeResizable
        false); // useBottomRightCornerResizer

    if (result == 1) {
        outPath = content.chosenPath();
        confirmed = true;
    }
}

} // namespace app