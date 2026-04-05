#include "MainComponent.h"

MainComponent::MainComponent()
    : thumbnailCache(5),
    thumbnail(512, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);

    // --- ESTILO DE BOTONES ---
    openButton.setButtonText("ABRIR ARCHIVO");
    openButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff323232));
    openButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    openButton.onClick = [this] { openButtonClicked(); };
    addAndMakeVisible(openButton);

    playButton.setButtonText("PLAY");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d5a27));
    playButton.setEnabled(false);
    playButton.onClick = [this] { playButtonClicked(); };
    addAndMakeVisible(playButton);

    stopButton.setButtonText("STOP");
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5a2727));
    stopButton.setEnabled(false);
    stopButton.onClick = [this] { stopButtonClicked(); };
    addAndMakeVisible(stopButton);

    setSize(900, 600);
    setAudioChannels(0, 2);
    startTimerHz(60); // Mas FPS para una linea de reproduccion ultra-suave
}

MainComponent::~MainComponent()
{
    transportSource.setSource(nullptr);
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

void MainComponent::releaseResources() { transportSource.releaseResources(); }

void MainComponent::timerCallback() { repaint(); }

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail) repaint();
}

// --- LOGICA DE CLICK Y BUSQUEDA (SEEKING) ---
void MainComponent::updateSeekPosition(int x)
{
    if (thumbnail.getTotalLength() > 0)
    {
        auto area = getLocalBounds().reduced(20);
        area.removeFromTop(80);

        auto relativeX = juce::jlimit(0, area.getWidth(), x - area.getX());
        auto ratio = (double)relativeX / (double)area.getWidth();
        auto seekTime = ratio * thumbnail.getTotalLength();

        transportSource.setPosition(seekTime);
    }
}

void MainComponent::mouseDown(const juce::MouseEvent& event) { updateSeekPosition(event.x); }
void MainComponent::mouseDrag(const juce::MouseEvent& event) { updateSeekPosition(event.x); }

void MainComponent::paint(juce::Graphics& g)
{
    // Fondo con degradado elegante
    juce::ColourGradient backgroundGradient(juce::Colour(0xff1a1a1a), 0, 0,
        juce::Colour(0xff0a0a0a), 0, (float)getHeight(), false);
    g.setGradientFill(backgroundGradient);
    g.fillAll();

    auto area = getLocalBounds().reduced(20);
    auto headerArea = area.removeFromTop(80);

    // Panel de Info Estilizado
    if (estimatedBPM > 0)
    {
        g.setColour(juce::Colours::cyan.withAlpha(0.7f));
        g.setFont(juce::Font("Consolas", 18.0f, juce::Font::bold));

        float dbValue = (averageRMS > 0) ? 20.0f * std::log10(averageRMS) : -100.0f;
        juce::String info = "TEMPO: " + juce::String(estimatedBPM, 0) + " BPM  |  LEVEL: " + juce::String(dbValue, 1) + " dB";
        g.drawText(info, headerArea, juce::Justification::centredRight);
    }

    // Dibujar Area de Onda
    if (thumbnail.getNumChannels() > 0)
    {
        // Sombra suave bajo la onda
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(area.toFloat(), 6.0f);

        // Dibujar Onda con Degradado
        juce::ColourGradient waveGradient(juce::Colours::cyan, (float)area.getX(), (float)area.getY(),
            juce::Colours::blueviolet, (float)area.getRight(), (float)area.getY(), false);
        g.setGradientFill(waveGradient);
        thumbnail.drawChannels(g, area, 0.0, thumbnail.getTotalLength(), 1.0f);

        // Dibujar Puntos de Saturacion (Mas finos y elegantes)
        g.setColour(juce::Colours::red.withAlpha(0.5f));
        for (auto time : clippingPoints)
        {
            auto xPos = area.getX() + (time / thumbnail.getTotalLength()) * area.getWidth();
            g.drawVerticalLine((int)xPos, (float)area.getY(), (float)area.getBottom());
        }

        // Linea de Reproduccion (Neon White)
        auto audioLength = thumbnail.getTotalLength();
        if (audioLength > 0.0)
        {
            auto drawPosition = (transportSource.getCurrentPosition() / audioLength) * area.getWidth();
            g.setColour(juce::Colours::white);
            g.drawLine((float)(area.getX() + drawPosition), (float)area.getY(),
                (float)(area.getX() + drawPosition), (float)area.getBottom(), 2.5f);

            // Brillo en el cabezal
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawEllipse((float)(area.getX() + drawPosition - 5), (float)area.getY(), 10, 10, 2.0f);
        }

        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawRoundedRectangle(area.toFloat(), 6.0f, 2.0f);
    }
    else
    {
        g.setColour(juce::Colours::grey.withAlpha(0.4f));
        g.drawRoundedRectangle(area.toFloat(), 6.0f, 2.0f);
        g.drawText("ARRASTRE O CARGUE UN ARCHIVO PARA ANALIZAR", area, juce::Justification::centred);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(30);
    auto buttonsArea = area.removeFromTop(50);

    int buttonWidth = 140;
    openButton.setBounds(buttonsArea.removeFromLeft(buttonWidth).reduced(4));
    playButton.setBounds(buttonsArea.removeFromLeft(buttonWidth).reduced(4));
    stopButton.setBounds(buttonsArea.removeFromLeft(buttonWidth).reduced(4));
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
                if (mag > 0.7f) { // Umbral de beat ligeramente mas sensible
                    double t = (startSample + s) / reader->sampleRate;
                    if (beatTimes.empty() || t > beatTimes.back() + 0.3) beatTimes.push_back(t);
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
        while (estimatedBPM > 190) estimatedBPM /= 2.0;
        while (estimatedBPM < 60)  estimatedBPM *= 2.0;
    }
    repaint();
}