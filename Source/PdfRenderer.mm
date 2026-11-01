#include "PdfRenderer.h"

#include <CoreGraphics/CoreGraphics.h>
#include <cmath>
#include <vector>

namespace
{
int clampedWidth (int requested)
{
    return juce::jlimit (400, 4096, requested);
}
} // namespace

PdfDocument::~PdfDocument()
{
    if (documentRef != nullptr)
        CGPDFDocumentRelease (static_cast<CGPDFDocumentRef> (documentRef));
}

std::unique_ptr<PdfDocument> PdfDocument::open (const juce::File& file)
{
    if (! file.existsAsFile())
        return {};

    const auto path = file.getFullPathName();

    CFURLRef url = CFURLCreateFromFileSystemRepresentation (
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*> (path.toRawUTF8()),
        static_cast<CFIndex> (path.getNumBytesAsUTF8()),
        false);

    if (url == nullptr)
        return {};

    auto document = CGPDFDocumentCreateWithURL (url);
    CFRelease (url);

    if (document == nullptr)
        return {};

    auto result = std::unique_ptr<PdfDocument> (new PdfDocument());
    result->documentRef = document;
    result->pageCount = static_cast<int> (CGPDFDocumentGetNumberOfPages (document));
    return result;
}

juce::Image PdfDocument::renderPage (int pageIndex, int targetPixelWidth) const
{
    if (documentRef == nullptr || pageCount <= 0)
        return {};

    const auto pageNumber = static_cast<size_t> (juce::jlimit (0, pageCount - 1, pageIndex) + 1);
    auto page = CGPDFDocumentGetPage (static_cast<CGPDFDocumentRef> (documentRef), pageNumber);

    if (page == nullptr)
        return {};

    auto box = CGPDFPageGetBoxRect (page, kCGPDFCropBox);

    if (box.size.width <= 1.0 || box.size.height <= 1.0)
        box = CGPDFPageGetBoxRect (page, kCGPDFMediaBox);

    if (box.size.width <= 1.0 || box.size.height <= 1.0)
        return {};

    const int width = clampedWidth (targetPixelWidth);
    const int height = juce::jlimit (200, 8192,
                                     static_cast<int> (std::lround (width * box.size.height / box.size.width)));

    std::vector<uint8_t> pixels (static_cast<size_t> (width) * static_cast<size_t> (height) * 4);

    auto colourSpace = CGColorSpaceCreateDeviceRGB();

    auto context = CGBitmapContextCreate (pixels.data(),
                                          static_cast<size_t> (width),
                                          static_cast<size_t> (height),
                                          8,
                                          static_cast<size_t> (width) * 4,
                                          colourSpace,
                                          kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease (colourSpace);

    if (context == nullptr)
        return {};

    CGContextSetRGBFillColor (context, 1.0, 1.0, 1.0, 1.0);
    CGContextFillRect (context, CGRectMake (0.0, 0.0, width, height));
    CGContextSetInterpolationQuality (context, kCGInterpolationHigh);
    CGContextSetShouldAntialias (context, true);

    const auto scale = static_cast<CGFloat> (width) / box.size.width;

    CGContextSaveGState (context);
    CGContextScaleCTM (context, scale, scale);

    // CGPDFPageGetDrawingTransform never enlarges a page, so the target rect is
    // the crop box itself and the scale above brings it up to the bitmap size.
    const auto transform = CGPDFPageGetDrawingTransform (page,
                                                         kCGPDFCropBox,
                                                         box,
                                                         0,
                                                         true);

    CGContextConcatCTM (context, transform);
    CGContextDrawPDFPage (context, page);
    CGContextRestoreGState (context);
    CGContextRelease (context);

    juce::Image image (juce::Image::ARGB, width, height, true);
    juce::Image::BitmapData destination (image, juce::Image::BitmapData::writeOnly);

    // Core Graphics stores rows top-down in a bitmap context's memory, matching
    // JUCE, so the pixels can be copied straight across.
    for (int y = 0; y < height; ++y)
    {
        const auto* sourceRow = pixels.data()
                                 + static_cast<size_t> (y) * static_cast<size_t> (width) * 4;
        auto* destinationRow = reinterpret_cast<juce::uint32*> (destination.getLinePointer (y));

        for (int x = 0; x < width; ++x)
        {
            const auto r = static_cast<juce::uint32> (sourceRow[x * 4 + 0]);
            const auto g = static_cast<juce::uint32> (sourceRow[x * 4 + 1]);
            const auto b = static_cast<juce::uint32> (sourceRow[x * 4 + 2]);
            const auto a = static_cast<juce::uint32> (sourceRow[x * 4 + 3]);

            destinationRow[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    return image;
}
