#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>
#include "Parameters.h"
#include "WebAssets.h"

/**
    Preset management backed by the shared preset bank (webapp/presets.json,
    embedded as binary data). Factory presets are grouped into category folders
    (Bass, Lead, Brass, ...). User presets are stored as APVTS-state XML files
    and surfaced under a "User" folder.

    The bank uses the web-engine camelCase parameter names; they are mapped to
    APVTS parameter IDs here.
*/
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& stateToManage)
        : apvts (stateToManage)
    {
        presetDir().createDirectory();
        loadBank();
    }

    /** Full bank (libraries + a User library) as a JSON string for the UI. */
    juce::String getBankJson() const
    {
        auto* root = new juce::DynamicObject();
        juce::Array<juce::var> libs;

        // Copy the embedded libraries verbatim.
        if (auto* bankObj = bankVar.getDynamicObject())
        {
            const auto libsVar = bankObj->getProperty ("libraries");
            if (libsVar.isArray())
                for (const auto& l : *libsVar.getArray())
                    libs.add (l);
        }

        // A "User" library holding saved presets (single "User" category).
        juce::Array<juce::var> userArr;
        for (const auto& f : presetDir().findChildFiles (juce::File::findFiles, false, "*.xml"))
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", f.getFileNameWithoutExtension());
            userArr.add (juce::var (o));
        }
        if (! userArr.isEmpty())
        {
            auto* userLib = new juce::DynamicObject();
            juce::Array<juce::var> userCats; userCats.add ("User");
            auto* userPresets = new juce::DynamicObject();
            userPresets->setProperty ("User", userArr);
            userLib->setProperty ("name", "User");
            userLib->setProperty ("categories", userCats);
            userLib->setProperty ("presets", juce::var (userPresets));
            libs.add (juce::var (userLib));
        }

        root->setProperty ("libraries", libs);
        return juce::JSON::toString (juce::var (root), true);
    }

    juce::String getCurrentPreset() const { return currentPreset; }

    /** id is "Library|||Category|||Name" (bank or User). */
    void loadPreset (const juce::String& id)
    {
        juce::StringArray parts;
        parts.addTokens (id, "|||", "");
        parts.removeEmptyStrings();
        if (parts.size() < 3)
            return;

        const auto library  = parts[0];
        const auto category = parts[1];
        const auto name     = parts[2];

        if (library == "User")
        {
            const auto file = presetDir().getChildFile (name + ".xml");
            if (file.existsAsFile())
                if (auto xml = juce::XmlDocument::parse (file))
                {
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    currentPreset = id;
                }
            return;
        }

        applyFromBank (library, category, name);
        currentPreset = id;
    }

    void savePreset (const juce::String& name)
    {
        if (name.isEmpty())
            return;
        if (auto xml = apvts.copyState().createXml())
            xml->writeTo (presetDir().getChildFile (name + ".xml"));
        currentPreset = "User|||User|||" + name;
    }

private:
    void loadBank()
    {
        for (int i = 0; i < WebAssets::namedResourceListSize; ++i)
        {
            if (juce::String (WebAssets::originalFilenames[i]) == "presets.json")
            {
                int size = 0;
                const char* data = WebAssets::getNamedResource (WebAssets::namedResourceList[i], size);
                bankVar = juce::JSON::parse (juce::String::fromUTF8 (data, size));
                return;
            }
        }
    }

    void applyFromBank (const juce::String& library, const juce::String& category, const juce::String& name)
    {
        auto* bankObj = bankVar.getDynamicObject();
        if (bankObj == nullptr) return;
        const auto libsVar = bankObj->getProperty ("libraries");
        if (! libsVar.isArray()) return;

        for (const auto& libVar : *libsVar.getArray())
        {
            auto* lib = libVar.getDynamicObject();
            if (lib == nullptr || lib->getProperty ("name").toString() != library) continue;

            auto* presets = lib->getProperty ("presets").getDynamicObject();
            if (presets == nullptr) return;
            const auto arr = presets->getProperty (category);
            if (! arr.isArray()) return;

            for (const auto& item : *arr.getArray())
            {
                if (auto* obj = item.getDynamicObject())
                {
                    if (obj->getProperty ("name").toString() == name)
                    {
                        resetToDefaults();
                        if (auto* params = obj->getProperty ("params").getDynamicObject())
                            for (const auto& kv : params->getProperties())
                                applyParam (kv.name.toString(), (float) (double) kv.value);
                        return;
                    }
                }
            }
            return;
        }
    }

    void applyParam (const juce::String& camelKey, float value)
    {
        const auto& map = camelToId();
        const auto it = map.find (camelKey.toStdString());
        if (it == map.end()) return;
        if (auto* p = apvts.getParameter (it->second))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void resetToDefaults()
    {
        // Global performance controls are player state, not part of a sound preset.
        static const juce::StringArray perform {
            ParamID::polyOn, ParamID::chordType, ParamID::arpOn, ParamID::arpRate,
            ParamID::arpMode, ParamID::arpOct, ParamID::arpGate, ParamID::arpBpm,
            ParamID::midiOutOn
        };
        for (auto* param : apvts.processor.getParameters())
        {
            if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
                if (perform.contains (withID->paramID))
                    continue;
            param->setValueNotifyingHost (param->getDefaultValue());
        }
    }

    static const std::unordered_map<std::string, juce::String>& camelToId()
    {
        static const std::unordered_map<std::string, juce::String> m = {
            {"masterVolume",ParamID::masterVolume},{"masterTune",ParamID::masterTune},
            {"glideOn",ParamID::glideOn},{"glideTime",ParamID::glideTime},
            {"modWheel",ParamID::modWheel},{"modMix",ParamID::modMix},
            {"modOscOn",ParamID::modOscOn},{"modFilterOn",ParamID::modFilterOn},
            {"osc1Wave",ParamID::osc1Wave},{"osc1Range",ParamID::osc1Range},
            {"osc2Wave",ParamID::osc2Wave},{"osc2Range",ParamID::osc2Range},{"osc2Detune",ParamID::osc2Detune},
            {"osc3Wave",ParamID::osc3Wave},{"osc3Range",ParamID::osc3Range},{"osc3Detune",ParamID::osc3Detune},
            {"osc3Kb",ParamID::osc3KbControl},
            {"mixOsc1Vol",ParamID::mixOsc1Vol},{"mixOsc2Vol",ParamID::mixOsc2Vol},{"mixOsc3Vol",ParamID::mixOsc3Vol},
            {"mixNoiseVol",ParamID::mixNoiseVol},{"mixExtVol",ParamID::mixExtVol},
            {"mixOsc1On",ParamID::mixOsc1On},{"mixOsc2On",ParamID::mixOsc2On},{"mixOsc3On",ParamID::mixOsc3On},
            {"mixNoiseOn",ParamID::mixNoiseOn},{"mixExtOn",ParamID::mixExtOn},{"noiseType",ParamID::noiseType},
            {"filterCutoff",ParamID::filterCutoff},{"filterReso",ParamID::filterReso},{"filterEnv",ParamID::filterEnv},
            {"filterKeyTrack",ParamID::filterKeyTrack},
            {"filterAttack",ParamID::filterAttack},{"filterDecay",ParamID::filterDecay},
            {"filterSustain",ParamID::filterSustain},{"filterRelease",ParamID::filterRelease},
            {"ampAttack",ParamID::ampAttack},{"ampDecay",ParamID::ampDecay},
            {"ampSustain",ParamID::ampSustain},{"ampRelease",ParamID::ampRelease},
            {"drift",ParamID::driftAmount},{"filterDrive",ParamID::filterDrive},{"bassThin",ParamID::bassThin},
            {"sampleOn",ParamID::sampleOn},{"sampleSel",ParamID::sampleSel},{"sampleVol",ParamID::sampleVol},
            {"fxDriveOn",ParamID::fxDriveOn},{"fxDrive",ParamID::fxDrive},
            {"fxChorusOn",ParamID::fxChorusOn},{"fxChorus",ParamID::fxChorus},
            {"fxPhaserOn",ParamID::fxPhaserOn},{"fxPhaser",ParamID::fxPhaser},
            {"fxCrushOn",ParamID::fxCrushOn},{"fxCrush",ParamID::fxCrush},
            {"fxToneOn",ParamID::fxToneOn},{"fxTone",ParamID::fxTone},
            {"fxDelayOn",ParamID::fxDelayOn},{"fxDelayMix",ParamID::fxDelayMix},{"fxDelayTime",ParamID::fxDelayTime},
            {"fxReverbOn",ParamID::fxReverbOn},{"fxReverbMix",ParamID::fxReverbMix},
        };
        return m;
    }

    juce::File presetDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("MoogVASynth").getChildFile ("Presets");
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::var bankVar;
    juce::String currentPreset { "Init" };
};
