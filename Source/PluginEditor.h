#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "PluginProcessor.h"

class PdfDocument;

/** One displayed sheet page: a PDF page, an image, or a GIF frame. */
struct SheetPageSource
{
    juce::File file;
    int pdfPage = 0;  // 1-based page in a PDF
    int gifFrame = 0; // 1-based frame in a GIF
};

/** Vertically scrolling lyric view with a highlighted current line. */
class LyricsView final : public juce::Component
{
public:
    LyricsView();

    void setDocument (std::shared_ptr<const lyrsheet::LrcDocument> document);
    void setFontSize (int points);
    void setActiveIndex (int index, bool jumpToLine);
    void animate();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuildLayout();
    void updateScrollTarget();
    float resolveTargetFor (int index) const;

    std::shared_ptr<const lyrsheet::LrcDocument> doc;
    std::vector<std::unique_ptr<juce::TextLayout>> dimLayouts;
    std::vector<std::unique_ptr<juce::TextLayout>> activeLayouts;
    std::vector<float> tops;
    std::vector<float> heights;
    float contentHeight = 0.0f;
    float scrollY = 0.0f;
    float targetScrollY = 0.0f;
    int fontSize = 56;
    int activeIndex = -1;
    int laidOutWidth = -1;
    int laidOutFontSize = -1;
};

/** Renders the current page of a multi-file sheet, fitted to the component. */
class SheetView final : public juce::Component
{
public:
    SheetView();
    ~SheetView() override;

    void load (const std::vector<SheetPageSource>& newSources);
    void clear();
    void showMessage (const juce::String& text);

    int getPageCount() const noexcept { return pageCount; }
    int getPage() const noexcept { return page; }

    void setPage (int newPage);
    void paint (juce::Graphics&) override;

private:
    void renderPage();

    std::vector<SheetPageSource> sources;
    std::map<juce::String, std::unique_ptr<PdfDocument>> pdfCache;
    juce::Image pageImage;
    juce::String message;
    int pageCount = 0;
    int page = 1;
};

/** Lists recorded page cues and forwards preview / delete actions. */
class CueListComponent final : public juce::Component,
                               private juce::ListBoxModel
{
public:
    CueListComponent();

    void setCues (const std::vector<lyrsheet::PageCue>& newCues);

    std::function<void (double)> onPreview;
    std::function<void (int)> onDelete;
    std::function<void()> onClear;

    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    juce::ListBox list { "cues", nullptr };
    juce::TextButton deleteButton;
    juce::TextButton clearButton;
    std::vector<lyrsheet::PageCue> cues;
};

class LyricsSheetAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer,
                                              private juce::ChangeListener
{
public:
    explicit LyricsSheetAudioProcessorEditor (LyricsSheetAudioProcessor&);
    ~LyricsSheetAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void refreshAll();
    void refreshCues();
    void updatePageRange();
    void setPerformanceMode (bool enabled, bool force = false);
    void updatePerformanceOverlay();
    void loadLrcFile();
    void loadSheetFile (bool append);
    void applyLrcFile (const juce::File&);
    void applySheetFiles (const juce::Array<juce::File>& files, bool append);
    void setPageFromUser (int page);
    void recordCue();
    void captureOrigin();
    void setStatus (const juce::String& text);
    double currentProjectSeconds() const;
    double currentContentSeconds() const;
    juce::String formatSeconds (double seconds) const;

    LyricsSheetAudioProcessor& plugin;

    juce::Label titleLabel;
    juce::Label sourceLabel;
    juce::Label fontCaption;
    juce::Label offsetCaption;
    juce::Label pageCaption;
    juce::Label statusLabel;

    juce::TextButton loadLrcButton;
    juce::TextButton loadSheetButton;
    juce::TextButton appendSheetButton;
    juce::TextButton captureButton;
    juce::TextButton recordCueButton;
    juce::TextButton prevButton;
    juce::TextButton nextButton;
    juce::TextButton performanceButton;

    juce::TextButton perfLyricsButton;
    juce::TextButton perfSheetButton;
    juce::TextButton perfPrevButton;
    juce::TextButton perfNextButton;
    juce::TextButton perfExitButton;

    juce::Slider fontSlider;
    juce::Slider offsetSlider;
    juce::Slider pageSlider;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    LyricsView lyricsView;
    SheetView sheetView;
    CueListComponent cueList;

    std::unique_ptr<juce::FileChooser> chooser;
    std::optional<double> previewContentTime;
    std::optional<double> lastDisplayTime;
    int lastCueIndex = -1000;
    juce::String loadedSheetKey;
    double lastHostPollMs = 0.0;
    bool originCaptureArmed = false;
    double armedLastRenderMs = 0.0;
    bool performanceMode = false;
    int lastDisplayTab = 0;
    juce::String transientStatus;
    double transientUntilMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LyricsSheetAudioProcessorEditor)
};
