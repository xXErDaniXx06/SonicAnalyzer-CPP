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

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    juce::TextButton openButton, playButton, stopButton, exportButton;
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioVisualiserComponent visualiser;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;

    struct AudioStats {
        double lufsIntegrated = 0.0;
        double lufsShortTermMax = -100.0;
        double lufsMomentaryMax = -100.0;
        double lra = 0.0; // Loudness Range
        float truePeakDb = -100.0f;
        float plr = 0.0f; // Peak to Loudness Ratio (Dynamics)
        float stereoWidth = 0.0f; // M/S Ratio
        int zeroCrossings = 0;
        int clippingCount = 0;
        double duration = 0.0;
        int sampleRate = 0;
        int bitDepth = 0;
        juce::String fileName;
    } stats;

    void analyzeAudio(juce::File file);
    void openButtonClicked();
    void playButtonClicked();
    void stopButtonClicked();
    void exportButtonClicked();
    void updateSeekPosition(int x);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};