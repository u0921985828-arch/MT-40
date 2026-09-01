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
            // Brand wordmark + double-helix mark are painted in paint(); the
            // `title`/`presetName` labels stay only as data holders (presetName
            // mirrors the current program name for the centre screen).
            title.setText ("PHENOTYPE", juce::dontSendNotification);
            presetName.setJustificationType (juce::Justification::centred);
            presetName.setFont (juce::Font (14.0f, juce::Font::bold));
            presetName.setColour (juce::Label::textColourId, ne::chloro);

            // bottom-bar centre tagline (mirrors the WebView footer)
            bottomMid.setText (juce::String (juce::CharPointer_UTF8 (
                "granular cross-synthesis \xC2\xB7 capillary modulation")),
                juce::dontSendNotification);
            bottomMid.setJustificationType (juce::Justification::centred);
            bottomMid.setFont (juce::Font (11.0f).withExtraKerningFactor (0.14f));
            bottomMid.setColour (juce::Label::textColourId, ne::faint);
            addAndMakeVisible (bottomMid);

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
            presetCombo.setJustificationType (juce::Justification::centred);
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

            // A/B compare — the bottom-bar genome chips
            abCopy.setColour (juce::TextButton::buttonColourId, ne::panel2);
            abA.setColour (juce::TextButton::textColourOffId, ne::chloro);
            abB.setColour (juce::TextButton::textColourOffId, ne::magenta);
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

            addAndMakeVisible (meter);

            buildSections();
            loadNames();
            searchBox.setText (settings().getValue ("search", ""), juce::dontSendNotification);
            populatePresetCombo();
            refreshName();

            // seed both A/B slots from the current state
            captureTo (slotA);
            slotB = slotA;
            updateAB();

            setResizable (true, true);
            setResizeLimits (720, 560, 1400, 1100);
            setSize (juce::jlimit (720, 1400, settings().getIntValue ("w", 940)),
                     juce::jlimit (560, 1100, settings().getIntValue ("h", 680)));
            startTimerHz (12);
        }

        ~NativeEditor() override
        {
            settings().setValue ("w", getWidth());
            settings().setValue ("h", getHeight());
            settings().setValue ("search", searchBox.getText());
            settings().saveIfNeeded();
            stopTimer();
            setLookAndFeel (nullptr);
        }

        //  The chassis: a top bar (brand · presets · status), a thin utility
        //  strip, the control bay, and a bottom readout bar — mirrors the
        //  WebView HUD so both editors read as the same instrument.
        void paint (juce::Graphics& g) override
        {
            const int W = getWidth(), H = getHeight();
            g.fillAll (ne::bg);

            // --- top bar + utility strip backing ---
            g.setColour (ne::panel);
            g.fillRect (0, 0, W, topH);
            g.setColour (ne::line);
            g.fillRect (0, topH - 1, W, 1);
            g.fillRect (0, 56, W, 1);   // divider between bar and utility strip

            // --- bottom bar backing ---
            g.setColour (ne::panel);
            g.fillRect (0, H - botH, W, botH);
            g.setColour (ne::line);
            g.fillRect (0, H - botH, W, 1);

            // --- brand: double-helix mark + two-tone wordmark + tag ---
            drawMark (g, juce::Rectangle<float> (14.0f, 13.0f, 24.0f, 30.0f));
            const float tx = 48.0f;
            g.setFont (juce::Font (18.0f, juce::Font::bold).withExtraKerningFactor (0.16f));
            const juce::String pre = "PHENO", suf = "TYPE";
            const float preW = g.getCurrentFont().getStringWidthFloat (pre);
            g.setColour (ne::ink);    g.drawText (pre, juce::Rectangle<float> (tx, 14.0f, preW + 4, 22.0f), juce::Justification::centredLeft);
            g.setColour (ne::chloro); g.drawText (suf, juce::Rectangle<float> (tx + preW + 3, 14.0f, 90.0f, 22.0f), juce::Justification::centredLeft);
            g.setColour (ne::faint);
            g.setFont (juce::Font (9.0f).withExtraKerningFactor (0.18f));
            g.drawText ("GRANULAR DIPLOIDE", juce::Rectangle<float> (tx, 34.0f, 200.0f, 12.0f), juce::Justification::centredLeft);

            // --- status dot (top-right, before the preset count) ---
            const bool live = proc.engine().activeGrains() > 0;
            const auto dot = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ (float) (W - 160), 27.0f });
            g.setColour (live ? ne::chloro : ne::line);
            g.fillEllipse (dot);
            if (live) { g.setColour (ne::chloro.withAlpha (0.35f)); g.drawEllipse (dot.expanded (2.5f), 1.5f); }
        }

        //  Two counter-rotating gradient arcs = the DNA mark from the WebView.
        static void drawMark (juce::Graphics& g, juce::Rectangle<float> area)
        {
            auto strand = [&] (juce::Colour c, float deg)
            {
                juce::Path p;
                p.addRoundedRectangle (area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                                       area.getWidth() * 0.5f, area.getHeight() * 0.36f);
                p.applyTransform (juce::AffineTransform::rotation (
                    juce::degreesToRadians (deg), area.getCentreX(), area.getCentreY()));
                g.setColour (c);
                g.strokePath (p, juce::PathStrokeType (2.0f));
            };
            strand (ne::chloro,  24.0f);
            strand (ne::magenta, -24.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds();

            // ---- top bar: brand (painted) | preset screen | status ----
            auto bar = r.removeFromTop (56).reduced (12, 10);
            bar.removeFromLeft (210);                       // brand zone (painted)
            statusLbl.setBounds (bar.removeFromRight (150)); // preset count
            bar.removeFromRight (16);                        // room for the live dot
            auto centre = bar.withSizeKeepingCentre (juce::jmin (480, bar.getWidth()), bar.getHeight());
            prev.setBounds (centre.removeFromLeft (34).reduced (1, 1));
            centre.removeFromLeft (6);
            next.setBounds (centre.removeFromRight (34).reduced (1, 1));
            centre.removeFromRight (6);
            presetCombo.setBounds (centre.reduced (0, 1));

            // ---- utility strip: search | rescan | import ----
            auto strip = r.removeFromTop (topH - 56).reduced (12, 5);
            importBtn.setBounds (strip.removeFromRight (150).reduced (2, 0));
            rescanBtn.setBounds (strip.removeFromRight (80).reduced (2, 0));
            strip.removeFromRight (8);
            searchBox.setBounds (strip.reduced (2, 0));

            // ---- bottom bar: A B Copy | tagline | grains ----
            auto bot = r.removeFromBottom (botH).reduced (12, 5);
            abA.setBounds (bot.removeFromLeft (30).reduced (1, 1));
            bot.removeFromLeft (4);
            abB.setBounds (bot.removeFromLeft (30).reduced (1, 1));
            bot.removeFromLeft (6);
            abCopy.setBounds (bot.removeFromLeft (54).reduced (1, 1));
            meter.setBounds (bot.removeFromRight (168));
            bottomMid.setBounds (bot);

            // ---- control bay ----
            viewport.setBounds (r);
            layoutContent();
        }

    private:
        static constexpr int topH = 92;   // brand/preset/status bar + utility strip
        static constexpr int botH = 34;   // bottom readout bar

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

        //  Live activity strip: capillary-level bar + grain count.
        struct Meter : juce::Component
        {
            int grains = 0; float level = 0.0f;
            void set (int g, float l) { grains = g; level = juce::jlimit (0.0f, 1.0f, l); repaint(); }
            void paint (juce::Graphics& g) override
            {
                auto r = getLocalBounds();
                auto bar = r.removeFromLeft (72).withSizeKeepingCentre (66, 5).toFloat();
                g.setColour (ne::line);   g.fillRoundedRectangle (bar, 2.5f);
                g.setColour (ne::chloro);  g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * level), 2.5f);
                g.setColour (ne::dim);     g.setFont (juce::Font (11.0f));
                g.drawText (juce::String (grains) + " granos", r.reduced (6, 0),
                            juce::Justification::centredLeft);
            }
        };

        //  A section card: rounded panel background + hairline border, so the
        //  rack reads as ordered tiles rather than a loose column of knobs.
        struct Panel : juce::Component
        {
            void paint (juce::Graphics& g) override
            {
                auto r = getLocalBounds().toFloat();
                g.setColour (ne::panel2);
                g.fillRoundedRectangle (r, 9.0f);
                g.setColour (ne::line);
                g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
            }
        };

        //  Per-user settings (window size + last search), persisted across sessions.
        static juce::PropertiesFile& settings()
        {
            static juce::PropertiesFile pf ( []
            {
                juce::PropertiesFile::Options o;
                o.applicationName     = "Phenotype";
                o.filenameSuffix      = "settings";
                o.folderName          = "Phenotype";
                o.osxLibrarySubFolder = "Application Support";
                return o;
            }() );
            return pf;
        }

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
            std::unique_ptr<Panel> panel;
            std::unique_ptr<juce::Label> header;
            std::vector<Ctl> ctls;
            int perRow = 4, rows = 1;   // filled in during layout
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
            sec.panel->addAndMakeVisible (*c.slider);

            c.name = std::make_unique<juce::Label>();
            c.name->setText (def ? juce::String (def->name) : id, juce::dontSendNotification);
            c.name->setJustificationType (juce::Justification::centred);
            c.name->setFont (juce::Font (10.0f).withExtraKerningFactor (0.04f));
            c.name->setColour (juce::Label::textColourId, ne::dim);
            sec.panel->addAndMakeVisible (*c.name);

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
                sec.panel = std::make_unique<Panel>();
                content.addAndMakeVisible (*sec.panel);
                sec.header = std::make_unique<juce::Label>();
                sec.header->setText (juce::String (juce::CharPointer_UTF8 (d.tag)) + "   "
                                     + juce::String (juce::CharPointer_UTF8 (d.title)),
                                     juce::dontSendNotification);
                sec.header->setFont (juce::Font (11.0f, juce::Font::bold).withExtraKerningFactor (0.08f));
                sec.header->setColour (juce::Label::textColourId, ne::chloro);
                sec.panel->addAndMakeVisible (*sec.header);

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

        //  Single-tab, no-scroll masonry. Sections become cards packed into 1–3
        //  columns (by width); the knob row height is then solved so the tallest
        //  column fits the visible height exactly — everything on one page.
        void layoutContent()
        {
            const int Vw = viewport.getWidth();
            const int Vh = viewport.getHeight();
            const int pad = 12, gap = 10, headH = 20, pt = 8, pb = 8, labelH = 14, cellMin = 78;

            const int ncol = Vw >= 1180 ? 3 : Vw >= 720 ? 2 : 1;
            const int avail = Vw - pad * 2;
            const int colW  = (avail - (ncol - 1) * gap) / ncol;

            // per-section geometry (perRow limited by card width, then row count)
            for (auto& sec : sections)
            {
                const int n = (int) sec.ctls.size();
                const int fit = juce::jmax (1, (colW - 2 * pt) / cellMin);
                sec.perRow = juce::jlimit (1, juce::jmin (n, 4), fit);
                sec.rows   = (n + sec.perRow - 1) / sec.perRow;
            }

            // greedy masonry: drop each card into the currently-shortest column
            std::vector<std::vector<int>> cols ((size_t) ncol);
            std::vector<double> load ((size_t) ncol, 0.0);
            for (int s = 0; s < (int) sections.size(); ++s)
            {
                int best = 0;
                for (int c = 1; c < ncol; ++c) if (load[(size_t) c] < load[(size_t) best]) best = c;
                cols[(size_t) best].push_back (s);
                load[(size_t) best] += sections[(size_t) s].rows + 0.42; // header ≈ 0.42 rows
            }

            // solve the knob row height R so the tightest column fits the height
            const int availH = Vh - pad * 2;
            int R = 92;
            for (const auto& col : cols)
            {
                if (col.empty()) continue;
                int fixedPx = (int) col.size() * (pt + headH + pb) + ((int) col.size() - 1) * gap;
                int rowsPx  = 0; for (int s : col) rowsPx += sections[(size_t) s].rows;
                if (rowsPx > 0) R = juce::jmin (R, (availH - fixedPx) / rowsPx);
            }
            R = juce::jlimit (46, 92, R);

            // place cards column by column, and knobs inside each card
            int maxBottom = pad;
            for (int c = 0; c < ncol; ++c)
            {
                int x = pad + c * (colW + gap);
                int y = pad;
                for (int s : cols[(size_t) c])
                {
                    auto& sec = sections[(size_t) s];
                    const int n = (int) sec.ctls.size();
                    const int panelH = pt + headH + sec.rows * R + pb;
                    sec.panel->setBounds (x, y, colW, panelH);

                    auto inner = juce::Rectangle<int> (0, 0, colW, panelH).reduced (pt, 0);
                    inner.removeFromTop (pt);
                    sec.header->setBounds (inner.removeFromTop (headH));
                    const int cellW = inner.getWidth() / sec.perRow;
                    for (int i = 0; i < n; ++i)
                    {
                        const int cc = i % sec.perRow, rr = i / sec.perRow;
                        auto& ct = sec.ctls[(size_t) i];
                        juce::Rectangle<int> cell (inner.getX() + cc * cellW, inner.getY() + rr * R, cellW, R);
                        ct.name->setBounds (cell.removeFromBottom (labelH));
                        ct.slider->setBounds (cell.reduced (3, 1));
                    }
                    y += panelH + gap;
                }
                maxBottom = juce::jmax (maxBottom, y);
            }
            // exact fit → no scrollbar; only a shrunk-below-minimum window scrolls
            content.setSize (Vw, juce::jmax (Vh, maxBottom + pad - gap));
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
            meter.set (proc.engine().activeGrains(), proc.engine().capillaryLevel());
            // reflect external program changes (host automation / preset recall)
            const int idx = proc.getCurrentProgram();
            if (idx != lastProgram) refreshName();
        }

        PhenotypeAudioProcessor& proc;
        NeonLNF lnf;

        juce::Label title, presetName, statusLbl, bottomMid;
        juce::TextButton prev { juce::String (juce::CharPointer_UTF8 ("\xE2\x97\x80")) };
        juce::TextButton next { juce::String (juce::CharPointer_UTF8 ("\xE2\x96\xB6")) };
        //  Split the "\xC3\xAD" escape from the trailing 'a' — otherwise MSVC
        //  parses "\xADa" as one (out-of-range) hex escape.
        juce::TextButton importBtn { juce::String (juce::CharPointer_UTF8 ("Importar librer\xC3\xAD" "a")) },
                         rescanBtn { "Rescan" };
        juce::ComboBox presetCombo;
        juce::TextEditor searchBox;
        juce::TextButton abA { "A" }, abB { "B" }, abCopy { "Copy" };
        Meter meter;

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
