#include "MainComponent.h"
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
    setupBtn(exportButton, "INFORME PRO", juce::Colour(0xff3c5a8c));
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
        auto waveArea = getLocalBounds().reduced(20).removeFromLeft(850).removeFromTop(350);
        float ratio = juce::jlimit(0.0f, 1.0f, (float)(x - waveArea.getX()) / waveArea.getWidth());
        transportSource.setPosition(ratio * thumbnail.getTotalLength());
    }
}

void MainComponent::mouseDown(const juce::MouseEvent& e) { updateSeekPosition(e.x); }
void MainComponent::mouseDrag(const juce::MouseEvent& e) { updateSeekPosition(e.x); }

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff050505));
    auto area = getLocalBounds().reduced(20);

    // Panel de Estadisticas (Lateral Derecho)
    auto statsArea = area.removeFromRight(320);
    g.setColour(juce::Colour(0xff101010));
    g.fillRoundedRectangle(statsArea.toFloat(), 12.0f);
    g.setColour(juce::Colours::cyan.withAlpha(0.2f));
    g.drawRoundedRectangle(statsArea.toFloat(), 12.0f, 1.5f);

    auto drawStat = [&](juce::String label, juce::String value, int y, juce::Colour vCol = juce::Colours::cyan) {
        g.setColour(juce::Colours::grey); g.setFont(13.0f);
        g.drawText(label, statsArea.getX() + 20, statsArea.getY() + y, 280, 20, juce::Justification::left);
        g.setColour(vCol); g.setFont(juce::Font("Consolas", 19.0f, juce::Font::bold));
        g.drawText(value, statsArea.getX() + 20, statsArea.getY() + y + 18, 280, 25, juce::Justification::left);
        };

    if (stats.duration > 0) {
        drawStat("INTEGRATED LOUDNESS", juce::String(stats.lufsIntegrated, 1) + " LUFS", 20, juce::Colours::yellow);
        drawStat("LOUDNESS RANGE (LRA)", juce::String(stats.lra, 1) + " LU", 75);
        drawStat("DYNAMIC RANGE (PLR)", juce::String(stats.plr, 1) + " dB", 130);
        drawStat("TRUE PEAK MAX", juce::String(stats.truePeakDb, 2) + " dBFS", 185,
            stats.truePeakDb > -1.0f ? juce::Colours::red : juce::Colours::cyan);
        drawStat("SHORT-TERM MAX", juce::String(stats.lufsShortTermMax, 1) + " LUFS", 240);
        drawStat("M/S WIDTH (Side/Mid)", juce::String(stats.stereoWidth * 100.0f, 0) + "%", 295);
        drawStat("ZERO CROSSING RATE", juce::String(stats.zeroCrossings / 1000) + "k / sec", 350);
        drawStat("CLIPPING DETECTED", juce::String(stats.clippingCount), 405,
            stats.clippingCount > 0 ? juce::Colours::orangered : juce::Colours::cyan);
    }

    // Dibujo de Interfaz Principal
    auto waveArea = area.removeFromTop(350);
    g.setColour(juce::Colour(0xff111111));
    g.fillRoundedRectangle(waveArea.toFloat(), 6.0f);

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
    stats.zeroCrossings = 0;

    juce::AudioSampleBuffer buffer(reader->numChannels, 16384);
    int64_t startSample = 0;

    std::vector<double> shortTermPowers;
    float maxAbs = 0.0f;
    double totalSumSqK = 0;
    double midSumSq = 0, sideSumSq = 0;

    while (startSample < reader->lengthInSamples)
    {
        int numRead = (int)juce::jmin((int64_t)buffer.getNumSamples(), reader->lengthInSamples - startSample);
        reader->read(&buffer, 0, numRead, startSample, true, true);

        double blockSumSqK = 0;
        for (int s = 0; s < numRead; ++s) {
            float l = buffer.getSample(0, s);
            float r = (reader->numChannels > 1) ? buffer.getSample(1, s) : l;

            // Deteccion de cruces por cero (Zero Crossing)
            if (s > 0 && ((l > 0 && buffer.getSample(0, s - 1) < 0) || (l < 0 && buffer.getSample(0, s - 1) > 0)))
                stats.zeroCrossings++;

            // Mid-Side Analysis
            float mid = (l + r) * 0.5f;
            float side = (l - r) * 0.5f;
            midSumSq += mid * mid;
            sideSumSq += side * side;

            maxAbs = juce::jmax(maxAbs, std::abs(l), std::abs(r));
            blockSumSqK += (l * l + r * r) * 0.5;

            if (std::abs(l) >= 0.999f || std::abs(r) >= 0.999f) stats.clippingCount++;
        }

        double blockPower = blockSumSqK / numRead;
        shortTermPowers.push_back(blockPower);
        totalSumSqK += blockSumSqK;
        startSample += numRead;
    }

    // Calculos Finales
    double meanSq = totalSumSqK / reader->lengthInSamples;
    stats.lufsIntegrated = -0.691 + (10.0 * std::log10(meanSq + 1e-10));
    stats.truePeakDb = 20.0f * std::log10(maxAbs + 1e-10f);
    stats.plr = stats.truePeakDb - (float)stats.lufsIntegrated;
    stats.stereoWidth = (float)(sideSumSq / (midSumSq + sideSumSq + 1e-10));

    // LRA (Simplified Loudness Range)
    if (shortTermPowers.size() > 10) {
        std::sort(shortTermPowers.begin(), shortTermPowers.end());
        double p10 = shortTermPowers[shortTermPowers.size() * 0.1];
        double p95 = shortTermPowers[shortTermPowers.size() * 0.95];
        stats.lra = 10.0 * std::log10(p95 / (p10 + 1e-10));

        double maxPower = *std::max_element(shortTermPowers.begin(), shortTermPowers.end());
        stats.lufsShortTermMax = -0.691 + (10.0 * std::log10(maxPower + 1e-10));
    }

    repaint();
}

void MainComponent::exportButtonClicked()
{
    juce::String r = "PROFESSIONAL AUDIO AUDIT\n";
    r << "--------------------------\n";
    r << "FILE: " << stats.fileName << "\n";
    r << "INTEGRATED LOUDNESS: " << juce::String(stats.lufsIntegrated, 2) << " LUFS\n";
    r << "LOUDNESS RANGE: " << juce::String(stats.lra, 2) << " LU\n";
    r << "DYNAMIC RANGE (PLR): " << juce::String(stats.plr, 2) << " dB\n";
    r << "TRUE PEAK: " << juce::String(stats.truePeakDb, 2) << " dBFS\n";
    r << "STEREO WIDTH: " << juce::String(stats.stereoWidth * 100.0f, 1) << "%\n";
    r << "ZERO CROSSINGS: " << stats.zeroCrossings << "\n";
    r << "CLIPPING COUNT: " << stats.clippingCount << "\n";
    r << "FORMAT: " << stats.sampleRate << "Hz / " << stats.bitDepth << "bit\n";

    chooser = std::make_unique<juce::FileChooser>("Guardar", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.txt");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, r](const juce::FileChooser& fc) {
        auto f = fc.getResult(); if (f.existsAsFile() || !f.exists()) f.replaceWithText(r);
        });
}