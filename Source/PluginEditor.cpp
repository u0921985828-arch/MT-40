#include "PluginEditor.h"
#include "WebAssets.h"

namespace
{
    juce::String mimeForExtension (const juce::String& filename)
    {
        if (filename.endsWith (".html")) return "text/html;charset=utf-8";
        if (filename.endsWith (".css"))  return "text/css;charset=utf-8";
        if (filename.endsWith (".js"))   return "text/javascript;charset=utf-8";
        if (filename.endsWith (".svg"))  return "image/svg+xml";
        if (filename.endsWith (".json")) return "application/json";
        return "application/octet-stream";
    }
}

MoogSynthAudioProcessorEditor::MoogSynthAudioProcessorEditor (MoogSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // ---- Build a relay for every parameter, remembering the pairing --------
    std::vector<std::pair<juce::RangedAudioParameter*, juce::WebSliderRelay*>>       sliderPairs;
    std::vector<std::pair<juce::RangedAudioParameter*, juce::WebToggleButtonRelay*>> togglePairs;
    std::vector<std::pair<juce::RangedAudioParameter*, juce::WebComboBoxRelay*>>     comboPairs;

    for (auto* param : processorRef.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param);
        if (ranged == nullptr || withID == nullptr)
            continue;

        const auto id = withID->paramID;

        if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
            togglePairs.push_back ({ ranged, toggleRelays.add (new juce::WebToggleButtonRelay (id)) });
        else if (dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr)
            comboPairs.push_back ({ ranged, comboRelays.add (new juce::WebComboBoxRelay (id)) });
        else
            sliderPairs.push_back ({ ranged, sliderRelays.add (new juce::WebSliderRelay (id)) });
    }

    // ---- Assemble the WebBrowserComponent options --------------------------
    auto options = juce::WebBrowserComponent::Options {}
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const auto& url) { return getResource (url); })
        .withNativeFunction ("noteOn",
            [this] (const juce::Array<juce::var>& args,
                    juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() >= 1)
                {
                    const int   note = (int) args[0];
                    const float vel  = args.size() >= 2 ? (float) args[1] : 0.8f;
                    processorRef.getKeyboardState().noteOn (1, note, vel);
                }
                complete (juce::var());
            })
        .withNativeFunction ("noteOff",
            [this] (const juce::Array<juce::var>& args,
                    juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() >= 1)
                    processorRef.getKeyboardState().noteOff (1, (int) args[0], 0.0f);
                complete (juce::var());
            })
        .withNativeFunction ("getPresetBank",
            [this] (const juce::Array<juce::var>&,
                    juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty ("bank", processorRef.getPresetManager().getBankJson());
                obj->setProperty ("current", processorRef.getPresetManager().getCurrentPreset());
                complete (juce::var (obj));
            })
        .withNativeFunction ("loadPreset",
            [this] (const juce::Array<juce::var>& args,
                    juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() >= 1)
                    processorRef.getPresetManager().loadPreset (args[0].toString());
                complete (juce::var());
            })
        .withNativeFunction ("savePreset",
            [this] (const juce::Array<juce::var>& args,
                    juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() >= 1)
                    processorRef.getPresetManager().savePreset (args[0].toString());
                complete (juce::var());
            })
        .withNativeFunction ("pitchBend",
            [this] (const juce::Array<juce::var>& args,
                    juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() >= 1)
                    processorRef.setUiPitchBend (juce::jlimit (-1.0f, 1.0f, (float) args[0]));
                complete (juce::var());
            });

    for (auto* r : sliderRelays) options = options.withOptionsFrom (*r);
    for (auto* r : toggleRelays) options = options.withOptionsFrom (*r);
    for (auto* r : comboRelays)  options = options.withOptionsFrom (*r);

    webView = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webView);

    // ---- Bind each relay to its parameter ----------------------------------
    for (auto& [param, relay] : sliderPairs)
        sliderAttachments.add (new juce::WebSliderParameterAttachment (*param, *relay));
    for (auto& [param, relay] : togglePairs)
        toggleAttachments.add (new juce::WebToggleButtonParameterAttachment (*param, *relay));
    for (auto& [param, relay] : comboPairs)
        comboAttachments.add (new juce::WebComboBoxParameterAttachment (*param, *relay));

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (1040, 560, 1900, 1000);
    setSize (1520, 700);

    startTimerHz (30); // drive the visualiser
}

MoogSynthAudioProcessorEditor::~MoogSynthAudioProcessorEditor()
{
    stopTimer();
}

void MoogSynthAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void MoogSynthAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr)
        return;

    // ---- Grab the latest block of output ----------------------------------
    processorRef.readScope (scopeSamples.data(), fftSize);

    // ---- Waveform (down-sampled to a fixed number of points) --------------
    constexpr int wavePoints = 220;
    juce::Array<juce::var> wave;
    wave.ensureStorageAllocated (wavePoints);
    for (int i = 0; i < wavePoints; ++i)
    {
        const int idx = (i * (fftSize - 1)) / (wavePoints - 1);
        wave.add (juce::jlimit (-1.0f, 1.0f, scopeSamples[(size_t) idx]));
    }

    // ---- Spectrum (log-spaced, in normalised dB) --------------------------
    std::copy (scopeSamples.begin(), scopeSamples.end(), fftData.begin());
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double sr = processorRef.getSampleRate() > 0.0 ? processorRef.getSampleRate() : 44100.0;
    const double nyquist = sr * 0.5;
    constexpr int specBins = 120;

    juce::Array<juce::var> spectrum;
    spectrum.ensureStorageAllocated (specBins);
    for (int b = 0; b < specBins; ++b)
    {
        const double freq = 20.0 * std::pow (1000.0, (double) b / (specBins - 1)); // 20 Hz .. 20 kHz
        int bin = (int) std::round (freq / nyquist * (fftSize / 2));
        bin = juce::jlimit (1, fftSize / 2, bin);

        const float mag = fftData[(size_t) bin] / (float) (fftSize / 2);
        const float db  = juce::Decibels::gainToDecibels (mag + 1.0e-9f);
        spectrum.add (juce::jlimit (0.0f, 1.0f, juce::jmap (db, -90.0f, 0.0f, 0.0f, 1.0f)));
    }

    // ---- Output level meters (peak-hold with decay) -----------------------
    juce::Array<juce::var> levels;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float lvl = processorRef.getMeterLevel (ch);
        meterDisplay[ch] = lvl > meterDisplay[ch] ? lvl
                                                   : meterDisplay[ch] * 0.82f; // fall
        levels.add (juce::jlimit (0.0f, 1.0f, meterDisplay[ch]));
    }

    auto* payload = new juce::DynamicObject();
    payload->setProperty ("wave", wave);
    payload->setProperty ("spectrum", spectrum);
    payload->setProperty ("levels", levels);
    webView->emitEventIfBrowserIsVisible ("visualiser", juce::var (payload));
}

std::optional<juce::WebBrowserComponent::Resource>
MoogSynthAudioProcessorEditor::getResource (const juce::String& url) const
{
    // Normalise the request to a base filename (assets have unique basenames).
    const auto path = url == "/" ? juce::String ("index.html")
                                 : url.fromLastOccurrenceOf ("/", false, false);

    for (int i = 0; i < WebAssets::namedResourceListSize; ++i)
    {
        if (juce::String (WebAssets::originalFilenames[i]) == path)
        {
            int dataSize = 0;
            const char* data = WebAssets::getNamedResource (WebAssets::namedResourceList[i], dataSize);

            const auto* bytes = reinterpret_cast<const std::byte*> (data);
            return juce::WebBrowserComponent::Resource {
                std::vector<std::byte> (bytes, bytes + dataSize),
                mimeForExtension (path)
            };
        }
    }

    return std::nullopt;
}
