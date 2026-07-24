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

    /** Full menu (bank categories + a User folder) as a JSON string for the UI. */
    juce::String getBankJson() const
    {
        auto* root = new juce::DynamicObject();
        juce::Array<juce::var> cats;

        auto* presetsObj = new juce::DynamicObject();
        if (auto* bankObj = bankVar.getDynamicObject())
        {
            if (auto* srcPresets = bankObj->getProperty ("presets").getDynamicObject())
            {
                const auto catsVar = bankObj->getProperty ("categories");
                if (catsVar.isArray())
                    for (const auto& c : *catsVar.getArray())
                    {
                        cats.add (c);
                        presetsObj->setProperty (c.toString(), srcPresets->getProperty (c.toString()));
                    }
            }
        }

        // User folder.
        juce::Array<juce::var> userArr;
        for (const auto& f : presetDir().findChildFiles (juce::File::findFiles, false, "*.xml"))
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", f.getFileNameWithoutExtension());
            userArr.add (juce::var (o));
        }
        if (! userArr.isEmpty())
        {
            cats.add ("User");
            presetsObj->setProperty ("User", userArr);
        }

        root->setProperty ("categories", cats);
        root->setProperty ("presets", juce::var (presetsObj));
        return juce::JSON::toString (juce::var (root), true);
    }

    juce::String getCurrentPreset() const { return currentPreset; }

    /** id is "Category|||Name" (bank or User). */
    void loadPreset (const juce::String& id)
    {
        const auto sep = id.indexOf ("|||");
        if (sep < 0)
            return;

        const auto category = id.substring (0, sep);
        const auto name     = id.substring (sep + 3);

        if (category == "User")
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

        applyFromBank (category, name);
        currentPreset = id;
    }

    void savePreset (const juce::String& name)
    {
        if (name.isEmpty())
            return;
        if (auto xml = apvts.copyState().createXml())
            xml->writeTo (presetDir().getChildFile (name + ".xml"));
        currentPreset = "User|||" + name;
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

    void applyFromBank (const juce::String& category, const juce::String& name)
    {
        auto* bankObj = bankVar.getDynamicObject();
        if (bankObj == nullptr) return;
        auto* presets = bankObj->getProperty ("presets").getDynamicObject();
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
        for (auto* param : apvts.processor.getParameters())
            param->setValueNotifyingHost (param->getDefaultValue());
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
