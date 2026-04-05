#include "MainComponent.h"

// --- ESTE ES EL CONSTRUCTOR (Solo debe haber uno) ---
MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();

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

// --- MÉTODOS DE AUDIO VACÍOS POR AHORA ---
void MainComponent::prepareToPlay(int, double) {}
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo&) {}
void MainComponent::releaseResources() {}

// --- DIBUJO DE LA INTERFAZ ---
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    openButton.setBounds(20, 20, 150, 40);
}

// --- LÓGICA DE APERTURA DE ARCHIVO ---
void MainComponent::openButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser>("Selecciona un MP3...",
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
                    juce::Logger::outputDebugString("¡Todo listo: " + file.getFileName() + "!");
                }
            }
        });
}
