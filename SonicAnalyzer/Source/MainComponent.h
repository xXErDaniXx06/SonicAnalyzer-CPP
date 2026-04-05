#pragma once

#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent,
    public juce::ChangeListener // Heredamos para saber cuando la onda esta lista
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Esta funcion se activa cuando el dibujo de la onda termina de procesarse
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    juce::TextButton openButton;
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::FileChooser> chooser;

    // Herramientas para dibujar la onda de audio
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;

    void openButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
