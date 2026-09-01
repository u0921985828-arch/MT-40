//==============================================================================
//  PhenotypeWebEditor.cpp
//==============================================================================

#include "PhenotypeWebEditor.h"
#include "BinaryData.h"

namespace phenotype
{
    using Options  = juce::WebBrowserComponent::Options;
    using Resource = juce::WebBrowserComponent::Resource;

    namespace
    {
        //  Maps a request path ("/", "/assets/index.js") to an embedded resource
        //  name as emitted by juce_add_binary_data (slashes/dots -> underscores).
        juce::String toBinaryName (juce::String path)
        {
            if (path.isEmpty() || path == "/")
                path = "index.html";
            if (path.startsWithChar ('/'))
                path = path.substring (1);
            return path.replaceCharacters ("./-", "___");
        }

        //  Returns the first embedded resource whose original filename ends in
        //  ".html" (index.html for a real build, fallback.html otherwise).
        const char* findHtmlEntry (int& sizeOut)
        {
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                const juce::String original { BinaryData::getNamedResourceOriginalFilename (
                                                  BinaryData::namedResourceList[i]) };
                if (original.endsWithIgnoreCase (".html"))
                    return BinaryData::getNamedResource (BinaryData::namedResourceList[i], sizeOut);
            }
            return nullptr;
        }
    }

    PhenotypeWebEditor::PhenotypeWebEditor (PhenotypeAudioProcessor& p)
        : AudioProcessorEditor (p),
          processorRef (p),
          dispatcher (p.state()),
          webView (Options()
              .withNativeIntegrationEnabled()
              .withResourceProvider ([this] (const auto& url) { return provide (url); })
              .withNativeFunction ("phenotypeSend",
                   [this] (const juce::Array<juce::var>& args,
                           juce::WebBrowserComponent::NativeFunctionCompletion completion)
                   {
                       onMessageFromUi (args, std::move (completion));
                   })
              .withNativeFunction ("phenotypeProgram",
                   [this] (const juce::Array<juce::var>& args,
                           juce::WebBrowserComponent::NativeFunctionCompletion completion)
                   {
                       onProgramFromUi (args, std::move (completion));
                   })
              .withNativeFunction ("phenotypeLibrary",
                   [this] (const juce::Array<juce::var>& args,
                           juce::WebBrowserComponent::NativeFunctionCompletion completion)
                   {
                       onLibraryFromUi (args, std::move (completion));
                   }))
    {
        addAndMakeVisible (webView);
        webView.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

        //  Reflect host automation / preset recall back into the UI.
        for (const auto& d : params::kDefs)
            processorRef.state().addParameterListener (d.id, this);

        setResizable (true, true);
        setSize (1200, 846);   // viewport + compact flat console (FX bank collapsible)
        startTimerHz (kTelemetryHz);
    }

    PhenotypeWebEditor::~PhenotypeWebEditor()
    {
        stopTimer();
        for (const auto& d : params::kDefs)
            processorRef.state().removeParameterListener (d.id, this);
    }

    void PhenotypeWebEditor::resized()
    {
        webView.setBounds (getLocalBounds());
    }

    void PhenotypeWebEditor::onMessageFromUi (const juce::Array<juce::var>& args,
                                              juce::WebBrowserComponent::NativeFunctionCompletion completion)
    {
        if (args.size() > 0)
            dispatcher.handleFromUi (args[0].isString() ? juce::JSON::parse (args[0].toString())
                                                        : args[0]);
        completion (juce::var (true));
    }

    void PhenotypeWebEditor::onProgramFromUi (const juce::Array<juce::var>& args,
                                              juce::WebBrowserComponent::NativeFunctionCompletion completion)
    {
        //  {action:"next"|"prev"|"set"|"get", index?} -> {index,name,count}
        juce::var payload;
        if (args.size() > 0)
            payload = args[0].isString() ? juce::JSON::parse (args[0].toString()) : args[0];

        const juce::String action = payload.getProperty ("action", "get").toString();
        const int n   = juce::jmax (1, processorRef.getNumPrograms());
        const int cur = processorRef.getCurrentProgram();
        int idx = cur;
        if      (action == "next") idx = (cur + 1) % n;
        else if (action == "prev") idx = (cur - 1 + n) % n;
        else if (action == "set")  idx = juce::jlimit (0, n - 1, (int) payload.getProperty ("index", cur));

        if (action != "get" && idx != cur)
            processorRef.setCurrentProgram (idx);

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("index", idx);
        obj->setProperty ("count", n);
        obj->setProperty ("name",  processorRef.getProgramName (idx));
        completion (juce::var (obj));
    }

    void PhenotypeWebEditor::onLibraryFromUi (const juce::Array<juce::var>& args,
                                              juce::WebBrowserComponent::NativeFunctionCompletion completion)
    {
        juce::var payload;
        if (args.size() > 0)
            payload = args[0].isString() ? juce::JSON::parse (args[0].toString()) : args[0];
        const juce::String action = payload.getProperty ("action", "count").toString();

        auto reply = [this] (juce::WebBrowserComponent::NativeFunctionCompletion c, int imported = 0)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("count", processorRef.getNumPrograms());
            obj->setProperty ("imported", imported);
            c (juce::var (obj));
        };

        if (action == "list")
        {
            //  Full preset roster for the in-UI browser: names in program order
            //  (the UI splits "LIBRARY > name" itself). Cheap enough to send in
            //  one JSON even for large DLC libraries.
            auto* obj = new juce::DynamicObject();
            const int n = processorRef.getNumPrograms();
            juce::Array<juce::var> names;
            names.ensureStorageAllocated (n);
            for (int i = 0; i < n; ++i)
                names.add (processorRef.getProgramName (i));
            obj->setProperty ("count", n);
            obj->setProperty ("imported", 0);
            obj->setProperty ("presets", names);
            completion (juce::var (obj));
            return;
        }

        if (action == "rescan")
        {
            processorRef.rescanLibrary();
            reply (std::move (completion));
            return;
        }

        if (action == "import")
        {
            //  Native chooser: pick a .phbank file or a DLC folder. Async — the
            //  completion resolves once the copy + rescan finishes.
            bankChooser = std::make_unique<juce::FileChooser> (
                "Importar banco / DLC Phenotype",
                PresetLibrary::userPresetDir(),
                "*.phbank");

            const auto flags = juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::canSelectDirectories;

            auto shared = std::make_shared<juce::WebBrowserComponent::NativeFunctionCompletion> (std::move (completion));
            bankChooser->launchAsync (flags,
                [this, reply, shared] (const juce::FileChooser& fc)
                {
                    int imported = 0;
                    const auto result = fc.getResult();
                    if (result != juce::File() && processorRef.importBank (result))
                        imported = 1;
                    reply (std::move (*shared), imported);
                });
            return;
        }

        //  "count" (default)
        reply (std::move (completion));
    }

    void PhenotypeWebEditor::timerCallback()
    {
        //  Push a parameter snapshot whenever the tree changed (host/preset).
        if (paramsDirty.exchange (false, std::memory_order_relaxed))
            webView.emitEventIfBrowserIsVisible ("phenotypeParams",
                                                 dispatcher.buildParamSnapshot());

        processorRef.copyFftFrame (fftFrame.data());

        const auto frame = ipc::MessageDispatcher::buildTelemetry (
            fftFrame.data(),
            PhenotypeAudioProcessor::kNumBins,
            processorRef.engine().capillaryLevel(),
            processorRef.engine().activeGrains());

        webView.emitEventIfBrowserIsVisible ("phenotypeTelemetry", frame);
    }

    std::optional<Resource> PhenotypeWebEditor::provide (const juce::String& url)
    {
        const auto name = toBinaryName (url);

        int   size = 0;
        const char* data = BinaryData::getNamedResource (name.toRawUTF8(), size);
        if (data == nullptr)
        {
            //  SPA fallback: unknown route -> the embedded HTML entry point.
            //  The bundle emits index.html; a frontend-less build embeds
            //  fallback.html. Locate whichever .html resource was embedded.
            data = findHtmlEntry (size);
            if (data == nullptr)
                return std::nullopt;
        }

        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));

        const auto ext = url.fromLastOccurrenceOf (".", false, false);
        return Resource { std::move (bytes), mimeForExtension (ext) };
    }

    const char* PhenotypeWebEditor::mimeForExtension (const juce::String& ext) noexcept
    {
        if (ext == "html") return "text/html";
        if (ext == "js")   return "text/javascript";
        if (ext == "mjs")  return "text/javascript";
        if (ext == "css")  return "text/css";
        if (ext == "json") return "application/json";
        if (ext == "svg")  return "image/svg+xml";
        if (ext == "png")  return "image/png";
        if (ext == "woff2")return "font/woff2";
        return "application/octet-stream";
    }
}
