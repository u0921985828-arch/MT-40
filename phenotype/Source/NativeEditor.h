//==============================================================================
//  NativeEditor.h — full native editor for legacy / no-WebView hosts.
//
//  The WebGL WebView UI can't render on old hosts (e.g. FL Studio 11, or any
//  32-bit host without WebView2), so those builds (PHENOTYPE_NATIVE_EDITOR)
//  need a first-class native editor — not a bare slider list. This provides:
//
//    * a preset browser: a library-grouped dropdown of every program, prev/next
//      steppers, and the current "LIBRARY > name" readout;
//    * an Import Library button (the same .phbank / DLC folder import as the
//      WebView UI) plus Rescan, so users can grow the library on any host;
//    * the real synthesis controls as labelled neon rotaries, grouped into the
//      same banks as the WebView rack, each bound to the APVTS.
//
//  Header-only; included by PluginProcessor.cpp's createEditor().
//==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include <vector>
#include <memory>
#include <cstring>

namespace phenotype
{
    // --- palette (mirrors the WebView grow-lab theme) ------------------------
    namespace ne
    {
        const juce::Colour bg       { 0xff06090a };
        const juce::Colour panel    { 0xff0d1512 };
        const juce::Colour panel2   { 0xff11201b };
        const juce::Colour line      { 0xff1b2a24 };
        const juce::Colour ink      { 0xffe9f6ef };
        const juce::Colour dim      { 0xff7f9d8c };
        const juce::Colour faint    { 0xff56705f };
        const juce::Colour chloro   { 0xff00ff6a };
        const juce::Colour magenta  { 0xffff2bd6 };
    }

    //==========================================================================
    //  Neon look & feel: dark rotaries with a glowing chlorophyll value arc.
    //==========================================================================
    class NeonLNF : public juce::LookAndFeel_V4
    {
    public:
        NeonLNF()
        {
            setColour (juce::Slider::textBoxTextColourId, ne::ink);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::ComboBox::backgroundColourId, ne::panel2);
            setColour (juce::ComboBox::textColourId, ne::ink);
            setColour (juce::ComboBox::outlineColourId, ne::line);
            setColour (juce::ComboBox::arrowColourId, ne::chloro);
            setColour (juce::PopupMenu::backgroundColourId, ne::panel);
            setColour (juce::PopupMenu::textColourId, ne::ink);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, ne::chloro.withAlpha (0.22f));
            setColour (juce::PopupMenu::headerTextColourId, ne::chloro);
            setColour (juce::TextButton::buttonColourId, ne::panel2);
            setColour (juce::TextButton::textColourOnId, ne::bg);
            setColour (juce::TextButton::textColourOffId, ne::ink);
            setColour (juce::Label::textColourId, ne::ink);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                               float pos, float startAngle, float endAngle,
                               juce::Slider& s) override
        {
            const auto full = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
            const float d = juce::jmin (full.getWidth(), full.getHeight());
            const auto sq = juce::Rectangle<float> (d, d).withCentre (full.getCentre());
            const auto cx = sq.getCentreX(), cy = sq.getCentreY();
            const auto r  = d * 0.5f;
            const auto ang = startAngle + pos * (endAngle - startAngle);
            const auto accent = s.getProperties()["accent"].toString();
            const juce::Colour a = accent == "b" ? ne::magenta
                                 : accent == "n" ? ne::ink : ne::chloro;

            // well (circular)
            const auto well = sq.reduced (r * 0.16f);
            g.setColour (juce::Colour (0xff070b09));
            g.fillEllipse (well);
            g.setColour (ne::line);
            g.drawEllipse (well, 1.0f);

            // track
            juce::Path track;
            track.addCentredArc (cx, cy, r * 0.82f, r * 0.82f, 0.0f, startAngle, endAngle, true);
            g.setColour (juce::Colour (0xff14211b));
            g.strokePath (track, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // value arc
            juce::Path val;
            val.addCentredArc (cx, cy, r * 0.82f, r * 0.82f, 0.0f, startAngle, ang, true);
            g.setColour (a);
            g.strokePath (val, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // pointer
            juce::Point<float> tip (cx + std::cos (ang - juce::MathConstants<float>::halfPi) * r * 0.72f,
                                    cy + std::sin (ang - juce::MathConstants<float>::halfPi) * r * 0.72f);
            g.setColour (a);
            g.drawLine (cx, cy, tip.x, tip.y, 2.2f);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (tip));
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (13.0f); }
        juce::Font getLabelFont (juce::Label& l) override     { return l.getFont(); }
    };

    //==========================================================================
    //  The editor.
    //==========================================================================
    class NativeEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
    {
    public:
        explicit NativeEditor (PhenotypeAudioProcessor& p)
            : juce::AudioProcessorEditor (p), proc (p)
        {
            setLookAndFeel (&lnf);

            // ---- preset bar ----
            title.setText ("PHENOTYPE", juce::dontSendNotification);
            title.setFont (juce::Font (18.0f, juce::Font::bold).withExtraKerningFactor (0.18f));
            title.setColour (juce::Label::textColourId, ne::ink);
            addAndMakeVisible (title);

            addAndMakeVisible (presetName);
            presetName.setJustificationType (juce::Justification::centred);
            presetName.setFont (juce::Font (14.0f, juce::Font::bold));
            presetName.setColour (juce::Label::textColourId, ne::chloro);

            auto initStep = [this] (juce::TextButton& btn, int dir)
            {
                btn.setColour (juce::TextButton::buttonColourId, ne::panel2);
                addAndMakeVisible (btn);
                btn.onClick = [this, dir] { step (dir); };
            };
            initStep (prev, -1);
            initStep (next, +1);

            addAndMakeVisible (presetCombo);
            presetCombo.setTextWhenNothingSelected ("Preset");
            presetCombo.onChange = [this]
            {
                const int id = presetCombo.getSelectedId();
                if (id > 0) { proc.setCurrentProgram (id - 1); refreshName(); }
            };

            // search filter over the whole roster
            addAndMakeVisible (searchBox);
            searchBox.setTextToShowWhenEmpty (juce::String (juce::CharPointer_UTF8 (
                "Buscar preset o librer\xC3\xAD" "a\xE2\x80\xA6")), ne::faint);
            searchBox.setColour (juce::TextEditor::backgroundColourId, ne::panel2);
            searchBox.setColour (juce::TextEditor::textColourId, ne::ink);
            searchBox.setColour (juce::TextEditor::outlineColourId, ne::line);
            searchBox.setColour (juce::TextEditor::focusedOutlineColourId, ne::chloro);
            searchBox.onTextChange = [this] { populatePresetCombo(); };

            importBtn.setColour (juce::TextButton::buttonColourId, ne::magenta.withAlpha (0.16f));
            importBtn.onClick = [this] { doImport(); };
            addAndMakeVisible (importBtn);

            rescanBtn.onClick = [this]
            {
                proc.rescanLibrary();
                loadNames();
                populatePresetCombo();
                refreshName();
            };
            addAndMakeVisible (rescanBtn);

            // A/B compare
            abCopy.setColour (juce::TextButton::buttonColourId, ne::panel2);
            abA.onClick    = [this] { selectSlot (0); };
            abB.onClick    = [this] { selectSlot (1); };
            abCopy.onClick = [this]
            {
                captureTo (activeSlot == 0 ? slotA : slotB);          // snapshot current
                (activeSlot == 0 ? slotB : slotA) = (activeSlot == 0 ? slotA : slotB); // copy to the other
            };
            abCopy.setTooltip ("Copiar el estado actual al otro (A<->B)");
            addAndMakeVisible (abA);
            addAndMakeVisible (abB);
            addAndMakeVisible (abCopy);

            addAndMakeVisible (statusLbl);
            statusLbl.setJustificationType (juce::Justification::centredRight);
            statusLbl.setFont (juce::Font (11.0f));
            statusLbl.setColour (juce::Label::textColourId, ne::faint);

            // ---- scrolling control content ----
            addAndMakeVisible (viewport);
            viewport.setViewedComponent (&content, false);
            viewport.setScrollBarsShown (true, false);

            buildSections();
            loadNames();
            populatePresetCombo();
            refreshName();

            // seed both A/B slots from the current state
            captureTo (slotA);
            slotB = slotA;
            updateAB();

            setResizable (true, true);
            setResizeLimits (600, 460, 1100, 1000);
            setSize (780, 660);
            startTimerHz (8);
        }

        ~NativeEditor() override
        {
            stopTimer();
            setLookAndFeel (nullptr);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (ne::bg);
            // top bar backing
            g.setColour (ne::panel);
            g.fillRect (getLocalBounds().removeFromTop (kBarH));
            g.setColour (ne::line);
            g.fillRect (0, kBarH - 1, getWidth(), 1);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto bar = r.removeFromTop (kBarH).reduced (12, 6);

            // row 1 — title | current preset | count
            auto row1 = bar.removeFromTop (24);
            title.setBounds (row1.removeFromLeft (150));
            statusLbl.setBounds (row1.removeFromRight (150));
            presetName.setBounds (row1);

            bar.removeFromTop (4);

            // row 2 — prev | next | combo | A B Copy
            auto row2 = bar.removeFromTop (32);
            prev.setBounds (row2.removeFromLeft (32).reduced (1, 2));
            next.setBounds (row2.removeFromLeft (32).reduced (1, 2));
            row2.removeFromLeft (6);
            abCopy.setBounds (row2.removeFromRight (56).reduced (1, 2));
            abB.setBounds (row2.removeFromRight (30).reduced (1, 2));
            abA.setBounds (row2.removeFromRight (30).reduced (1, 2));
            row2.removeFromRight (6);
            presetCombo.setBounds (row2.reduced (2, 2));

            bar.removeFromTop (4);

            // row 3 — search | import | rescan
            auto row3 = bar.removeFromTop (32);
            importBtn.setBounds (row3.removeFromRight (128).reduced (2, 2));
            rescanBtn.setBounds (row3.removeFromRight (76).reduced (2, 2));
            row3.removeFromRight (6);
            searchBox.setBounds (row3.reduced (2, 2));

            viewport.setBounds (r);
            layoutContent();
        }

    private:
        static constexpr int kBarH = 112;

        //  Rotary with Shift = fine drag and Shift = fine wheel.
        struct Rotary : juce::Slider
        {
            Rotary() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow) {}
            void mouseDown (const juce::MouseEvent& e) override
            {
                setMouseDragSensitivity (e.mods.isShiftDown() ? 900 : 240);
                juce::Slider::mouseDown (e);
            }
            void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
            {
                juce::MouseWheelDetails d = w;
                if (e.mods.isShiftDown()) { d.deltaX *= 0.25f; d.deltaY *= 0.25f; }
                juce::Slider::mouseWheelMove (e, d);
            }
        };

        struct Ctl
        {
            std::unique_ptr<juce::Slider> slider;
            std::unique_ptr<juce::Label>  name;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        };
        struct Section
        {
            juce::String tag, title;
            juce::StringArray ids;
            std::unique_ptr<juce::Label> header;
            std::vector<Ctl> ctls;
        };

        //  Normalised 0..1 -> human, unit-bearing readout — mirrors the WebView
        //  (ui/src/format.ts) and the DSP mappings, so the native editor reads
        //  like the instrument sounds (Hz, dB, st, ms, LP/HP, Up/Down…).
        static juce::String nativeFormat (const juce::String& id, double v)
        {
            static const char* ARP[]  = { "Up", "Down", "Up-Down", "Random" };
            static const char* SCAL[] = { "Cromatica", "Mayor", "Menor", "Pentatonica", "Dorica" };
            static const char* FILT[] = { "LP", "BP", "HP" };
            auto stepv = [] (double x, const char* const* list, int n)
            { return juce::String (list[juce::jlimit (0, n - 1, (int) std::floor (x * (n - 0.0001)))]); };

            if (id == "grainDensity") return juce::String (juce::roundToInt (2 + v * 198)) + " gr/s";
            if (id == "grainSize")    return juce::String (juce::roundToInt (8 + v * 392)) + " ms";
            if (id == "pitchA" || id == "pitchB") { const double s = (v - 0.5) * 24.0; return (s >= 0 ? "+" : "") + juce::String (s, 1) + " st"; }
            if (id == "outputGain")   { const double db = v <= 0.0001 ? -60.0 : 20.0 * std::log10 (v); return (db > -0.05 ? juce::String ("0.0") : juce::String (db, 1)) + " dB"; }
            if (id == "filterCutoff") { const double hz = 20.0 * std::exp (v * 6.9077); return hz >= 1000.0 ? juce::String (hz / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }
            if (id == "filterReso")   return juce::String (0.5 + v * 9.5, 1) + " Q";
            if (id == "filterType")   return stepv (v, FILT, 3);
            if (id == "filterMod")    return juce::String (juce::roundToInt (v * 100)) + "%";
            if (id == "unison")       return juce::String (1 + juce::roundToInt (v * 6)) + " voces";
            if (id == "unisonDetune") return juce::String::fromUTF8 ("\xC2\xB1") + juce::String (juce::roundToInt (v * 50)) + " cent";
            if (id == "stereoWidth")  return juce::String (juce::roundToInt (v * 200)) + "%";
            if (id == "delayTime")    return juce::String (juce::roundToInt (20 + v * 730)) + " ms";
            if (id == "arpOn" || id == "arpSync") return v > 0.5 ? "On" : "Off";
            if (id == "arpMode")      return stepv (v, ARP, 4);
            if (id == "arpRate")      return juce::String (0.5 + v * 19.5, 1) + " Hz";
            if (id == "scaleType")    return stepv (v, SCAL, 5);
            return juce::String (juce::roundToInt (v * 100)) + "%";
        }

        void addCtl (Section& sec, const juce::String& id, const char* accent)
        {
            const auto* def = findDef (id);
            Ctl c;
            c.slider = std::make_unique<Rotary>();
            c.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 15);
            c.slider->setTextBoxIsEditable (false);   // display-only readout; drag to change
            c.slider->setColour (juce::Slider::textBoxTextColourId, ne::ink);
            c.slider->getProperties().set ("accent", accent);
            c.slider->setScrollWheelEnabled (true);   // mouse wheel adjusts
            if (def) c.slider->setDoubleClickReturnValue (true, def->defaultValue); // dbl-click = default
            content.addAndMakeVisible (*c.slider);

            c.name = std::make_unique<juce::Label>();
            c.name->setText (def ? juce::String (def->name) : id, juce::dontSendNotification);
            c.name->setJustificationType (juce::Justification::centred);
            c.name->setFont (juce::Font (10.0f).withExtraKerningFactor (0.04f));
            c.name->setColour (juce::Label::textColourId, ne::dim);
            content.addAndMakeVisible (*c.name);

            c.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                proc.state(), id, *c.slider);
            //  Override the attachment's default (raw 0..1) text with our
            //  unit-bearing readout — must be set AFTER the attachment, whose
            //  constructor installs its own textFromValueFunction.
            const juce::String pid = id;
            c.slider->textFromValueFunction = [pid] (double v) { return nativeFormat (pid, v); };
            c.slider->updateText();   // show the formatted readout from the start

            sec.ctls.push_back (std::move (c));
        }

        const params::Def* findDef (const juce::String& id) const
        {
            for (const auto& d : params::kDefs)
                if (id == juce::String (d.id)) return &d;
            return nullptr;
        }

        void buildSections()
        {
            struct SDef { const char* tag; const char* title; std::vector<const char*> ids; const char* accent; };
            const std::vector<SDef> defs = {
                { "MOD",  "Capillary Modulator", { "caudal","soilDensity","saturation","modDepth" }, "a" },
                { "GRAIN","Granular Cloud",      { "grainDensity","grainSize","position","spray" }, "a" },
                { "A\xC3\x97""B","Diploid Genome",{ "pitchA","pitchB","crossBlend","outputGain" }, "mixed" },
                { "SVF",  "Filtro",              { "filterType","filterCutoff","filterReso","filterMod" }, "a" },
                { "TONE", "Textura & Espacio",   { "drive","unison","unisonDetune","stereoWidth" }, "a" },
                { "ARP",  "Arpegiador & Escala", { "arpOn","arpRate","arpMode","arpSync","scaleType" }, "a" },
                { "FX",   "FX \xC2\xB7 Espacio", { "delayMix","delayTime","delayFb","reverbMix","reverbSize","reverbDamp" }, "a" },
            };

            for (const auto& d : defs)
            {
                Section sec;
                sec.tag = d.tag; sec.title = d.title;
                sec.header = std::make_unique<juce::Label>();
                sec.header->setText (juce::String (juce::CharPointer_UTF8 (d.tag)) + "   "
                                     + juce::String (juce::CharPointer_UTF8 (d.title)),
                                     juce::dontSendNotification);
                sec.header->setFont (juce::Font (11.0f, juce::Font::bold).withExtraKerningFactor (0.08f));
                sec.header->setColour (juce::Label::textColourId, ne::chloro);
                content.addAndMakeVisible (*sec.header);

                for (const char* id : d.ids)
                {
                    const char* accent = "a";
                    if (std::strcmp (d.accent, "mixed") == 0)
                        accent = std::strcmp (id, "pitchA") == 0 ? "a"
                               : std::strcmp (id, "pitchB") == 0 ? "b" : "n";
                    addCtl (sec, id, accent);
                }
                sections.push_back (std::move (sec));
            }
        }

        void layoutContent()
        {
            const int W = viewport.getWidth() - 14;   // account for scrollbar
            const int pad = 14, cell = 84, rowH = 96, headH = 24;
            const int perRow = juce::jmax (3, W / cell);
            int y = pad;

            for (auto& sec : sections)
            {
                sec.header->setBounds (pad, y, W - pad, headH);
                y += headH;
                const int n = (int) sec.ctls.size();
                const int cols = juce::jmin (perRow, n);
                const int cw = (W - pad) / cols;
                for (int i = 0; i < n; ++i)
                {
                    const int col = i % cols, rr = i / cols;
                    auto& c = sec.ctls[(size_t) i];
                    juce::Rectangle<int> cellR (pad + col * cw, y + rr * rowH, cw, rowH);
                    c.name->setBounds (cellR.removeFromBottom (16));
                    c.slider->setBounds (cellR.reduced (6, 2));
                }
                const int rows = (n + cols - 1) / cols;
                y += rows * rowH + 10;
            }
            content.setSize (viewport.getWidth(), y + pad);
        }

        // ---- preset browser ----
        //  Rebuilds the dropdown from the cached roster, filtered by the search
        //  box (matches name or library, case-insensitive). Item ids stay the
        //  program index + 1 so a filtered pick still loads the right preset.
        void populatePresetCombo()
        {
            presetCombo.clear (juce::dontSendNotification);
            const juce::String q = searchBox.getText().trim().toLowerCase();
            juce::String lastLib;
            auto* menu = presetCombo.getRootMenu();
            int shown = 0;
            for (int i = 0; i < allNames.size(); ++i)
            {
                const juce::String& full = allNames.getReference (i);
                if (q.isNotEmpty() && ! full.toLowerCase().contains (q))
                    continue;
                juce::String lib = "PRESET", nm = full;
                const int sep = full.indexOf (" > ");
                if (sep >= 0) { lib = full.substring (0, sep); nm = full.substring (sep + 3); }
                if (lib != lastLib) { menu->addSectionHeader (lib); lastLib = lib; }
                presetCombo.addItem (nm.isEmpty() ? full : nm, i + 1);
                ++shown;
            }
            presetCombo.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
            statusLbl.setText (q.isEmpty() ? juce::String (allNames.size()) + " presets"
                                           : juce::String (shown) + " / " + juce::String (allNames.size()),
                               juce::dontSendNotification);
        }

        void refreshName()
        {
            const int idx = proc.getCurrentProgram();
            lastProgram = idx;
            presetCombo.setSelectedId (idx + 1, juce::dontSendNotification);
            const juce::String full = proc.getProgramName (idx);
            presetName.setText (full, juce::dontSendNotification);
        }

        void step (int dir)
        {
            const int n = proc.getNumPrograms();
            if (n <= 0) return;
            int idx = juce::jlimit (0, n - 1, proc.getCurrentProgram() + dir);
            proc.setCurrentProgram (idx);
            refreshName();
        }

        void doImport()
        {
            auto initial = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile ("Phenotype").getChildFile ("Presets");
            chooser = std::make_unique<juce::FileChooser> (
                "Importar banco / DLC  (.phbank o carpeta)", initial, "*.phbank");
            const auto fbFlags = juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::canSelectDirectories;
            importBtn.setEnabled (false);
            chooser->launchAsync (fbFlags, [this] (const juce::FileChooser& fc)
            {
                const juce::File f = fc.getResult();
                if (f != juce::File())
                {
                    proc.importBank (f);
                    loadNames();
                    populatePresetCombo();
                    refreshName();
                }
                importBtn.setEnabled (true);
            });
        }

        // ---- A/B compare (two live snapshots of all params) ----
        void captureTo (std::array<float, params::kDefs.size()>& slot)
        {
            for (size_t i = 0; i < params::kDefs.size(); ++i)
                if (auto* v = proc.state().getRawParameterValue (params::kDefs[i].id))
                    slot[i] = v->load();
        }
        void applyFrom (const std::array<float, params::kDefs.size()>& slot)
        {
            for (size_t i = 0; i < params::kDefs.size(); ++i)
                if (auto* p = proc.state().getParameter (params::kDefs[i].id))
                    p->setValueNotifyingHost (slot[i]);   // params are 0..1, raw == normalised
        }
        void selectSlot (int s)
        {
            if (s == activeSlot) return;
            captureTo (activeSlot == 0 ? slotA : slotB);       // stash current edits
            activeSlot = s;
            applyFrom (activeSlot == 0 ? slotA : slotB);
            updateAB();
        }
        void updateAB()
        {
            auto tint = [this] (juce::TextButton& b, bool on)
            {
                b.setColour (juce::TextButton::buttonColourId, on ? ne::chloro : ne::panel2);
                b.setColour (juce::TextButton::textColourOffId, on ? ne::bg : ne::ink);
                b.repaint();
            };
            tint (abA, activeSlot == 0);
            tint (abB, activeSlot == 1);
        }

        void loadNames()
        {
            allNames.clearQuick();
            const int n = proc.getNumPrograms();
            for (int i = 0; i < n; ++i)
                allNames.add (proc.getProgramName (i));
        }

        void timerCallback() override
        {
            // reflect external program changes (host automation / preset recall)
            const int idx = proc.getCurrentProgram();
            if (idx != lastProgram) refreshName();
        }

        PhenotypeAudioProcessor& proc;
        NeonLNF lnf;

        juce::Label title, presetName, statusLbl;
        juce::TextButton prev { juce::String (juce::CharPointer_UTF8 ("\xE2\x97\x80")) };
        juce::TextButton next { juce::String (juce::CharPointer_UTF8 ("\xE2\x96\xB6")) };
        //  Split the "\xC3\xAD" escape from the trailing 'a' — otherwise MSVC
        //  parses "\xADa" as one (out-of-range) hex escape.
        juce::TextButton importBtn { juce::String (juce::CharPointer_UTF8 ("Importar librer\xC3\xAD" "a")) },
                         rescanBtn { "Rescan" };
        juce::ComboBox presetCombo;
        juce::TextEditor searchBox;
        juce::TextButton abA { "A" }, abB { "B" }, abCopy { "Copy" };

        juce::Viewport viewport;
        juce::Component content;
        std::vector<Section> sections;
        std::unique_ptr<juce::FileChooser> chooser;
        int lastProgram = -1;

        juce::StringArray allNames;                             // full roster (search cache)
        std::array<float, params::kDefs.size()> slotA {}, slotB {};
        int activeSlot = 0;                                     // 0 = A, 1 = B

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativeEditor)
    };
}
