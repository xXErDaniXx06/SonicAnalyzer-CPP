#include "MainComponent.h"

MainComponent::MainComponent()
    : thumbnailCache(5),
    thumbnail(512, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);

    openButton.setButtonText("Cargar archivo de audio...");
    openButton.onClick = [this] { openButtonClicked(); };
    addAndMakeVisible(openButton);

    setSize(800, 600);
    setAudioChannels(0, 2);
}

MainComponent::~MainComponent() { shutdownAudio(); }

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
    auto headerArea = area.removeFromTop(60);
    openButton.setBounds(headerArea.removeFromLeft(180).reduced(5));

    // Dibujar Info de Analisis
    if (estimatedBPM > 0)
    {
        g.setColour(juce::Colours::white);
        g.setFont(18.0f);
        juce::String infoText = "BPM estimado: " + juce::String(estimatedBPM, 1) +
            "  |  Nivel RMS: " + juce::String(20.0 * std::log10(averageRMS), 1) + " dB";
        g.drawText(infoText, headerArea, juce::Justification::centredRight);
    }

    // Dibujar Onda
    if (thumbnail.getNumChannels() > 0)
    {
        g.setColour(juce::Colours::lightblue);
        thumbnail.drawChannels(g, area, 0.0, thumbnail.getTotalLength(), 1.0f);

        // Dibujar Saturacion (Rojo)
        g.setColour(juce::Colours::red.withAlpha(0.7f));
        for (auto time : clippingPoints)
        {
            auto xPos = area.getX() + (time / thumbnail.getTotalLength()) * area.getWidth();
            g.drawVerticalLine((int)xPos, (float)area.getY(), (float)area.getBottom());
        }

        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawRect(area);
    }
    else
    {
        g.setColour(juce::Colours::grey);
        g.drawText("Cargue un MP3 para analizar ritmo y volumen", area, juce::Justification::centred);
    }
}

void MainComponent::resized() {}

void MainComponent::openButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser>("Abrir audio...",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.mp3;*.wav");

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
                    analyzeAudio(file);
                }
            }
        });
}

void MainComponent::analyzeAudio(juce::File file)
{
    clippingPoints.clear();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader != nullptr)
    {
        juce::AudioSampleBuffer buffer(reader->numChannels, 4096);
        int64_t startSample = 0;
        float totalSumSquared = 0;
        int64_t totalSamplesProcessed = 0;

        std::vector<double> beatTimes; // Para estimar BPM

        while (startSample < reader->lengthInSamples)
        {
            int numSamplesToRead = (int)juce::jmin((int64_t)buffer.getNumSamples(), reader->lengthInSamples - startSample);
            reader->read(&buffer, 0, numSamplesToRead, startSample, true, true);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* data = buffer.getReadPointer(ch);
                for (int s = 0; s < numSamplesToRead; ++s)
                {
                    float sample = std::abs(data[s]);
                    totalSumSquared += sample * sample;

                    // Deteccion de clipping
                    if (sample >= 0.999f)
                    {
                        double t = (startSample + s) / reader->sampleRate;
                        if (clippingPoints.empty() || t > clippingPoints.back() + 0.05)
                            clippingPoints.push_back(t);
                    }

                    // Deteccion simple de transitorios (golpes) para BPM
                    if (sample > 0.8f) // Umbral de "golpe"
                    {
                        double t = (startSample + s) / reader->sampleRate;
                        if (beatTimes.empty() || t > beatTimes.back() + 0.25) // Maximo 240 BPM
                            beatTimes.push_back(t);
                    }
                }
            }
            startSample += numSamplesToRead;
            totalSamplesProcessed += numSamplesToRead * reader->numChannels;
        }

        // Calcular RMS (Volumen promedio)
        averageRMS = std::sqrt(totalSumSquared / totalSamplesProcessed);

        // Estimar BPM simple basado en intervalos de golpes
        if (beatTimes.size() > 5)
        {
            double totalInterval = 0;
            for (size_t i = 1; i < beatTimes.size(); ++i)
                totalInterval += (beatTimes[i] - beatTimes[i - 1]);

            double avgInterval = totalInterval / (beatTimes.size() - 1);
            estimatedBPM = 60.0 / avgInterval;

            // Ajustar a rangos normales (60-180) si sale algo loco
            while (estimatedBPM > 185) estimatedBPM /= 2.0;
            while (estimatedBPM < 65)  estimatedBPM *= 2.0;
        }
    }
    repaint();
}
