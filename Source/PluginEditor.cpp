#include "PluginEditor.h"
#include "GifFrames.h"
#include "PdfRenderer.h"

#include <algorithm>
#include <cmath>

namespace
{
const juce::Colour background { 0xff12141a };
const juce::Colour panel { 0xff1a1d26 };
const juce::Colour panelLight { 0xff232734 };
const juce::Colour textMain { 0xffe9ecf3 };
const juce::Colour textDim { 0xff8b93a7 };
const juce::Colour accent { 0xff5aa9ff };

/** JUCE's String(const char*) treats its input as Latin-1, so UTF-8 literals
    have to be converted explicitly. */
juce::String t8 (const char* text)
{
    return juce::String::fromUTF8 (text);
}

juce::String formatTime (double seconds)
{
    const bool negative = seconds < 0.0;
    const auto totalMs = static_cast<juce::int64> (std::llround (std::abs (seconds) * 1000.0));

    const auto minutes = totalMs / 60000;
    const auto secondsPart = (totalMs / 1000) % 60;
    const auto millis = totalMs % 1000;

    return juce::String (negative ? "-" : "")
           + juce::String (minutes).paddedLeft ('0', 2) + ":"
           + juce::String (secondsPart).paddedLeft ('0', 2) + "."
           + juce::String (millis).paddedLeft ('0', 3);
}

juce::Font makeFont (float height, int style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions ("PingFang SC", height, style));
}

juce::Font makeMonoFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                          height,
                                          juce::Font::plain));
}
} // namespace

//==============================================================================
LyricsView::LyricsView()
{
    setOpaque (true);
}

void LyricsView::setDocument (std::shared_ptr<const lyrsheet::LrcDocument> document)
{
    if (doc == document)
        return;

    doc = std::move (document);
    rebuildLayout();
    updateScrollTarget();
    scrollY = targetScrollY;
    repaint();
}

void LyricsView::setFontSize (int points)
{
    if (fontSize == points)
        return;

    fontSize = points;
    rebuildLayout();
    updateScrollTarget();
    scrollY = targetScrollY;
    repaint();
}

void LyricsView::resized()
{
    rebuildLayout();
    updateScrollTarget();
    repaint();
}

void LyricsView::rebuildLayout()
{
    laidOutWidth = getWidth();
    laidOutFontSize = fontSize;

    dimLayouts.clear();
    activeLayouts.clear();
    tops.clear();
    heights.clear();
    contentHeight = 0.0f;

    if (doc == nullptr || laidOutWidth <= 0)
        return;

    constexpr float margin = 26.0f;
    const auto textWidth = juce::jmax (80.0f, static_cast<float> (laidOutWidth) - margin * 2.0f);
    const auto font = makeFont (static_cast<float> (fontSize));
    const auto gap = static_cast<float> (fontSize) * 0.6f;

    float y = 20.0f;

    for (const auto& line : doc->lines())
    {
        auto text = juce::String::fromUTF8 (line.text.c_str(), static_cast<int> (line.text.size()));

        if (text.isEmpty())
            text = " ";

        juce::AttributedString dimString;
        dimString.setJustification (juce::Justification::centred);
        dimString.append (text, font, textDim);

        juce::AttributedString activeString;
        activeString.setJustification (juce::Justification::centred);
        activeString.append (text, font, textMain);

        auto dimLayout = std::make_unique<juce::TextLayout>();
        dimLayout->createLayout (dimString, textWidth);

        auto activeLayout = std::make_unique<juce::TextLayout>();
        activeLayout->createLayout (activeString, textWidth);

        const auto height = juce::jmax (dimLayout->getHeight(),
                                        static_cast<float> (fontSize) * 1.3f);

        tops.push_back (y);
        heights.push_back (height);
        dimLayouts.push_back (std::move (dimLayout));
        activeLayouts.push_back (std::move (activeLayout));

        y += height + gap;
    }

    contentHeight = y + 20.0f;
}

float LyricsView::resolveTargetFor (int index) const
{
    if (index < 0 || index >= static_cast<int> (tops.size()) || getHeight() <= 0)
        return 0.0f;

    const auto centred = tops[static_cast<size_t> (index)]
                         + heights[static_cast<size_t> (index)] * 0.5f
                         - static_cast<float> (getHeight()) * 0.38f;

    return juce::jlimit (0.0f,
                         juce::jmax (0.0f, contentHeight - static_cast<float> (getHeight())),
                         centred);
}

void LyricsView::updateScrollTarget()
{
    targetScrollY = resolveTargetFor (activeIndex);
}

void LyricsView::setActiveIndex (int index, bool jumpToLine)
{
    const bool changed = index != activeIndex;
    activeIndex = index;
    updateScrollTarget();

    if (jumpToLine)
        scrollY = targetScrollY;

    if (changed || jumpToLine)
        repaint();
}

void LyricsView::animate()
{
    if (laidOutWidth != getWidth() || laidOutFontSize != fontSize)
    {
        rebuildLayout();
        updateScrollTarget();
    }

    const auto difference = targetScrollY - scrollY;

    if (std::abs (difference) < 0.4f)
    {
        if (difference != 0.0f)
        {
            scrollY = targetScrollY;
            repaint();
        }

        return;
    }

    scrollY += difference * 0.22f;
    repaint();
}

void LyricsView::paint (juce::Graphics& g)
{
    g.fillAll (background);

    if (dimLayouts.empty())
    {
        g.setColour (textDim);
        g.setFont (makeFont (16.0f));
        g.drawFittedText (t8 ("打开 .lrc 歌词文件后，内容会跟随宿主播放位置滚动"),
                          getLocalBounds().reduced (24),
                          juce::Justification::centred, 2);
        return;
    }

    constexpr float margin = 26.0f;
    const auto width = static_cast<float> (getWidth()) - margin * 2.0f;

    for (int i = 0; i < static_cast<int> (dimLayouts.size()); ++i)
    {
        const auto y = tops[static_cast<size_t> (i)] - scrollY;
        const auto height = heights[static_cast<size_t> (i)];

        if (y + height < 0.0f || y > static_cast<float> (getHeight()))
            continue;

        if (i == activeIndex)
        {
            auto highlight = juce::Rectangle<float> (margin - 14.0f,
                                                     y - 5.0f,
                                                     width + 28.0f,
                                                     height + 10.0f);

            g.setColour (accent.withAlpha (0.10f));
            g.fillRoundedRectangle (highlight, 8.0f);

            g.setColour (accent.withAlpha (0.8f));
            g.fillRoundedRectangle (highlight.withWidth (3.0f), 1.5f);
        }

        auto& layout = (i == activeIndex && i < static_cast<int> (activeLayouts.size()))
                           ? activeLayouts[static_cast<size_t> (i)]
                           : dimLayouts[static_cast<size_t> (i)];

        if (layout != nullptr)
            layout->draw (g, { margin, y, width, height });
    }
}

//==============================================================================
SheetView::SheetView()
{
    message = t8 ("打开 PDF、图片或 GIF 乐谱（可多选）");
}

SheetView::~SheetView() = default;

void SheetView::clear()
{
    sources.clear();
    pdfCache.clear();
    pageImage = {};
    pageCount = 0;
    page = 1;
    message = t8 ("打开 PDF、图片或 GIF 乐谱（可多选）");
    repaint();
}

void SheetView::showMessage (const juce::String& text)
{
    clear();
    message = text;
    repaint();
}

void SheetView::load (const std::vector<SheetPageSource>& newSources)
{
    clear();

    sources = newSources;
    pageCount = static_cast<int> (sources.size());
    page = 1;

    if (pageCount > 0)
    {
        message.clear();
        renderPage();
    }

    repaint();
}

void SheetView::setPage (int newPage)
{
    const auto clamped = juce::jlimit (1, juce::jmax (1, pageCount), newPage);

    if (clamped == page && pageImage.isValid())
        return;

    page = clamped;

    if (pageCount > 0)
        renderPage();

    repaint();
}

void SheetView::renderPage()
{
    pageImage = {};

    if (page < 1 || page > static_cast<int> (sources.size()))
        return;

    const auto& source = sources[static_cast<size_t> (page - 1)];

    if (source.gifFrame > 0)
    {
        pageImage = gifframes::loadFrame (source.file, source.gifFrame);
    }
    else if (source.pdfPage > 0)
    {
        const auto path = source.file.getFullPathName();
        auto found = pdfCache.find (path);

        if (found == pdfCache.end())
            found = pdfCache.emplace (path, PdfDocument::open (source.file)).first;

        if (found->second != nullptr)
            pageImage = found->second->renderPage (source.pdfPage - 1, 2000);
    }
    else
    {
        pageImage = juce::ImageFileFormat::loadFrom (source.file);
    }

    if (pageImage.isValid())
        message.clear();
    else
        message = t8 ("无法打开: ") + source.file.getFileName();
}

void SheetView::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    if (! pageImage.isValid())
    {
        g.setColour (textDim);
        g.setFont (makeFont (16.0f));
        g.drawFittedText (message, getLocalBounds().reduced (24),
                          juce::Justification::centred, 3);
        return;
    }

    const auto area = getLocalBounds().toFloat().reduced (10.0f);
    const auto scale = juce::jmin (area.getWidth() / static_cast<float> (pageImage.getWidth()),
                                   area.getHeight() / static_cast<float> (pageImage.getHeight()));
    const auto width = static_cast<float> (pageImage.getWidth()) * scale;
    const auto height = static_cast<float> (pageImage.getHeight()) * scale;

    const juce::Rectangle<float> pageArea (area.getX() + (area.getWidth() - width) * 0.5f,
                                           area.getY() + (area.getHeight() - height) * 0.5f,
                                           width,
                                           height);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (pageArea.translated (0.0f, 4.0f), 4.0f);

    // drawImage multiplies by the current colour's alpha, so reset it first.
    g.setColour (juce::Colours::white);
    g.drawImage (pageImage, pageArea);

    if (pageCount > 1)
    {
        g.setColour (panel.withAlpha (0.82f));
        g.fillRoundedRectangle (pageArea.getRight() - 74.0f, pageArea.getBottom() - 30.0f, 66.0f, 22.0f, 6.0f);

        g.setColour (textMain);
        g.setFont (makeMonoFont (13.0f));
        g.drawText (juce::String (page) + " / " + juce::String (pageCount),
                    juce::Rectangle<float> (pageArea.getRight() - 74.0f,
                                            pageArea.getBottom() - 30.0f,
                                            66.0f, 22.0f),
                    juce::Justification::centred, false);
    }
}

//==============================================================================
CueListComponent::CueListComponent()
{
    deleteButton.setButtonText (t8 ("删除选中"));
    clearButton.setButtonText (t8 ("清空"));

    list.setModel (this);
    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, panel);
    list.setColour (juce::ListBox::outlineColourId, panelLight);
    list.setOutlineThickness (1);

    deleteButton.onClick = [this]
    {
        const auto row = list.getSelectedRow();

        if (row >= 0 && onDelete != nullptr)
            onDelete (row);
    };

    clearButton.onClick = [this]
    {
        if (onClear != nullptr)
            onClear();
    };

    for (auto* button : { &deleteButton, &clearButton })
    {
        button->setColour (juce::TextButton::buttonColourId, panelLight);
        button->setColour (juce::TextButton::textColourOffId, textMain);
    }

    addAndMakeVisible (list);
    addAndMakeVisible (deleteButton);
    addAndMakeVisible (clearButton);
}

void CueListComponent::setCues (const std::vector<lyrsheet::PageCue>& newCues)
{
    cues = newCues;
    list.updateContent();
    list.repaint();
}

void CueListComponent::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto top = area.removeFromTop (30);
    deleteButton.setBounds (top.removeFromLeft (96));
    top.removeFromLeft (8);
    clearButton.setBounds (top.removeFromLeft (72));
    area.removeFromTop (8);
    list.setBounds (area);
}

int CueListComponent::getNumRows()
{
    return static_cast<int> (cues.size());
}

void CueListComponent::paintListBoxItem (int row, juce::Graphics& g,
                                         int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (cues.size()))
        return;

    g.fillAll (selected ? accent.withAlpha (0.18f)
                        : (row % 2 == 0 ? panel : panel.darker (0.12f)));

    const auto& cue = cues[static_cast<size_t> (row)];
    const auto half = width / 2;

    g.setFont (makeMonoFont (13.0f));
    g.setColour (textMain);
    g.drawText (formatTime (cue.timeSec), 10, 0, half - 10, height,
                juce::Justification::centredLeft, false);

    g.setColour (accent);
    g.drawText (t8 ("第 ") + juce::String (cue.page) + t8 (" 页"),
                half, 0, half - 10, height,
                juce::Justification::centredRight, false);
}

void CueListComponent::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < static_cast<int> (cues.size()) && onPreview != nullptr)
        onPreview (cues[static_cast<size_t> (row)].timeSec);
}

//==============================================================================
LyricsSheetAudioProcessorEditor::LyricsSheetAudioProcessorEditor (LyricsSheetAudioProcessor& p)
    : AudioProcessorEditor (&p), plugin (p)
{
    setOpaque (true);

    titleLabel.setText (t8 ("歌词 & 乐谱"), juce::dontSendNotification);
    titleLabel.setFont (makeFont (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, textMain);

    sourceLabel.setFont (makeFont (12.0f));
    sourceLabel.setColour (juce::Label::textColourId, textDim);
    sourceLabel.setMinimumHorizontalScale (1.0f);

    for (auto* caption : { &fontCaption, &offsetCaption, &pageCaption })
    {
        caption->setFont (makeFont (12.0f));
        caption->setColour (juce::Label::textColourId, textDim);
    }

    fontCaption.setText (t8 ("字号"), juce::dontSendNotification);
    offsetCaption.setText (t8 ("偏移"), juce::dontSendNotification);
    pageCaption.setText (t8 ("页码"), juce::dontSendNotification);

    statusLabel.setFont (makeMonoFont (11.5f));
    statusLabel.setColour (juce::Label::textColourId, textDim);
    statusLabel.setMinimumHorizontalScale (1.0f);

    loadLrcButton.setButtonText (t8 ("打开 LRC"));
    loadSheetButton.setButtonText (t8 ("打开乐谱"));
    appendSheetButton.setButtonText (t8 ("追加"));
    captureButton.setButtonText (t8 ("设为起点"));
    recordCueButton.setButtonText (t8 ("记翻页点"));
    prevButton.setButtonText (t8 ("上一页"));
    nextButton.setButtonText (t8 ("下一页"));
    performanceButton.setButtonText (t8 ("演奏模式"));

    for (auto* button : { &loadLrcButton, &loadSheetButton, &appendSheetButton, &captureButton,
                          &recordCueButton, &prevButton, &nextButton, &performanceButton })
    {
        button->setColour (juce::TextButton::buttonColourId, panelLight);
        button->setColour (juce::TextButton::textColourOffId, textMain);
        button->setColour (juce::TextButton::textColourOnId, textMain);
    }

    perfLyricsButton.setButtonText (t8 ("歌词"));
    perfSheetButton.setButtonText (t8 ("乐谱"));
    perfPrevButton.setButtonText (t8 ("‹"));
    perfNextButton.setButtonText (t8 ("›"));
    perfExitButton.setButtonText (t8 ("退出"));

    for (auto* button : { &perfLyricsButton, &perfSheetButton, &perfPrevButton,
                          &perfNextButton, &perfExitButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.45f));
        button->setColour (juce::TextButton::textColourOffId, textMain);
        button->setColour (juce::TextButton::textColourOnId, textMain);
        button->setAlpha (0.9f);
        button->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        button->setVisible (false);
    }

    recordCueButton.setColour (juce::TextButton::buttonColourId, accent.withAlpha (0.75f));
    captureButton.setColour (juce::TextButton::buttonColourId, panelLight.brighter (0.08f));

    fontSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    fontSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 22);
    fontSlider.setRange (24.0, 110.0, 1.0);
    fontSlider.setValue (plugin.getFontSize(), juce::dontSendNotification);

    offsetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 78, 22);
    offsetSlider.setRange (-5.0, 5.0, 0.001);
    offsetSlider.setNumDecimalPlacesToDisplay (3);
    offsetSlider.setTextValueSuffix (" s");
    offsetSlider.setValue (plugin.getFineOffsetSeconds(), juce::dontSendNotification);

    pageSlider.setSliderStyle (juce::Slider::IncDecButtons);
    pageSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 22);
    pageSlider.setRange (1.0, 1.0, 1.0);
    pageSlider.setValue (1.0, juce::dontSendNotification);

    for (auto* slider : { &fontSlider, &offsetSlider, &pageSlider })
    {
        slider->setColour (juce::Slider::thumbColourId, accent);
        slider->setColour (juce::Slider::trackColourId, accent.withAlpha (0.55f));
        slider->setColour (juce::Slider::textBoxTextColourId, textMain);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, panelLight);
        slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    tabs.setColour (juce::TabbedComponent::backgroundColourId, background);
    tabs.addTab (t8 ("歌词"), panel, &lyricsView, false);
    tabs.addTab (t8 ("乐谱"), panel, &sheetView, false);
    tabs.addTab (t8 ("翻页点"), panel, &cueList, false);
    tabs.getTabbedButtonBar().setColour (juce::TabbedButtonBar::tabTextColourId, textDim);
    tabs.getTabbedButtonBar().setColour (juce::TabbedButtonBar::frontTextColourId, textMain);

    for (auto* child : std::initializer_list<juce::Component*> {
             &titleLabel, &sourceLabel, &fontCaption, &offsetCaption, &pageCaption,
             &statusLabel, &loadLrcButton, &loadSheetButton, &appendSheetButton, &captureButton,
             &recordCueButton, &prevButton, &nextButton, &performanceButton, &fontSlider,
             &offsetSlider, &pageSlider, &tabs,
             &perfLyricsButton, &perfSheetButton, &perfPrevButton, &perfNextButton, &perfExitButton })
    {
        addAndMakeVisible (child);
    }

    loadLrcButton.onClick = [this] { loadLrcFile(); };
    loadSheetButton.onClick = [this] { loadSheetFile (false); };
    appendSheetButton.onClick = [this] { loadSheetFile (true); };
    captureButton.onClick = [this] { captureOrigin(); };
    recordCueButton.onClick = [this] { recordCue(); };
    prevButton.onClick = [this] { setPageFromUser (sheetView.getPage() - 1); };
    nextButton.onClick = [this] { setPageFromUser (sheetView.getPage() + 1); };

    performanceButton.onClick = [this] { setPerformanceMode (true); };
    perfExitButton.onClick = [this] { setPerformanceMode (false); };

    perfLyricsButton.onClick = [this]
    {
        tabs.setCurrentTabIndex (0);
        lastDisplayTab = 0;
        updatePerformanceOverlay();
    };

    perfSheetButton.onClick = [this]
    {
        tabs.setCurrentTabIndex (1);
        lastDisplayTab = 1;
        updatePerformanceOverlay();
    };

    perfPrevButton.onClick = [this] { setPageFromUser (sheetView.getPage() - 1); };
    perfNextButton.onClick = [this] { setPageFromUser (sheetView.getPage() + 1); };

    fontSlider.onValueChange = [this]
    {
        plugin.setFontSize (static_cast<int> (fontSlider.getValue()));
        lyricsView.setFontSize (static_cast<int> (fontSlider.getValue()));
    };

    offsetSlider.onValueChange = [this]
    {
        plugin.setFineOffsetSeconds (offsetSlider.getValue());
    };

    pageSlider.onValueChange = [this]
    {
        setPageFromUser (static_cast<int> (pageSlider.getValue()));
    };

    cueList.onPreview = [this] (double time)
    {
        previewContentTime = time;
        lastDisplayTime.reset();
        setStatus (t8 ("预览 ") + formatTime (time));
    };

    cueList.onDelete = [this] (int index)
    {
        plugin.removeCue (index);
    };

    cueList.onClear = [this]
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::WarningIcon)
                                          .withTitle (t8 ("清空翻页点"))
                                          .withMessage (t8 ("确定删除全部翻页点吗？"))
                                          .withButton (t8 ("清空"))
                                          .withButton (t8 ("取消")),
                                      [this] (int result)
                                      {
                                          if (result == 1)
                                              plugin.clearCues();
                                      });
    };

    plugin.addChangeListener (this);

    setResizable (true, true);
    setResizeLimits (760, 540, 4096, 3200);
    setSize (980, 700);

    refreshAll();
    setPerformanceMode (plugin.getPerformanceMode(), true);
    startTimerHz (60);
}

LyricsSheetAudioProcessorEditor::~LyricsSheetAudioProcessorEditor()
{
    stopTimer();
    plugin.removeChangeListener (this);
}

void LyricsSheetAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    if (performanceMode)
    {
        g.setColour (panel);
        g.fillRect (0, 0, getWidth(), 42);
        g.setColour (panelLight);
        g.fillRect (0, 41, getWidth(), 1);
    }
}

void LyricsSheetAudioProcessorEditor::resized()
{
    if (performanceMode)
    {
        tabs.setBounds (getLocalBounds().withTrimmedTop (42));

        auto bar = juce::Rectangle<int> (juce::jmax (0, getWidth() - 364), 6, 356, 30);
        perfLyricsButton.setBounds (bar.removeFromLeft (68));
        bar.removeFromLeft (8);
        perfSheetButton.setBounds (bar.removeFromLeft (68));
        bar.removeFromLeft (14);

        if (perfPrevButton.isVisible())
        {
            perfPrevButton.setBounds (bar.removeFromLeft (54));
            bar.removeFromLeft (8);
            perfNextButton.setBounds (bar.removeFromLeft (54));
            bar.removeFromLeft (14);
        }

        perfExitButton.setBounds (bar.removeFromLeft (64));

        for (auto* button : { &perfLyricsButton, &perfSheetButton, &perfPrevButton,
                              &perfNextButton, &perfExitButton })
        {
            button->toFront (false);
        }

        return;
    }

    auto area = getLocalBounds().reduced (14);

    auto titleRow = area.removeFromTop (30);
    titleLabel.setBounds (titleRow.removeFromLeft (120));
    appendSheetButton.setBounds (titleRow.removeFromRight (72));
    titleRow.removeFromRight (8);
    loadSheetButton.setBounds (titleRow.removeFromRight (104));
    titleRow.removeFromRight (8);
    loadLrcButton.setBounds (titleRow.removeFromRight (104));
    titleRow.removeFromRight (12);
    sourceLabel.setBounds (titleRow);

    area.removeFromTop (6);

    auto row2 = area.removeFromTop (30);
    fontCaption.setBounds (row2.removeFromLeft (42));
    fontSlider.setBounds (row2.removeFromLeft (170));
    row2.removeFromLeft (14);
    captureButton.setBounds (row2.removeFromLeft (98));
    row2.removeFromLeft (14);
    offsetCaption.setBounds (row2.removeFromLeft (42));
    offsetSlider.setBounds (row2.removeFromLeft (208));

    area.removeFromTop (6);

    auto row3 = area.removeFromTop (30);
    performanceButton.setBounds (row3.removeFromRight (104));
    pageCaption.setBounds (row3.removeFromLeft (42));
    pageSlider.setBounds (row3.removeFromLeft (92));
    row3.removeFromLeft (10);
    recordCueButton.setBounds (row3.removeFromLeft (98));
    row3.removeFromLeft (8);
    prevButton.setBounds (row3.removeFromLeft (76));
    row3.removeFromLeft (6);
    nextButton.setBounds (row3.removeFromLeft (76));

    area.removeFromTop (8);
    statusLabel.setBounds (area.removeFromBottom (20));
    area.removeFromBottom (6);
    tabs.setBounds (area);
}

void LyricsSheetAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll();
}

void LyricsSheetAudioProcessorEditor::refreshAll()
{
    const auto size = plugin.getFontSize();
    fontSlider.setValue (size, juce::dontSendNotification);
    lyricsView.setFontSize (size);
    lyricsView.setDocument (plugin.getLrcSnapshot());

    offsetSlider.setValue (plugin.getFineOffsetSeconds(), juce::dontSendNotification);

    const auto pages = plugin.getSheetPages();

    juce::String sheetKey;

    for (const auto& page : pages)
        sheetKey << page.path << "#" << page.pdfPage << "#" << page.gifFrame << "|";

    if (sheetKey != loadedSheetKey)
    {
        loadedSheetKey = sheetKey;

        if (pages.empty())
        {
            sheetView.clear();
        }
        else
        {
            std::vector<SheetPageSource> sources;
            sources.reserve (pages.size());

            for (const auto& page : pages)
                sources.push_back ({ juce::File (page.path), page.pdfPage, page.gifFrame });

            sheetView.load (sources);
            sheetView.setPage (plugin.getManualPage());
        }

        updatePageRange();
    }

    const auto lrcName = plugin.getLrcSourceName();
    sourceLabel.setText (lrcName.isNotEmpty() ? juce::String ("LRC: ") + lrcName
                                              : t8 ("尚未加载歌词"),
                         juce::dontSendNotification);

    updatePageRange();
    refreshCues();

    if (plugin.getPerformanceMode() != performanceMode)
        setPerformanceMode (plugin.getPerformanceMode(), true);
}

void LyricsSheetAudioProcessorEditor::refreshCues()
{
    cueList.setCues (plugin.getCues());
    lastCueIndex = -1000;
}

void LyricsSheetAudioProcessorEditor::updatePageRange()
{
    const auto pages = juce::jmax (1, sheetView.getPageCount());
    const bool canTurn = pages > 1;

    pageSlider.setRange (1.0, static_cast<double> (pages), 1.0);
    pageSlider.setValue (sheetView.getPage(), juce::dontSendNotification);
    pageSlider.setEnabled (canTurn);

    prevButton.setEnabled (canTurn);
    nextButton.setEnabled (canTurn);

    updatePerformanceOverlay();
}

void LyricsSheetAudioProcessorEditor::setPerformanceMode (bool enabled, bool force)
{
    if (! force && performanceMode == enabled)
        return;

    performanceMode = enabled;
    plugin.setPerformanceMode (enabled);

    if (enabled)
    {
        if (tabs.getCurrentTabIndex() < 2)
            lastDisplayTab = tabs.getCurrentTabIndex();
        else
            tabs.setCurrentTabIndex (lastDisplayTab);
    }

    for (auto* child : std::initializer_list<juce::Component*> {
             &titleLabel, &sourceLabel, &fontCaption, &offsetCaption,
             &pageCaption, &statusLabel, &loadLrcButton, &loadSheetButton,
             &appendSheetButton, &captureButton, &recordCueButton,
             &prevButton, &nextButton, &performanceButton, &fontSlider,
             &offsetSlider, &pageSlider })
    {
        child->setVisible (! enabled);
    }

    tabs.setTabBarDepth (enabled ? 0 : 30);
    tabs.getTabbedButtonBar().setVisible (! enabled);

    updatePerformanceOverlay();

    if (! enabled)
        resized();
}

void LyricsSheetAudioProcessorEditor::updatePerformanceOverlay()
{
    const bool visible = performanceMode;

    perfLyricsButton.setVisible (visible);
    perfSheetButton.setVisible (visible);
    perfExitButton.setVisible (visible);

    const auto inactive = juce::Colours::black.withAlpha (0.45f);
    const bool lyricsActive = tabs.getCurrentTabIndex() == 0;

    perfLyricsButton.setColour (juce::TextButton::buttonColourId,
                                lyricsActive ? accent.withAlpha (0.8f) : inactive);
    perfSheetButton.setColour (juce::TextButton::buttonColourId,
                               lyricsActive ? inactive : accent.withAlpha (0.8f));

    const bool pager = visible
                       && tabs.getCurrentTabIndex() == 1
                       && sheetView.getPageCount() > 1;

    perfPrevButton.setVisible (pager);
    perfNextButton.setVisible (pager);

    if (visible)
        resized();
}

void LyricsSheetAudioProcessorEditor::loadLrcFile()
{
    chooser = std::make_unique<juce::FileChooser> (t8 ("选择 LRC 歌词文件"),
                                                   juce::File(),
                                                   "*.lrc;*.txt");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& result)
                          {
                              const auto file = result.getResult();

                              if (file.existsAsFile())
                                  applyLrcFile (file);
                          });
}

void LyricsSheetAudioProcessorEditor::loadSheetFile (bool append)
{
    chooser = std::make_unique<juce::FileChooser> (t8 ("选择乐谱文件（可多选）"),
                                                   juce::File(),
                                                   "*.pdf;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.bmp");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this, append] (const juce::FileChooser& result)
                          {
                              const auto files = result.getResults();

                              if (! files.isEmpty())
                                  applySheetFiles (files, append);
                          });
}

void LyricsSheetAudioProcessorEditor::applyLrcFile (const juce::File& file)
{
    const auto text = file.loadFileAsString();

    if (text.isEmpty())
    {
        setStatus (t8 ("文件为空或无法读取: ") + file.getFileName());
        return;
    }

    plugin.setLrcText (text, file.getFileName());
    previewContentTime.reset();
    lastDisplayTime.reset();
    setStatus (t8 ("已加载歌词 ") + file.getFileName());
}

void LyricsSheetAudioProcessorEditor::applySheetFiles (const juce::Array<juce::File>& files, bool append)
{
    std::vector<juce::File> ordered (files.begin(), files.end());

    std::sort (ordered.begin(), ordered.end(),
               [] (const juce::File& a, const juce::File& b)
               {
                   return a.getFileName().compareNatural (b.getFileName()) < 0;
               });

    std::vector<SheetPageDescriptor> pages;

    for (const auto& file : ordered)
    {
        if (! file.existsAsFile())
            continue;

        if (file.hasFileExtension ("pdf"))
        {
            const auto document = PdfDocument::open (file);
            const auto count = document != nullptr ? document->getPageCount() : 0;

            for (int page = 1; page <= count; ++page)
                pages.push_back ({ file.getFullPathName(), page, 0 });
        }
        else if (file.hasFileExtension ("gif"))
        {
            // A GIF is a page container here: every frame becomes one page.
            const auto frameCount = juce::jlimit (1, 500, gifframes::getFrameCount (file));

            for (int frame = 1; frame <= frameCount; ++frame)
                pages.push_back ({ file.getFullPathName(), 0, frame });
        }
        else
        {
            pages.push_back ({ file.getFullPathName(), 0, 0 });
        }
    }

    if (pages.empty())
    {
        setStatus (t8 ("没有可用的乐谱文件"));
        return;
    }

    if (append)
        plugin.appendSheetPages (pages);
    else
        plugin.setSheetPages (pages);

    previewContentTime.reset();
    lastDisplayTime.reset();
    setStatus ((append ? t8 ("已追加 ") : t8 ("已加载 "))
               + juce::String (static_cast<int> ((append ? plugin.getSheetPages().size() : pages.size())))
               + t8 (" 页乐谱"));
}

void LyricsSheetAudioProcessorEditor::setPageFromUser (int page)
{
    const auto clamped = juce::jlimit (1, juce::jmax (1, sheetView.getPageCount()), page);
    sheetView.setPage (clamped);
    plugin.setManualPage (clamped);
    pageSlider.setValue (clamped, juce::dontSendNotification);
}

void LyricsSheetAudioProcessorEditor::recordCue()
{
    const auto snapshot = plugin.getTransportSnapshot();

    if (! snapshot.valid)
    {
        setStatus (t8 ("还没有收到宿主位置，先播放一下"));
        return;
    }

    previewContentTime.reset();
    lastDisplayTime.reset();

    const auto contentTime = currentContentSeconds();
    const auto page = juce::jmax (1, static_cast<int> (pageSlider.getValue()));

    plugin.addCue (contentTime, page);
    setStatus (t8 ("已记录 ") + formatTime (contentTime)
               + t8 (" → 第 ") + juce::String (page) + t8 (" 页"));
}

void LyricsSheetAudioProcessorEditor::captureOrigin()
{
    const auto snapshot = plugin.getTransportSnapshot();
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const bool live = snapshot.valid && snapshot.playing
                      && (now - snapshot.lastRenderMs) < 300.0;

    if (live)
    {
        const auto projectTime = currentProjectSeconds();
        plugin.setOriginSeconds (projectTime - plugin.getFineOffsetSeconds());

        previewContentTime.reset();
        lastDisplayTime.reset();

        setStatus (t8 ("起点已设为工程 ") + formatTime (projectTime));
        return;
    }

    // While Logic is stopped the plugin receives no callbacks, so a parked
    // playhead cannot be read. Capture on the first callback after playback
    // starts instead.
    originCaptureArmed = true;
    armedLastRenderMs = snapshot.lastRenderMs;
    setStatus (t8 ("已预备：按下播放后，在第一个位置回调处记录起点"));
}

void LyricsSheetAudioProcessorEditor::setStatus (const juce::String& text)
{
    transientStatus = text;
    transientUntilMs = juce::Time::getMillisecondCounterHiRes() + 3500.0;
    statusLabel.setText (text, juce::dontSendNotification);
}

double LyricsSheetAudioProcessorEditor::currentProjectSeconds() const
{
    const auto snapshot = plugin.getTransportSnapshot();
    const auto now = juce::Time::getMillisecondCounterHiRes();

    auto project = snapshot.projectSeconds;

    if (snapshot.valid && snapshot.playing && now - snapshot.lastRenderMs < 300.0)
        project += juce::jlimit (0.0, 0.25, (now - snapshot.lastRenderMs) / 1000.0);

    return project;
}

double LyricsSheetAudioProcessorEditor::currentContentSeconds() const
{
    return plugin.toContentTime (currentProjectSeconds());
}

juce::String LyricsSheetAudioProcessorEditor::formatSeconds (double seconds) const
{
    return formatTime (seconds);
}

void LyricsSheetAudioProcessorEditor::timerCallback()
{
    auto snapshot = plugin.getTransportSnapshot();
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const bool hostRecent = snapshot.valid && (now - snapshot.lastRenderMs) < 400.0;

    if (! hostRecent && now - lastHostPollMs > 80.0)
    {
        lastHostPollMs = now;

        if (plugin.pollTransportFromMessageThread())
            snapshot = plugin.getTransportSnapshot();
    }

    if (originCaptureArmed && snapshot.lastRenderMs > armedLastRenderMs)
    {
        originCaptureArmed = false;
        plugin.setOriginSeconds (snapshot.projectSeconds - plugin.getFineOffsetSeconds());
        previewContentTime.reset();
        lastDisplayTime.reset();
        setStatus (t8 ("起点已设为工程 ") + formatTime (snapshot.projectSeconds));
    }

    const bool live = snapshot.valid && snapshot.playing
                      && (now - snapshot.lastRenderMs) < 300.0;

    if (live)
        previewContentTime.reset();

    auto project = snapshot.projectSeconds;

    if (live)
        project += juce::jlimit (0.0, 0.25, (now - snapshot.lastRenderMs) / 1000.0);

    const auto displayTime = previewContentTime.has_value()
                                 ? *previewContentTime
                                 : plugin.toContentTime (project);

    bool jumped = false;

    if (lastDisplayTime.has_value())
        jumped = std::abs (displayTime - *lastDisplayTime) > 0.75;

    lastDisplayTime = displayTime;

    if (auto doc = plugin.getLrcSnapshot())
        lyricsView.setActiveIndex (doc->lineIndexAt (displayTime), jumped);
    else
        lyricsView.setActiveIndex (-1, false);

    lyricsView.animate();

    const auto cueIndex = plugin.getCueIndexAt (displayTime);

    if (cueIndex != lastCueIndex || jumped)
    {
        lastCueIndex = cueIndex;
        const auto page = plugin.getPageAt (displayTime);

        if (page > 0)
        {
            sheetView.setPage (page);
            pageSlider.setValue (page, juce::dontSendNotification);
            plugin.setManualPage (page);
        }
    }

    if (now < transientUntilMs)
        return;

    juce::String state = ! snapshot.valid ? t8 ("等待宿主")
                         : live ? t8 ("播放")
                                : t8 ("停止");

    auto text = state
                + t8 ("   工程 ") + formatTime (project)
                + t8 ("   内容 ") + formatTime (displayTime)
                + t8 ("   第 ") + juce::String (sheetView.getPage())
                + "/" + juce::String (juce::jmax (1, sheetView.getPageCount())) + t8 (" 页")
                + t8 ("   翻页点 ") + juce::String (static_cast<int> (plugin.getCues().size()))
                + t8 ("   偏移 ")
                + juce::String (juce::roundToInt (plugin.getFineOffsetSeconds() * 1000.0))
                + t8 (" ms");

    if (originCaptureArmed)
        text += t8 ("   起点待录制");

    statusLabel.setText (text, juce::dontSendNotification);
}
