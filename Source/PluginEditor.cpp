#include "PluginEditor.h"
#include "BinaryData.h"
#include <cstring>

namespace
{
    std::vector<std::byte> toByteVector (const char* data, int size)
    {
        std::vector<std::byte> v (static_cast<size_t> (size));
        std::memcpy (v.data(), data, static_cast<size_t> (size));
        return v;
    }

    // Basename of a request path, with the query string stripped.
    juce::String basenameOf (const juce::String& url)
    {
        auto path = url.upToFirstOccurrenceOf ("?", false, false);
        return path.fromLastOccurrenceOf ("/", false, false);
    }
}

bool CasioMT40AudioProcessorEditor::SinglePageBrowser::pageAboutToLoad (const juce::String& url)
{
    // Only allow the embedded resource-provider origin (and the initial blank
    // page). There are no external links in the UI, so nothing should ever
    // navigate away.
    return url == juce::WebBrowserComponent::getResourceProviderRoot()
        || url.startsWith ("about:");
}

juce::WebBrowserComponent::Options CasioMT40AudioProcessorEditor::makeOptions()
{
    using Options = juce::WebBrowserComponent::Options;
    return Options{}
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (Options::WinWebView2{}
            .withUserDataFolder (juce::File::getSpecialLocation (
                juce::File::SpecialLocationType::tempDirectory)))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const auto& url) { return getResource (url); })
        // On-screen keyboard + transport controls -> engine.
        .withNativeFunction ("noteOn", [this] (auto& a, auto complete)
        {
            processor.injectNoteOn ((int) a[0], (float) a[1]);
            complete (juce::var());
        })
        .withNativeFunction ("noteOff", [this] (auto& a, auto complete)
        {
            processor.injectNoteOff ((int) a[0]);
            complete (juce::var());
        })
        .withNativeFunction ("synchro", [this] (auto&, auto complete)
        {
            processor.uiPressSynchro();
            complete (juce::var());
        })
        .withNativeFunction ("setFill", [this] (auto& a, auto complete)
        {
            processor.uiSetFillHeld ((bool) a[0]);
            complete (juce::var());
        })
        .withNativeFunction ("startStop", [this] (auto&, auto complete)
        {
            processor.uiStartStop();
            complete (juce::var());
        })
        .withOptionsFrom (masterRelay)
        .withOptionsFrom (rhythmRelay)
        .withOptionsFrom (bassRelay)
        .withOptionsFrom (tempoRelay)
        .withOptionsFrom (vibratoRelay)
        .withOptionsFrom (sustainRelay)
        .withOptionsFrom (modeRelay)
        .withOptionsFrom (rhythmSelRelay)
        .withOptionsFrom (presetRelay);
}

CasioMT40AudioProcessorEditor::CasioMT40AudioProcessorEditor (CasioMT40AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    webView = std::make_unique<SinglePageBrowser> (makeOptions());
    addAndMakeVisible (*webView);

    auto& apvts = processor.apvts;
    auto param = [&apvts] (const char* id) -> juce::RangedAudioParameter&
    {
        return *apvts.getParameter (id);
    };

    masterAtt  = std::make_unique<juce::WebSliderParameterAttachment>       (param (ParamIDs::masterVolume), masterRelay,  nullptr);
    rhythmAtt  = std::make_unique<juce::WebSliderParameterAttachment>       (param (ParamIDs::rhythmVolume), rhythmRelay,  nullptr);
    bassAtt    = std::make_unique<juce::WebSliderParameterAttachment>       (param (ParamIDs::bassVolume),   bassRelay,    nullptr);
    tempoAtt   = std::make_unique<juce::WebSliderParameterAttachment>       (param (ParamIDs::tempo),        tempoRelay,   nullptr);
    vibratoAtt = std::make_unique<juce::WebToggleButtonParameterAttachment> (param (ParamIDs::vibrato),      vibratoRelay, nullptr);
    sustainAtt = std::make_unique<juce::WebToggleButtonParameterAttachment> (param (ParamIDs::sustain),      sustainRelay, nullptr);
    modeAtt    = std::make_unique<juce::WebComboBoxParameterAttachment>     (param (ParamIDs::kbdMode),      modeRelay,    nullptr);
    rhythmSelAtt = std::make_unique<juce::WebComboBoxParameterAttachment>   (param (ParamIDs::rhythmIdx),    rhythmSelRelay, nullptr);
    presetAtt  = std::make_unique<juce::WebComboBoxParameterAttachment>     (param (ParamIDs::patchIdx),     presetRelay,  nullptr);

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (420, 300, 900, 620);
    setSize (560, 380);
    startTimerHz (15); // push transport state to the UI
}

CasioMT40AudioProcessorEditor::~CasioMT40AudioProcessorEditor()
{
    stopTimer();
}

std::optional<juce::WebBrowserComponent::Resource>
CasioMT40AudioProcessorEditor::getResource (const juce::String& url) const
{
    const auto name = (url == "/" || url.isEmpty()) ? juce::String ("index.html")
                                                    : basenameOf (url);

    int size = 0;
    const char* data = nullptr;
    juce::String mime;

    if (name == "index.html")
    {
        data = BinaryData::index_html; size = BinaryData::index_htmlSize; mime = "text/html";
    }
    else if (name == "index.js")
    {
        data = BinaryData::index_js; size = BinaryData::index_jsSize; mime = "text/javascript";
    }
    else if (name == "check_native_interop.js")
    {
        data = BinaryData::check_native_interop_js;
        size = BinaryData::check_native_interop_jsSize;
        mime = "text/javascript";
    }

    if (data == nullptr)
        return std::nullopt;

    return juce::WebBrowserComponent::Resource { toByteVector (data, size), std::move (mime) };
}

void CasioMT40AudioProcessorEditor::timerCallback()
{
    const int state = static_cast<int> (processor.getTransportState());
    if (state != lastTransport && webView != nullptr)
    {
        lastTransport = state;
        webView->evaluateJavascript (
            "window.mt40SetTransport && window.mt40SetTransport(" + juce::String (state) + ");");
    }
}

void CasioMT40AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1b1f));
}

void CasioMT40AudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}
