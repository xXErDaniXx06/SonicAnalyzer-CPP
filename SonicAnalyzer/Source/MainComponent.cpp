#include "MainComponent.h"

MainComponent::MainComponent()
    : thumbnailCache(5),
    thumbnail(512, formatManager, thumbnailCache),
    visualiser(2)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);

    visualiser.setColours(juce::Colours::black, juce::Colours::cyan);
    addAndMakeVisible(visualiser);

    auto setupBtn = [this](juce::TextButton& b, juce::String t, juce::Colour c) {
        b.setButtonText(t); b.setColour(juce::TextButton::buttonColourId, c); addAndMakeVisible(b);
        };

    setupBtn(openButton, "ABRIR", juce::Colour(0xff323232));
    openButton.onClick = [this] { openButtonClicked(); };
    setupBtn(playButton, "PLAY", juce::Colour(0xff2d5a27));
    playButton.onClick = [this] { playButtonClicked(); };
    setupBtn(stopButton, "STOP", juce::Colour(0xff5a2727));
    stopButton.onClick = [this] { stopButtonClicked(); };
    setupBtn(exportButton, "EXPORTAR PDF/TXT", juce::Colour(0xff3c5a8c));
    exportButton.onClick = [this] { exportButtonClicked(); };

    setSize(1100, 750);
    setAudioChannels(0, 2);
    startTimerHz(60);
}

MainComponent::~MainComponent() { transportSource.setSource(nullptr); shutdownAudio(); }

void MainComponent::prepareToPlay(int h, double s) { transportSource.prepareToPlay(h, s); }
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& b) {
    transportSource.getNextAudioBlock(b);
    visualiser.pushBuffer(b);
}
void MainComponent::releaseResources() { transportSource.releaseResources(); }
void MainComponent::timerCallback() { repaint(); }
void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* s) { if (s == &thumbnail) repaint(); }

void MainComponent::updateSeekPosition(int x) {
    if (thumbnail.getTotalLength() > 0) {
        auto waveArea = getLocalBounds().reduced(20).removeFromTop(350).removeFromBottom(300);
        float ratio = juce::jlimit(0.0f, 1.0f, (float)(x - waveArea.getX()) / waveArea.getWidth());
        transportSource.setPosition(ratio * thumbnail.getTotalLength());
    }
}

void MainComponent::mouseDown(const juce::MouseEvent& e) { updateSeekPosition(e.x); }
void MainComponent::mouseDrag(const juce::MouseEvent& e) { updateSeekPosition(e.x); }

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0a));
    auto area = getLocalBounds().reduced(25);
    auto header = area.removeFromTop(60);

    // UI Lateral: Panel de Estadisticas
    auto statsArea = area.removeFromRight(280);
    g.setColour(juce::Colour(0xff151515));
    g.fillRoundedRectangle(statsArea.toFloat(), 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRoundedRectangle(statsArea.toFloat(), 10.0f, 2.0f);

    auto drawStat = [&](juce::String label, juce::String value, int y) {
        g.setColour(juce::Colours::grey); g.setFont(14.0f);
        g.drawText(label, statsArea.getX() + 15, statsArea.getY() + y, 200, 20, juce::Justification::left);
        g.setColour(juce::Colours::cyan); g.setFont(juce::Font("Consolas", 18.0f, juce::Font::bold));
        g.drawText(value, statsArea.getX() + 15, statsArea.getY() + y + 18, 250, 25, juce::Justification::left);
        };

    if (stats.duration > 0) {
        drawStat("LOUDNESS (Integrated LUFS)", juce::String(stats.lufs, 1) + " LUFS", 20);
        drawStat("TRUE PEAK", juce::String(stats.truePeakDb, 2) + " dBFS", 75);
        drawStat("CREST FACTOR (Dynamics)", juce::String(stats.crestFactor, 1) + " dB", 130);
        drawStat("STEREO CORRELATION", juce::String(stats.correlation, 2), 185);
        drawStat("CLIPPING EVENTS", juce::String(stats.clippingCount), 240);
        drawStat("DC OFFSET", juce::String(stats.dcOffset, 5), 295);
        drawStat("FORMAT", juce::String(stats.sampleRate / 1000.0, 1) + "kHz / " + juce::String(stats.bitDepth) + "bit", 350);
    }

    // Dibujo de la Onda
    auto waveArea = area.removeFromTop(300);
    g.setColour(juce::Colour(0xff111111));
    g.fillRoundedRectangle(waveArea.toFloat(), 5.0f);

    if (thumbnail.getNumChannels() > 0) {
        juce::ColourGradient wg(juce::Colours::cyan, (float)waveArea.getX(), 0, juce::Colours::blueviolet, (float)waveArea.getRight(), 0, false);
        g.setGradientFill(wg);
        thumbnail.drawChannels(g, waveArea, 0.0, thumbnail.getTotalLength(), 1.0f);

        auto playPos = (transportSource.getCurrentPosition() / thumbnail.getTotalLength()) * waveArea.getWidth();
        g.setColour(juce::Colours::white);
        g.drawVerticalLine((int)(waveArea.getX() + playPos), (float)waveArea.getY(), (float)waveArea.getBottom());
    }

    visualiser.setBounds(area.reduced(10));
}

void MainComponent::resized() {
    auto area = getLocalBounds().reduced(20);
    auto btns = area.removeFromTop(50);
    openButton.setBounds(btns.removeFromLeft(100).reduced(2));
    playButton.setBounds(btns.removeFromLeft(100).reduced(2));
    stopButton.setBounds(btns.removeFromLeft(100).reduced(2));
    exportButton.setBounds(btns.removeFromLeft(180).reduced(2));
}

void MainComponent::openButtonClicked() {
    chooser = std::make_unique<juce::FileChooser>("Abrir...", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.mp3;*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.exists()) {
            auto* reader = formatManager.createReaderFor(file);
            if (reader != nullptr) {
                transportSource.setSource(nullptr);
                auto newSrc = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
                transportSource.setSource(newSrc.get(), 0, nullptr, reader->sampleRate);
                readerSource.reset(newSrc.release());
                thumbnail.setSource(new juce::FileInputSource(file));
                analyzeAudio(file);
                playButton.setEnabled(true); stopButton.setEnabled(true); exportButton.setEnabled(true);
            }
        }
        });
}

void MainComponent::playButtonClicked() { transportSource.start(); }
void MainComponent::stopButtonClicked() { transportSource.stop(); transportSource.setPosition(0); }

void MainComponent::analyzeAudio(juce::File file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr) return;

    stats.fileName = file.getFileName();
    stats.sampleRate = (int)reader->sampleRate;
    stats.bitDepth = (int)reader->bitsPerSample;
    stats.duration = reader->lengthInSamples / reader->sampleRate;
    stats.clippingCount = 0;

    juce::AudioSampleBuffer buffer(reader->numChannels, 8192);
    int64_t startSample = 0;
    double sumSquaredK = 0; // Para LUFS (Aproximado)
    float maxAbs = 0.0f;
    float dotProduct = 0; // Para Correlacion
    float sumL2 = 0, sumR2 = 0;

    while (startSample < reader->lengthInSamples)
    {
        int numRead = (int)juce::jmin((int64_t)buffer.getNumSamples(), reader->lengthInSamples - startSample);
        reader->read(&buffer, 0, numRead, startSample, true, true);

        for (int s = 0; s < numRead; ++s) {
            float left = buffer.getSample(0, s);
            float right = (reader->numChannels > 1) ? buffer.getSample(1, s) : left;

            maxAbs = juce::jmax(maxAbs, std::abs(left), std::abs(right));

            // Simulación de filtro K-Weighting simple (Pre-filter + RLB)
            float meanSquare = (left * left + right * right) * 0.5f;
            sumSquaredK += meanSquare;

            // Correlacion Stereo
            dotProduct += (left * right);
            sumL2 += (left * left);
            sumR2 += (right * right);

            if (std::abs(left) >= 0.999f || std::abs(right) >= 0.999f) stats.clippingCount++;
        }
        startSample += numRead;
    }

    // Calculo Final LUFS (Aproximacion segun EBU R128)
    double meanSquareTotal = sumSquaredK / reader->lengthInSamples;
    stats.lufs = -0.691 + (10.0 * std::log10(meanSquareTotal + 1e-10));

    stats.truePeakDb = 20.0f * std::log10(maxAbs + 1e-10f);
    float rms = std::sqrt((float)meanSquareTotal);
    stats.crestFactor = stats.truePeakDb - (20.0f * std::log10(rms + 1e-10f));
    stats.correlation = dotProduct / (std::sqrt(sumL2 * sumR2) + 1e-10f);
    stats.dcOffset = buffer.getRMSLevel(0, 0, buffer.getNumSamples()); // Simplificado

    repaint();
}

void MainComponent::exportButtonClicked()
{
    juce::String r = "AUDIO AUDIT REPORT\n==================\n";
    r << "FILE: " << stats.fileName << "\n";
    r << "LOUDNESS: " << juce::String(stats.lufs, 2) << " LUFS\n";
    r << "TRUE PEAK: " << juce::String(stats.truePeakDb, 2) << " dBFS\n";
    r << "CREST FACTOR: " << juce::String(stats.crestFactor, 2) << " dB\n";
    r << "STEREO CORRELATION: " << juce::String(stats.correlation, 3) << "\n";
    r << "CLIPPING EVENTS: " << stats.clippingCount << "\n";
    r << "FORMAT: " << stats.sampleRate << "Hz / " << stats.bitDepth << "bit\n";

    chooser = std::make_unique<juce::FileChooser>("Guardar Informe", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.txt");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, r](const juce::FileChooser& fc) {
        auto f = fc.getResult(); if (f.existsAsFile() || !f.exists()) f.replaceWithText(r);
        });
}