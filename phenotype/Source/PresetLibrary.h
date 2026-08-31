#pragma once

//============================================================================
//  PresetLibrary.h
//
//  Runtime preset registry. Seeds from the compiled factory (Presets.h) so the
//  plugin always has presets with zero files present, then scans a user
//  directory for `.phbank` bank files (DLC / imported libraries) and appends
//  their presets — scaling to tens of thousands without bloating the binary.
//
//  A bank is JSON:
//    {
//      "bank": "MY LIBRARY",
//      "format": 1,
//      "presets": [
//        { "name": "Emerald Cathedral",
//          "params": { "grainDensity": 0.68, "filterCutoff": 0.65, ... },
//          "sample": "samples/og_kush.wav" }        // optional, relative to bank
//      ]
//    }
//
//  `sample` (optional) points at a mono/stereo audio file loaded as that
//  preset's genome — the source of genuinely different, HQ timbres. Omit it and
//  the preset plays the built-in wavetable genome.
//============================================================================

#include <juce_core/juce_core.h>
#include <vector>
#include <utility>
#include "Presets.h"

namespace phenotype
{
    struct PresetEntry
    {
        juce::String name;                                     // "LIBRARY > Name"
        std::vector<std::pair<juce::String, float>> params;    // id -> normalised
        juce::File   sample;                                   // external DLC sample
        juce::String embeddedGenome;                           // built-in HQ palette id
        // Genome priority: sample (DLC) > embeddedGenome (factory) > built-in.
    };

    class PresetLibrary
    {
    public:
        //  Built-in factory presets (always available).
        void seedFactory()
        {
            for (const auto& p : presets::kFactory)
            {
                PresetEntry e;
                e.name = p.name;
                for (const auto& kv : p.overrides)
                    if (kv.id != nullptr)
                        e.params.emplace_back (juce::String (kv.id), kv.value);
                if (p.genome != nullptr)
                    e.embeddedGenome = juce::String (p.genome);
                entries.push_back (std::move (e));
            }
        }

        //  Recursively scan a directory for *.phbank files; returns count added.
        int scan (const juce::File& dir)
        {
            if (! dir.isDirectory())
                return 0;

            juce::Array<juce::File> banks;
            dir.findChildFiles (banks, juce::File::findFiles, true, "*.phbank");
            banks.sort();                              // deterministic order
            int added = 0;
            for (const auto& bankFile : banks)
                added += loadBank (bankFile);
            return added;
        }

        //  Parse one bank file; returns count of presets appended.
        int loadBank (const juce::File& bankFile)
        {
            const auto json = juce::JSON::parse (bankFile.loadFileAsString());
            if (! json.isObject())
                return 0;

            const juce::String bankName =
                json.getProperty ("bank", bankFile.getFileNameWithoutExtension()).toString();

            const auto presetsVar = json.getProperty ("presets", juce::var());
            if (! presetsVar.isArray())
                return 0;

            int added = 0;
            for (const auto& pv : *presetsVar.getArray())
            {
                if (! pv.isObject())
                    continue;

                const juce::String nm = pv.getProperty ("name", "").toString();
                if (nm.isEmpty())
                    continue;

                PresetEntry e;
                e.name = nm.contains (" > ") ? nm : (bankName + " > " + nm);

                if (auto* obj = pv.getProperty ("params", juce::var()).getDynamicObject())
                    for (const auto& prop : obj->getProperties())
                        e.params.emplace_back (prop.name.toString(),
                                               (float) (double) prop.value);

                const juce::String samplePath = pv.getProperty ("sample", "").toString();
                if (samplePath.isNotEmpty())
                {
                    const juce::File s = bankFile.getSiblingFile (samplePath);
                    if (s.existsAsFile())
                        e.sample = s;
                }

                entries.push_back (std::move (e));
                ++added;
            }
            return added;
        }

        void clear()                       { entries.clear(); }
        int  size()             const      { return (int) entries.size(); }
        juce::String nameAt (int i) const  { return isValid (i) ? entries[(size_t) i].name : juce::String(); }
        const PresetEntry* at (int i) const{ return isValid (i) ? &entries[(size_t) i] : nullptr; }

        //  ~/Documents/Phenotype/Presets — created if missing. Drop DLC banks
        //  (a folder with a .phbank plus its samples) here.
        static juce::File userPresetDir()
        {
            auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                         .getChildFile ("Phenotype").getChildFile ("Presets");
            if (! d.exists())
                d.createDirectory();
            return d;
        }

    private:
        [[nodiscard]] bool isValid (int i) const noexcept { return i >= 0 && i < size(); }
        std::vector<PresetEntry> entries;
    };
}
