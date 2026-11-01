#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

/** Minimal PDF page renderer backed by Core Graphics.

    Pages are rasterised once into a juce::Image; the plugin never needs a
    native view. Only page rendering happens here, layout stays in JUCE.
*/
class PdfDocument
{
public:
    static std::unique_ptr<PdfDocument> open (const juce::File& file);

    ~PdfDocument();

    int getPageCount() const noexcept { return pageCount; }

    /** Renders a 0-based page at roughly the requested pixel width. */
    juce::Image renderPage (int pageIndex, int targetPixelWidth) const;

private:
    PdfDocument() = default;

    void* documentRef = nullptr;
    int pageCount = 0;
};
