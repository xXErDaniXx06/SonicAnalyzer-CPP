#include "MainComponent.h"

MainComponent::MainComponent()
    : thumbnailCache(5),
    thumbnail(512, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);

    openButton.setButtonText("Cargar MP3...");
    openButton.onClick = [this] { openButtonClicked(); };
    addAndMakeVisible(openButton);

    setSize(800, 600);
    setAudioChannels(0, 2);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay(int, double) {}
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo&) {}
void MainComponent::releaseResources() {}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail)
        repaint();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(60);

    if (thumbnail.getNumChannels() > 0)
    {
        // Dibujamos la onda en azul claro
        g.setColour(juce::Colours::lightblue);
        thumbnail.drawChannels(g, area, 0.0, thumbnail.getTotalLength(), 1.0f);

        // --- DIBUJAR SATURACION ---
        g.setColour(juce::Colours::red.withAlpha(0.8f));
        for (auto time : clippingPoints)
        {
            auto xPos = area.getX() + (time / thumbnail.getTotalLength()) * area.getWidth();
            g.drawVerticalLine((int)xPos, (float)area.getY(), (float)area.getBottom());
        }

        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawRect(area);
    }
    else
    {
        g.setColour(juce::Colours::white);
        g.drawText("Cargue un archivo para analizar picos de audio", area, juce::Justification::centred);
    }
}

void MainComponent::resized()
{
    openButton.setBounds(20, 20, 150, 40);
}

void MainComponent::openButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser>("Seleccione archivo...",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.mp3;*.wav");

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();

            if (file != juce::File{})
            {
                auto* reader = formatManager.createReaderFor(file);

                if (reader != nullptr)
                {
                    readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
                    thumbnail.setSource(new juce::FileInputSource(file));

                    // Escaneamos picos de saturacion
                    findClippingPoints(file);

                    juce::Logger::outputDebugString("Analisis completado: " + file.getFileName());
                }
            }
        });
}

void MainComponent::findClippingPoints(juce::File file)
{
    clippingPoints.clear();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader != nullptr)
    {
        juce::AudioSampleBuffer buffer(reader->numChannels, 1024);
        int64_t startSample = 0;

        while (startSample < reader->lengthInSamples)
        {
            int numSamplesToRead = (int)juce::jmin((int64_t)buffer.getNumSamples(), reader->lengthInSamples - startSample);
            reader->read(&buffer, 0, numSamplesToRead, startSample, true, true);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* channelData = buffer.getReadPointer(ch);
                for (int s = 0; s < numSamplesToRead; ++s)
                {
                    if (std::abs(channelData[s]) >= 0.999f)
                    {
                        double timeInSeconds = (startSample + s) / reader->sampleRate;

                        if (clippingPoints.empty() || timeInSeconds > clippingPoints.back() + 0.1)
                            clippingPoints.push_back(timeInSeconds);

                        break;
                    }
                }
            }
            startSample += numSamplesToRead;
        }
    }
    repaint();
}
