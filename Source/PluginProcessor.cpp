#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdint>
#include <map>

namespace
{
juce::String hashBytes (const void* data, size_t size)
{
    const auto* bytes = static_cast<const uint8_t*> (data);
    juce::uint64 hash = 1469598103934665603ull;

    for (size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }

    return juce::String::toHexString (static_cast<juce::int64> (hash));
}
} // namespace

LyricsSheetAudioProcessor::LyricsSheetAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    lrcSnapshot = std::make_shared<const lyrsheet::LrcDocument>();
}

void LyricsSheetAudioProcessor::prepareToPlay (double, int) {}
void LyricsSheetAudioProcessor::releaseResources() {}

bool LyricsSheetAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    return in == out;
}

void LyricsSheetAudioProcessor::resetTransport()
{
    hostValid.store (false);
    hostPlaying.store (false);
    projectSeconds.store (0.0);
    ppqPosition.store (0.0);
    lastRenderMs.store (0.0);
}

void LyricsSheetAudioProcessor::applyPosition (const juce::AudioPlayHead::PositionInfo& position)
{
    const auto sampleRate = getSampleRate();

    if (const auto timeInSamples = position.getTimeInSamples();
        timeInSamples.hasValue() && sampleRate > 0.0)
    {
        projectSeconds.store (static_cast<double> (*timeInSamples) / sampleRate);
    }
    else if (const auto timeInSeconds = position.getTimeInSeconds(); timeInSeconds.hasValue())
    {
        projectSeconds.store (*timeInSeconds);
    }

    if (const auto ppq = position.getPpqPosition())
        ppqPosition.store (*ppq);

    if (const auto bpm = position.getBpm())
        tempo.store (*bpm);

    hostPlaying.store (position.getIsPlaying());
    hostValid.store (true);
}

void LyricsSheetAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (buffer);

    // Pass-through: this plugin only observes the transport.

    lastRenderMs.store (juce::Time::getMillisecondCounterHiRes());

    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            applyPosition (*position);
}

bool LyricsSheetAudioProcessor::pollTransportFromMessageThread()
{
    auto* playHead = getPlayHead();

    if (playHead == nullptr)
        return false;

    const auto position = playHead->getPosition();

    if (! position.hasValue())
        return false;

    applyPosition (*position);
    return true;
}

juce::AudioProcessorEditor* LyricsSheetAudioProcessor::createEditor()
{
    return new LyricsSheetAudioProcessorEditor (*this);
}

LyricsSheetAudioProcessor::TransportSnapshot
LyricsSheetAudioProcessor::getTransportSnapshot() const
{
    TransportSnapshot snapshot;
    snapshot.valid = hostValid.load();
    snapshot.playing = hostPlaying.load();
    snapshot.projectSeconds = projectSeconds.load();
    snapshot.ppq = ppqPosition.load();
    snapshot.bpm = tempo.load();
    snapshot.sampleRate = getSampleRate();
    snapshot.lastRenderMs = lastRenderMs.load();
    return snapshot;
}

//== LRC ======================================================================

void LyricsSheetAudioProcessor::setLrcText (const juce::String& text,
                                            const juce::String& sourceName)
{
    auto document = std::make_shared<lyrsheet::LrcDocument>();
    document->parse (text.toStdString());

    {
        const juce::ScopedLock lock (stateLock);
        lrcText = text;
        lrcSource = sourceName;
        lrcSnapshot = document;
    }

    notifyStateChange();
}

juce::String LyricsSheetAudioProcessor::getLrcText() const
{
    const juce::ScopedLock lock (stateLock);
    return lrcText;
}

juce::String LyricsSheetAudioProcessor::getLrcSourceName() const
{
    const juce::ScopedLock lock (stateLock);
    return lrcSource;
}

std::shared_ptr<const lyrsheet::LrcDocument>
LyricsSheetAudioProcessor::getLrcSnapshot() const
{
    const juce::ScopedLock lock (stateLock);
    return lrcSnapshot;
}

//== Sheet ====================================================================

void LyricsSheetAudioProcessor::setSheetPages (const std::vector<SheetPageDescriptor>& pages)
{
    {
        const juce::ScopedLock lock (stateLock);
        sheetPages = pages;
    }

    notifyStateChange();
}

std::vector<SheetPageDescriptor> LyricsSheetAudioProcessor::getSheetPages() const
{
    const juce::ScopedLock lock (stateLock);
    return sheetPages;
}

void LyricsSheetAudioProcessor::appendSheetPages (const std::vector<SheetPageDescriptor>& pages)
{
    {
        const juce::ScopedLock lock (stateLock);
        sheetPages.insert (sheetPages.end(), pages.begin(), pages.end());
    }

    notifyStateChange();
}

//== Calibration ==============================================================

double LyricsSheetAudioProcessor::getOriginSeconds() const
{
    const juce::ScopedLock lock (stateLock);
    return originSeconds;
}

void LyricsSheetAudioProcessor::setOriginSeconds (double seconds)
{
    const juce::ScopedLock lock (stateLock);
    originSeconds = seconds;
}

double LyricsSheetAudioProcessor::getFineOffsetSeconds() const
{
    const juce::ScopedLock lock (stateLock);
    return fineOffsetSeconds;
}

void LyricsSheetAudioProcessor::setFineOffsetSeconds (double seconds)
{
    const juce::ScopedLock lock (stateLock);
    fineOffsetSeconds = seconds;
}

double LyricsSheetAudioProcessor::toContentTime (double projectTimeSeconds) const
{
    const juce::ScopedLock lock (stateLock);
    return projectTimeSeconds - originSeconds - fineOffsetSeconds;
}

//== Cues =====================================================================

std::vector<lyrsheet::PageCue> LyricsSheetAudioProcessor::getCues() const
{
    const juce::ScopedLock lock (stateLock);
    return cues.cues();
}

bool LyricsSheetAudioProcessor::addCue (double contentTimeSec, int page)
{
    {
        const juce::ScopedLock lock (stateLock);
        cues.add (contentTimeSec, page);
    }

    notifyStateChange();
    return true;
}

bool LyricsSheetAudioProcessor::removeCue (int index)
{
    bool removed = false;

    {
        const juce::ScopedLock lock (stateLock);
        removed = cues.removeAt (index);
    }

    if (removed)
        notifyStateChange();

    return removed;
}

void LyricsSheetAudioProcessor::clearCues()
{
    {
        const juce::ScopedLock lock (stateLock);
        cues.clear();
    }

    notifyStateChange();
}

int LyricsSheetAudioProcessor::getPageAt (double contentTimeSec) const
{
    const juce::ScopedLock lock (stateLock);
    return cues.pageAt (contentTimeSec);
}

int LyricsSheetAudioProcessor::getCueIndexAt (double contentTimeSec) const
{
    const juce::ScopedLock lock (stateLock);
    return cues.indexAt (contentTimeSec);
}

int LyricsSheetAudioProcessor::getFontSize() const
{
    const juce::ScopedLock lock (stateLock);
    return fontSize;
}

void LyricsSheetAudioProcessor::setFontSize (int points)
{
    const juce::ScopedLock lock (stateLock);
    fontSize = juce::jlimit (18, 160, points);
}

int LyricsSheetAudioProcessor::getManualPage() const
{
    const juce::ScopedLock lock (stateLock);
    return manualPage;
}

void LyricsSheetAudioProcessor::setManualPage (int page)
{
    const juce::ScopedLock lock (stateLock);
    manualPage = juce::jmax (1, page);
}

bool LyricsSheetAudioProcessor::getPerformanceMode() const
{
    const juce::ScopedLock lock (stateLock);
    return performanceMode;
}

void LyricsSheetAudioProcessor::setPerformanceMode (bool enabled)
{
    const juce::ScopedLock lock (stateLock);
    performanceMode = enabled;
}

//== State ====================================================================

void LyricsSheetAudioProcessor::notifyStateChange()
{
    sendChangeMessage();
}

void LyricsSheetAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    constexpr juce::int64 maxEmbeddedAssetBytes = 64 * 1024 * 1024;

    juce::ValueTree state ("LYRICS_SHEET");
    state.setProperty ("version", 2, nullptr);

    {
        const juce::ScopedLock lock (stateLock);

        state.setProperty ("lrcB64", juce::Base64::toBase64 (lrcText.toRawUTF8(),
                                                             lrcText.getNumBytesAsUTF8()),
                           nullptr);
        state.setProperty ("lrcSource", lrcSource, nullptr);
        state.setProperty ("origin", originSeconds, nullptr);
        state.setProperty ("fine", fineOffsetSeconds, nullptr);
        state.setProperty ("fontSize", fontSize, nullptr);
        state.setProperty ("manualPage", manualPage, nullptr);
        state.setProperty ("performanceMode", performanceMode, nullptr);

        juce::ValueTree sheets ("SHEETS");

        for (const auto& page : sheetPages)
        {
            juce::ValueTree child ("PAGE");
            child.setProperty ("path", page.path, nullptr);
            child.setProperty ("pdfPage", page.pdfPage, nullptr);
            child.setProperty ("gifFrame", page.gifFrame, nullptr);
            sheets.appendChild (child, nullptr);
        }

        state.appendChild (sheets, nullptr);

        // Embed the sheet files themselves so the project opens on another
        // machine without the original files. ASSETS is keyed by page path.
        juce::ValueTree assets ("ASSETS");
        juce::StringArray embeddedPaths;

        for (const auto& page : sheetPages)
        {
            if (embeddedPaths.contains (page.path))
                continue;

            embeddedPaths.add (page.path);

            const juce::File file (page.path);

            if (! file.existsAsFile()
                || file.getSize() <= 0
                || file.getSize() > maxEmbeddedAssetBytes)
            {
                continue;
            }

            juce::MemoryBlock raw;

            if (! file.loadFileAsData (raw))
                continue;

            juce::MemoryOutputStream compressed;

            {
                juce::GZIPCompressorOutputStream gzip (compressed);
                gzip.write (raw.getData(), raw.getSize());
            }

            juce::ValueTree asset ("ASSET");
            asset.setProperty ("path", page.path, nullptr);
            asset.setProperty ("name", file.getFileName(), nullptr);
            asset.setProperty ("data",
                               juce::Base64::toBase64 (compressed.getData(), compressed.getDataSize()),
                               nullptr);
            assets.appendChild (asset, nullptr);
        }

        state.appendChild (assets, nullptr);

        juce::ValueTree cueList ("CUES");

        for (const auto& cue : cues.cues())
        {
            juce::ValueTree child ("CUE");
            child.setProperty ("t", cue.timeSec, nullptr);
            child.setProperty ("p", cue.page, nullptr);
            cueList.appendChild (child, nullptr);
        }

        state.appendChild (cueList, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LyricsSheetAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName ("LYRICS_SHEET"))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid())
        return;

    juce::String text;
    const auto encoded = state.getProperty ("lrcB64").toString();
    juce::MemoryOutputStream decoded;

    if (encoded.isNotEmpty()
        && juce::Base64::convertFromBase64 (decoded, encoded))
    {
        text = juce::String::fromUTF8 (static_cast<const char*> (decoded.getData()),
                                       static_cast<int> (decoded.getDataSize()));
    }

    auto document = std::make_shared<lyrsheet::LrcDocument>();
    document->parse (text.toStdString());

    // Materialise embedded assets: use the original file when it exists on this
    // machine, otherwise write the embedded copy to a local cache.
    std::map<juce::String, juce::String> remappedAssets;

    if (const auto assets = state.getChildWithName ("ASSETS"); assets.isValid())
    {
        const auto cacheDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                  .getChildFile ("Application Support")
                                  .getChildFile ("LogicLyricsAndSheet")
                                  .getChildFile ("assets");
        cacheDir.createDirectory();

        for (const auto child : assets)
        {
            if (! child.hasType ("ASSET"))
                continue;

            const auto originalPath = child.getProperty ("path").toString();
            const auto encodedData = child.getProperty ("data").toString();

            if (originalPath.isEmpty())
                continue;

            if (juce::File (originalPath).existsAsFile())
            {
                remappedAssets[originalPath] = originalPath;
                continue;
            }

            if (encodedData.isEmpty())
                continue;

            juce::MemoryOutputStream compressed;

            if (! juce::Base64::convertFromBase64 (compressed, encodedData))
                continue;

            juce::MemoryInputStream compressedInput (compressed.getData(), compressed.getDataSize(), false);
            juce::GZIPDecompressorInputStream gzip (compressedInput);
            juce::MemoryOutputStream raw;
            raw.writeFromInputStream (gzip, -1);

            if (raw.getDataSize() == 0)
                continue;

            const auto hash = hashBytes (raw.getData(), raw.getDataSize());
            const auto extension = juce::File (child.getProperty ("name").toString()).getFileExtension();
            const auto cached = cacheDir.getChildFile (hash + extension);

            if (! cached.existsAsFile()
                || cached.getSize() != static_cast<juce::int64> (raw.getDataSize()))
            {
                cached.replaceWithData (raw.getData(), raw.getDataSize());
            }

            if (cached.existsAsFile())
                remappedAssets[originalPath] = cached.getFullPathName();
        }
    }

    std::vector<SheetPageDescriptor> restoredSheets;

    if (const auto sheets = state.getChildWithName ("SHEETS"); sheets.isValid())
    {
        for (const auto child : sheets)
        {
            if (! child.hasType ("PAGE"))
                continue;

            SheetPageDescriptor page;
            page.path = child.getProperty ("path").toString();
            page.pdfPage = static_cast<int> (child.getProperty ("pdfPage"));
            page.gifFrame = static_cast<int> (child.getProperty ("gifFrame"));

            if (const auto remapped = remappedAssets.find (page.path);
                remapped != remappedAssets.end())
            {
                page.path = remapped->second;
            }

            if (page.path.isNotEmpty())
                restoredSheets.push_back (page);
        }
    }
    else if (const auto legacyPath = state.getProperty ("sheetPath").toString();
             legacyPath.isNotEmpty())
    {
        restoredSheets.push_back ({ legacyPath,
                                    legacyPath.endsWithIgnoreCase (".pdf") ? 1 : 0 });
    }

    std::vector<lyrsheet::PageCue> restoredCues;

    if (const auto cueList = state.getChildWithName ("CUES"); cueList.isValid())
    {
        for (const auto child : cueList)
        {
            if (! child.hasType ("CUE"))
                continue;

            restoredCues.push_back ({ static_cast<double> (child.getProperty ("t")),
                                      static_cast<int> (child.getProperty ("p")) });
        }
    }

    {
        const juce::ScopedLock lock (stateLock);

        lrcText = text;
        lrcSource = state.getProperty ("lrcSource").toString();
        sheetPages = std::move (restoredSheets);
        originSeconds = static_cast<double> (state.getProperty ("origin"));
        fineOffsetSeconds = static_cast<double> (state.getProperty ("fine"));
        fontSize = juce::jlimit (18, 160, static_cast<int> (state.getProperty ("fontSize", 56)));
        manualPage = juce::jmax (1, static_cast<int> (state.getProperty ("manualPage", 1)));
        performanceMode = static_cast<bool> (state.getProperty ("performanceMode", false));
        lrcSnapshot = document;
        cues.save (restoredCues);
    }

    resetTransport();
    notifyStateChange();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LyricsSheetAudioProcessor();
}
