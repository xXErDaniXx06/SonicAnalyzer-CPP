#pragma once

#include <JuceHeader.h>
#include <vector>

class MainComponent : public juce::AudioAppComponent,
    public juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    juce::TextButton openButton;
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;

    // --- NUEVO: Logica de saturacion ---
    std::vector<double> clippingPoints;
    void findClippingPoints(juce::File file);
    // -----------------------------------

    void openButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
