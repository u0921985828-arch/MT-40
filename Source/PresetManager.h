#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

/**
    Factory + user preset management for the APVTS state.

    Factory presets are defined in code as parameter overrides; user presets are
    stored as APVTS-state XML files under the user application-data directory.
*/
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& stateToManage)
        : apvts (stateToManage)
    {
        presetDir().createDirectory();
    }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;
        for (const auto& p : factoryPresets())
            names.add (p.name);

        for (const auto& f : presetDir().findChildFiles (juce::File::findFiles, false, "*.xml"))
            names.add (f.getFileNameWithoutExtension());

        return names;
    }

    juce::String getCurrentPreset() const { return currentPreset; }

    void loadPreset (const juce::String& name)
    {
        // Factory preset?
        for (const auto& preset : factoryPresets())
        {
            if (preset.name == name)
            {
                resetToDefaults();
                for (const auto& [id, value] : preset.values)
                    if (auto* param = apvts.getParameter (id))
                        param->setValueNotifyingHost (param->convertTo0to1 (value));

                currentPreset = name;
                return;
            }
        }

        // User preset file.
        const auto file = presetDir().getChildFile (name + ".xml");
        if (file.existsAsFile())
            if (auto xml = juce::XmlDocument::parse (file))
            {
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
                currentPreset = name;
            }
    }

    void savePreset (const juce::String& name)
    {
        if (name.isEmpty())
            return;

        if (auto xml = apvts.copyState().createXml())
            xml->writeTo (presetDir().getChildFile (name + ".xml"));

        currentPreset = name;
    }

private:
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    static const std::vector<Preset>& factoryPresets()
    {
        // Waveform indices: Triangle 0, Tri-Saw 1, Saw 2, Square 3, Wide 4, Narrow 5.
        static const std::vector<Preset> presets
        {
            { "Init", {} },
            { "Fat Bass", {
                { ParamID::osc1Wave, 2 }, { ParamID::osc1Range, 2 },
                { ParamID::mixOsc2On, 1 }, { ParamID::osc2Wave, 2 }, { ParamID::osc2Detune, 0.1f },
                { ParamID::mixOsc1Vol, 0.9f }, { ParamID::mixOsc2Vol, 0.7f },
                { ParamID::filterCutoff, 900.0f }, { ParamID::filterReso, 0.35f },
                { ParamID::filterEnv, 0.7f }, { ParamID::filterDecay, 350.0f },
                { ParamID::filterDrive, 0.35f }, { ParamID::driftAmount, 0.3f },
                { ParamID::ampDecay, 600.0f }, { ParamID::ampSustain, 0.9f } } },
            { "Screaming Lead", {
                { ParamID::osc1Wave, 3 }, { ParamID::osc1Range, 3 },
                { ParamID::mixOsc2On, 1 }, { ParamID::osc2Wave, 2 }, { ParamID::osc2Detune, 0.2f },
                { ParamID::filterCutoff, 2500.0f }, { ParamID::filterReso, 0.55f },
                { ParamID::filterEnv, 0.6f }, { ParamID::glideOn, 1 }, { ParamID::glideTime, 0.08f },
                { ParamID::filterDrive, 0.5f }, { ParamID::ampSustain, 0.85f } } },
            { "Brass", {
                { ParamID::osc1Wave, 2 }, { ParamID::mixOsc2On, 1 }, { ParamID::osc2Wave, 2 },
                { ParamID::osc2Detune, 0.07f }, { ParamID::filterCutoff, 1400.0f },
                { ParamID::filterReso, 0.25f }, { ParamID::filterEnv, 0.75f },
                { ParamID::filterAttack, 60.0f }, { ParamID::filterDecay, 800.0f },
                { ParamID::ampAttack, 40.0f }, { ParamID::ampSustain, 0.8f } } },
            { "Resonant Drone", {
                { ParamID::osc1Wave, 3 }, { ParamID::osc1Range, 1 },
                { ParamID::mixOsc3On, 1 }, { ParamID::osc3KbControl, 0 },
                { ParamID::modMix, 0.0f }, { ParamID::modFilterOn, 1 }, { ParamID::modWheel, 0.4f },
                { ParamID::filterCutoff, 500.0f }, { ParamID::filterReso, 0.8f },
                { ParamID::bassThin, 0.5f }, { ParamID::ampSustain, 1.0f }, { ParamID::ampRelease, 1200.0f } } },
        };
        return presets;
    }

    void resetToDefaults()
    {
        for (auto* param : apvts.processor.getParameters())
            param->setValueNotifyingHost (param->getDefaultValue());
    }

    juce::File presetDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("MoogVASynth")
                   .getChildFile ("Presets");
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentPreset { "Init" };
};
