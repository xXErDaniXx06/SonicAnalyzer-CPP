#pragma once

#include <JuceHeader.h>
#include <vector>

class MainComponent : public juce::AudioAppComponent,
    public juce::ChangeListener,
    public juce::Timer
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
    void timerCallback() override;

    // --- NUEVO: Interaccion con el raton ---
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    juce::TextButton openButton;
    juce::TextButton playButton;
    juce::TextButton stopButton;

    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;

    std::vector<double> clippingPoints;
    double estimatedBPM = 0.0;
    float averageRMS = 0.0f;

    void analyzeAudio(juce::File file);
    void openButtonClicked();
    void playButtonClicked();
    void stopButtonClicked();

    // Funcion auxiliar para mover el cabezal
    void updateSeekPosition(int x);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};