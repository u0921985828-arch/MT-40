#include "PluginEditor.h"
#include "WebAssets.h"

namespace
{
    juce::String mimeForExtension (const juce::String& filename)
    {
        if (filename.endsWith (".html")) return "text/html";
        if (filename.endsWith (".css"))  return "text/css";
        if (filename.endsWith (".js"))   return "text/javascript";
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
    setResizeLimits (960, 600, 1800, 1100);
    setSize (1120, 700);
}

void MoogSynthAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
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
