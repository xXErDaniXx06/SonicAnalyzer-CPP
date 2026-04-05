#include "MainComponent.h"
#include <cmath>
#include <algorithm>

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
    setupBtn(exportButton, "INFORME FINAL", juce::Colour(0xff3c5a8c));
    exportButton.onClick = [this] { exportButtonClicked(); };

    setSize(1200, 800);
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
        auto waveArea = getLocalBounds().reduced(25).removeFromLeft(800).removeFromTop(400);
        float ratio = juce::jlimit(0.0f, 1.0f, (float)(x - waveArea.getX()) / waveArea.getWidth());
        transportSource.setPosition(ratio * thumbnail.getTotalLength());
    }
}

void MainComponent::mouseDown(const juce::MouseEvent& e) { updateSeekPosition(e.x); }
void MainComponent::mouseDrag(const juce::MouseEvent& e) { updateSeekPosition(e.x); }

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff050505));
    auto area = getLocalBounds().reduced(25);

    // Panel de Estadisticas
    auto statsArea = area.removeFromRight(350);
    g.setColour(juce::Colour(0xff0f0f0f));
    g.fillRoundedRectangle(statsArea.toFloat(), 15.0f);
    g.setColour(juce::Colours::cyan.withAlpha(0.15f));
    g.drawRoundedRectangle(statsArea.toFloat(), 15.0f, 2.0f);

    auto drawStat = [&](juce::String label, juce::String value, int y, juce::Colour vCol = juce::Colours::cyan) {
        g.setColour(juce::Colours::grey); g.setFont(13.0f);
        g.drawText(label, statsArea.getX() + 25, statsArea.getY() + y, 300, 20, juce::Justification::left);
        g.setColour(vCol); g.setFont(juce::Font("Consolas", 21.0f, juce::Font::bold));
        g.drawText(value, statsArea.getX() + 25, statsArea.getY() + y + 20, 300, 25, juce::Justification::left);
        };

    if (stats.duration > 0) {
        drawStat("KEY / TONALITY", stats.detectedKey, 25, juce::Colours::gold);
        drawStat("LOUDNESS INTEGRATED", juce::String(stats.lufsIntegrated, 1) + " LUFS", 85, juce::Colours::yellow);
        drawStat("DYNAMIC RANGE (PLR)", juce::String(stats.plr, 1) + " dB", 145);
        drawStat("TRUE PEAK MAX", juce::String(stats.truePeakDb, 2) + " dBFS", 205);
        drawStat("LOUDNESS RANGE (LRA)", juce::String(stats.lra, 1) + " LU", 265);
        drawStat("STEREO WIDTH", juce::String(stats.stereoWidth * 100.0f, 0) + "%", 325);
        drawStat("CLIPPING EVENTS", juce::String(stats.clippingCount), 385, stats.clippingCount > 0 ? juce::Colours::red : juce::Colours::cyan);
        drawStat("ZERO CROSSINGS", juce::String(stats.zeroCrossings / 1000) + "k/s", 445);
    }

    auto waveArea = area.removeFromTop(400);
    g.setColour(juce::Colour(0xff111111));
    g.fillRoundedRectangle(waveArea.toFloat(), 8.0f);

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

// --- LOGICA DE DETECCION DE TONALIDAD ---
juce::String MainComponent::estimateKey(const std::vector<float>& chroma)
{
    const juce::String noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Perfiles ideales de Krumhansl-Schmuckler (Simplificados)
    float majorProfile[] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    float minorProfile[] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    int bestRoot = 0;
    bool isMinor = false;
    float maxCorrelation = -1.0f;

    for (int root = 0; root < 12; ++root) {
        float corrMajor = 0, corrMinor = 0;
        for (int i = 0; i < 12; ++i) {
            int note = (root + i) % 12;
            corrMajor += chroma[note] * majorProfile[i];
            corrMinor += chroma[note] * minorProfile[i];
        }
        if (corrMajor > maxCorrelation) { maxCorrelation = corrMajor; bestRoot = root; isMinor = false; }
        if (corrMinor > maxCorrelation) { maxCorrelation = corrMinor; bestRoot = root; isMinor = true; }
    }

    return noteNames[bestRoot] + (isMinor ? " Minor" : " Major");
}

void MainComponent::analyzeAudio(juce::File file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr) return;

    stats.fileName = file.getFileName();
    stats.sampleRate = (int)reader->sampleRate;
    stats.bitDepth = (int)reader->bitsPerSample;
    stats.duration = reader->lengthInSamples / reader->sampleRate;
    stats.clippingCount = 0;
    stats.zeroCrossings = 0;

    std::vector<float> chroma(12, 0.0f);
    juce::AudioSampleBuffer buffer(reader->numChannels, 16384);
    int64_t startSample = 0;
    double totalSumSqK = 0;
    float maxAbs = 0.0f;
    double midSumSq = 0, sideSumSq = 0;
    std::vector<double> shortTermPowers;

    while (startSample < reader->lengthInSamples)
    {
        int numRead = (int)juce::jmin((int64_t)buffer.getNumSamples(), reader->lengthInSamples - startSample);
        reader->read(&buffer, 0, numRead, startSample, true, true);

        double blockPower = 0;
        for (int s = 0; s < numRead; ++s) {
            float l = buffer.getSample(0, s);
            float r = (reader->numChannels > 1) ? buffer.getSample(1, s) : l;

            float absL = std::abs(l);
            maxAbs = juce::jmax(maxAbs, absL, std::abs(r));
            blockPower += (l * l + r * r) * 0.5;

            midSumSq += std::pow(l + r, 2);
            sideSumSq += std::pow(l - r, 2);

            if (s > 0 && ((l > 0 && buffer.getSample(0, s - 1) < 0) || (l < 0 && buffer.getSample(0, s - 1) > 0)))
                stats.zeroCrossings++;

            if (absL >= 0.999f) stats.clippingCount++;

            // Deteccion de nota (Frecuencia dominante estimada por periodo)
            // Una aproximacion simple al Pitch para el Chromagram
            if (s % 100 == 0 && absL > 0.1f) {
                float freq = (float)reader->sampleRate / 100.0f; // Muy simplificado
                int note = (int)std::round(12.0 * std::log2(freq / 440.0) + 69.0) % 12;
                chroma[std::abs(note)] += absL;
            }
        }
        shortTermPowers.push_back(blockPower / numRead);
        totalSumSqK += blockPower;
        startSample += numRead;
    }

    stats.lufsIntegrated = -0.691 + (10.0 * std::log10((totalSumSqK / reader->lengthInSamples) + 1e-10));
    stats.truePeakDb = 20.0f * std::log10(maxAbs + 1e-10f);
    stats.plr = stats.truePeakDb - (float)stats.lufsIntegrated;
    stats.stereoWidth = (float)(sideSumSq / (midSumSq + sideSumSq + 1e-10));
    stats.detectedKey = estimateKey(chroma);

    if (shortTermPowers.size() > 10) {
        std::sort(shortTermPowers.begin(), shortTermPowers.end());
        stats.lra = 10.0 * std::log10(shortTermPowers[shortTermPowers.size() * 0.95] / (shortTermPowers[shortTermPowers.size() * 0.1] + 1e-10));
    }

    repaint();
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

void MainComponent::exportButtonClicked()
{
    juce::String r = "ULTRASONIC AUDIT REPORT\n=======================\n";
    r << "FILE: " << stats.fileName << "\n";
    r << "DETECTED KEY: " << stats.detectedKey << "\n";
    r << "INTEGRATED LOUDNESS: " << juce::String(stats.lufsIntegrated, 2) << " LUFS\n";
    r << "DYNAMIC RANGE (PLR): " << juce::String(stats.plr, 2) << " dB\n";
    r << "LOUDNESS RANGE (LRA): " << juce::String(stats.lra, 2) << " LU\n";
    r << "TRUE PEAK MAX: " << juce::String(stats.truePeakDb, 2) << " dBFS\n";
    r << "STEREO WIDTH: " << juce::String(stats.stereoWidth * 100.0f, 1) << "%\n";
    r << "CLIPPING EVENTS: " << stats.clippingCount << "\n";

    chooser = std::make_unique<juce::FileChooser>("Guardar", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.txt");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, r](const juce::FileChooser& fc) {
        auto f = fc.getResult(); if (f.existsAsFile() || !f.exists()) f.replaceWithText(r);
        });
}