#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>

#include "Core/CueModel.h"
#include "Core/LrcDocument.h"

struct SheetPageDescriptor
{
    juce::String path;
    int pdfPage = 0;  // 1-based page in a PDF
    int gifFrame = 0; // 1-based frame in a GIF
};

class LyricsSheetAudioProcessor final : public juce::AudioProcessor,
                                        public juce::ChangeBroadcaster
{
public:
    struct TransportSnapshot
    {
        bool valid = false;
        bool playing = false;
        double projectSeconds = 0.0;
        double ppq = 0.0;
        double bpm = 0.0;
        double sampleRate = 48000.0;
        double lastRenderMs = 0.0;
    };

    LyricsSheetAudioProcessor();
    ~LyricsSheetAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Lyrics & Sheet"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //== Project content ======================================================

    void setLrcText (const juce::String& text, const juce::String& sourceName = {});
    juce::String getLrcText() const;
    juce::String getLrcSourceName() const;
    std::shared_ptr<const lyrsheet::LrcDocument> getLrcSnapshot() const;

    void setSheetPages (const std::vector<SheetPageDescriptor>& pages);
    void appendSheetPages (const std::vector<SheetPageDescriptor>& pages);
    std::vector<SheetPageDescriptor> getSheetPages() const;

    //== Calibration ==========================================================

    /** Project seconds at which content time zero sits (coarse calibration). */
    double getOriginSeconds() const;
    void setOriginSeconds (double seconds);

    /** Extra trim in seconds; positive shows lyrics later (fine calibration). */
    double getFineOffsetSeconds() const;
    void setFineOffsetSeconds (double seconds);

    double toContentTime (double projectSeconds) const;

    //== Page cues ============================================================

    std::vector<lyrsheet::PageCue> getCues() const;
    bool addCue (double contentTimeSec, int page);
    bool removeCue (int index);
    void clearCues();
    int getPageAt (double contentTimeSec) const;
    int getCueIndexAt (double contentTimeSec) const;

    //== View settings ========================================================

    int getFontSize() const;
    void setFontSize (int points);

    int getManualPage() const;
    void setManualPage (int page);

    bool getPerformanceMode() const;
    void setPerformanceMode (bool enabled);

    TransportSnapshot getTransportSnapshot() const;

    /** Reads the host playhead from the message thread.

        Logic stops calling processBlock as soon as the transport stops, so a
        parked playhead would otherwise never be seen. Safe to call when the
        host exposes no transport callbacks (returns false).
    */
    bool pollTransportFromMessageThread();

private:
    void resetTransport();
    void notifyStateChange();
    void applyPosition (const juce::AudioPlayHead::PositionInfo& position);

    std::atomic<bool> hostValid { false };
    std::atomic<bool> hostPlaying { false };
    std::atomic<double> projectSeconds { 0.0 };
    std::atomic<double> ppqPosition { 0.0 };
    std::atomic<double> tempo { 120.0 };
    std::atomic<double> lastRenderMs { 0.0 };

    mutable juce::CriticalSection stateLock;
    std::shared_ptr<const lyrsheet::LrcDocument> lrcSnapshot;
    juce::String lrcText;
    juce::String lrcSource;
    std::vector<SheetPageDescriptor> sheetPages;
    double originSeconds = 0.0;
    double fineOffsetSeconds = 0.0;
    int fontSize = 56;
    int manualPage = 1;
    bool performanceMode = false;
    lyrsheet::CueModel cues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LyricsSheetAudioProcessor)
};
