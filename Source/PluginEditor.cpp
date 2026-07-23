#include "PluginEditor.h"

MoogSynthAudioProcessorEditor::MoogSynthAudioProcessorEditor (MoogSynthAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      controllers (p.getAPVTS()),
      oscillators (p.getAPVTS()),
      mixer       (p.getAPVTS()),
      filter      (p.getAPVTS()),
      output      (p.getAPVTS()),
      keyboard    (p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (controllers);
    addAndMakeVisible (oscillators);
    addAndMakeVisible (mixer);
    addAndMakeVisible (filter);
    addAndMakeVisible (output);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (900, 560, 1600, 1000);
    setSize (1080, 640);
}

MoogSynthAudioProcessorEditor::~MoogSynthAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MoogSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Walnut side frame.
    g.fillAll (LookAndFeelMoog::woodwork);

    // Inner dark-metal chassis with a subtle brushed gradient.
    auto chassisBounds = getLocalBounds().reduced (18, 12).toFloat();
    juce::ColourGradient grad (LookAndFeelMoog::chassis.brighter (0.06f),
                               chassisBounds.getTopLeft(),
                               LookAndFeelMoog::chassis.darker (0.15f),
                               chassisBounds.getBottomRight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (chassisBounds, 8.0f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (chassisBounds, 8.0f, 1.5f);

    // Title / branding.
    g.setColour (LookAndFeelMoog::labelText);
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    g.drawText ("MoogVA  SYNTHESIZER", chassisBounds.removeFromTop (34).toNearestInt(),
                juce::Justification::centred, false);
}

void MoogSynthAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (24, 20);
    area.removeFromTop (30); // title strip

    // On-screen keyboard along the bottom.
    keyboard.setBounds (area.removeFromBottom (80).reduced (0, 4));
    area.removeFromBottom (6);

    // Two rows of control sections.
    auto topRow = area.removeFromTop (area.getHeight() * 55 / 100);

    controllers.setBounds (topRow.removeFromLeft (topRow.getWidth() * 30 / 100).reduced (4));
    oscillators.setBounds (topRow.reduced (4));

    mixer.setBounds  (area.removeFromLeft (area.getWidth() * 28 / 100).reduced (4));
    filter.setBounds (area.removeFromLeft (area.getWidth() * 62 / 100).reduced (4));
    output.setBounds (area.reduced (4));
}
