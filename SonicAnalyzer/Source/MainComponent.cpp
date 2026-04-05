#include "MainComponent.h"

MainComponent::MainComponent()
    : thumbnailCache(5),
    thumbnail(512, formatManager, thumbnailCache)
{
    // Registramos formatos como MP3 y WAV
    formatManager.registerBasicFormats();

    // Configuramos el dibujante de ondas
    thumbnail.addChangeListener(this);

    // Configuramos el boton
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
        repaint(); // Forzamos el redibujado de la pantalla
}

void MainComponent::paint(juce::Graphics& g)
{
    // Fondo negro
    g.fillAll(juce::Colours::black);

    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(60); // Dejamos sitio para el boton

    // Si hay un archivo cargado, dibujamos la onda
    if (thumbnail.getNumChannels() > 0)
    {
        g.setColour(juce::Colours::lightblue);
        thumbnail.drawChannels(g, area, 0.0, thumbnail.getTotalLength(), 1.0f);

        // Marco de la zona de visualizacion
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawRect(area);
    }
    else
    {
        g.setColour(juce::Colours::white);
        g.drawText("Cargue un archivo para ver la onda de audio", area, juce::Justification::centred);
    }
}

void MainComponent::resized()
{
    openButton.setBounds(20, 20, 150, 40);
}

void MainComponent::openButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser>("Seleccione un archivo de audio...",
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

                    // Pasamos el archivo al dibujante de ondas
                    thumbnail.setSource(new juce::FileInputSource(file));

                    juce::Logger::outputDebugString("Archivo cargado: " + file.getFileName());
                }
            }
        });
}
