#include "MainComponent.h"

MainComponent::MainComponent()
    : thumbnailCache(5),
    thumbnail(512, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);

    openButton.setButtonText("Abrir Archivo...");
    openButton.onClick = [this] { openButtonClicked(); };
    addAndMakeVisible(openButton);

    playButton.setButtonText("Play");
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green.withAlpha(0.5f));
    playButton.setEnabled(false);
    addAndMakeVisible(playButton);

    stopButton.setButtonText("Stop");
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.5f));
    stopButton.setEnabled(false);
    addAndMakeVisible(stopButton);

    setSize(800, 600);
    setAudioChannels(0, 2);

    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    transportSource.setSource(nullptr); // Importante: Limpiar antes de cerrar
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    transportSource.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    transportSource.releaseResources();
}

void MainComponent::timerCallback()
{
    repaint();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail) repaint();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto area = getLocalBounds().reduced(20);
    auto headerArea = area.removeFromTop(60);

    if (estimatedBPM > 0)
    {
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);
        float dbValue = (averageRMS > 0) ? 20.0f * std::log10(averageRMS) : -100.0f;
        juce::String info = "BPM: " + juce::String(estimatedBPM, 1) + " | RMS: " + juce::String(dbValue, 1) + " dB";
        g.drawText(info, headerArea, juce::Justification::centredRight);
    }

    if (thumbnail.getNumChannels() > 0)
    {
        g.setColour(juce::Colours::lightblue);
        thumbnail.drawChannels(g, area, 0.0, thumbnail.getTotalLength(), 1.0f);

        g.setColour(juce::Colours::red.withAlpha(0.6f));
        for (auto time : clippingPoints)
        {
            auto xPos = area.getX() + (time / thumbnail.getTotalLength()) * area.getWidth();
            g.drawVerticalLine((int)xPos, (float)area.getY(), (float)area.getBottom());
        }

        auto audioLength = thumbnail.getTotalLength();
        if (audioLength > 0.0)
        {
            auto drawPosition = (transportSource.getCurrentPosition() / audioLength) * area.getWidth();
            g.setColour(juce::Colours::white);
            g.drawLine((float)(area.getX() + drawPosition), (float)area.getY(), (float)(area.getX() + drawPosition), (float)area.getBottom(), 2.0f);
        }

        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawRect(area);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto buttonsArea = area.removeFromTop(40);
    openButton.setBounds(buttonsArea.removeFromLeft(120).reduced(2));
    playButton.setBounds(buttonsArea.removeFromLeft(100).reduced(2));
    stopButton.setBounds(buttonsArea.removeFromLeft(100).reduced(2));
}

void MainComponent::openButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser>("Abrir...", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.mp3;*.wav");

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                auto* reader = formatManager.createReaderFor(file);
                if (reader != nullptr)
                {
                    // PASO CRITICO: Paramos el audio antes de cambiar el archivo
                    transportSource.stop();
                    transportSource.setSource(nullptr);

                    auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
                    transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);

                    playButton.setEnabled(true);
                    stopButton.setEnabled(true);

                    readerSource.reset(newSource.release());
                    thumbnail.setSource(new juce::FileInputSource(file));
                    analyzeAudio(file);
                }
            }
        });
}

void MainComponent::playButtonClicked() { transportSource.start(); }
void MainComponent::stopButtonClicked() { transportSource.stop(); transportSource.setPosition(0); }

void MainComponent::analyzeAudio(juce::File file)
{
    clippingPoints.clear();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr) return;

    juce::AudioSampleBuffer buffer(reader->numChannels, 4096);
    int64_t startSample = 0;
    float totalSumSq = 0;
    std::vector<double> beatTimes;

    while (startSample < reader->lengthInSamples)
    {
        int numRead = (int)juce::jmin((int64_t)buffer.getNumSamples(), reader->lengthInSamples - startSample);
        reader->read(&buffer, 0, numRead, startSample, true, true);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getReadPointer(ch);
            for (int s = 0; s < numRead; ++s)
            {
                float mag = std::abs(data[s]);
                totalSumSq += mag * mag;
                if (mag >= 0.999f) {
                    double t = (startSample + s) / reader->sampleRate;
                    if (clippingPoints.empty() || t > clippingPoints.back() + 0.05) clippingPoints.push_back(t);
                }
                if (mag > 0.8f) {
                    double t = (startSample + s) / reader->sampleRate;
                    if (beatTimes.empty() || t > beatTimes.back() + 0.25) beatTimes.push_back(t);
                }
            }
        }
        startSample += numRead;
    }
    if (reader->lengthInSamples > 0)
        averageRMS = std::sqrt(totalSumSq / (float)(reader->lengthInSamples * reader->numChannels));

    if (beatTimes.size() > 5) {
        double avgInt = (beatTimes.back() - beatTimes.front()) / (beatTimes.size() - 1);
        estimatedBPM = 60.0 / avgInt;
        while (estimatedBPM > 185) estimatedBPM /= 2.0;
        while (estimatedBPM < 65)  estimatedBPM *= 2.0;
    }
    repaint();
}