#include "GifFrames.h"

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <cmath>
#include <vector>

namespace
{
constexpr double maxPixelSize = 3000.0;

CGImageSourceRef createSource (const juce::File& file)
{
    const auto path = file.getFullPathName();

    CFURLRef url = CFURLCreateFromFileSystemRepresentation (
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*> (path.toRawUTF8()),
        static_cast<CFIndex> (path.getNumBytesAsUTF8()),
        false);

    if (url == nullptr)
        return nullptr;

    auto source = CGImageSourceCreateWithURL (url, nullptr);
    CFRelease (url);
    return source;
}

juce::Image convertToJuceImage (CGImageRef image)
{
    const auto sourceWidth = static_cast<int> (CGImageGetWidth (image));
    const auto sourceHeight = static_cast<int> (CGImageGetHeight (image));

    if (sourceWidth <= 0 || sourceHeight <= 0)
        return {};

    const auto scale = juce::jmin (1.0,
                                   maxPixelSize / static_cast<double> (juce::jmax (sourceWidth, sourceHeight)));
    const auto width = juce::jmax (1, static_cast<int> (std::lround (sourceWidth * scale)));
    const auto height = juce::jmax (1, static_cast<int> (std::lround (sourceHeight * scale)));

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
    CGContextDrawImage (context, CGRectMake (0.0, 0.0, width, height), image);
    CGContextRelease (context);

    juce::Image result (juce::Image::ARGB, width, height, true);
    juce::Image::BitmapData destination (result, juce::Image::BitmapData::writeOnly);

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

    return result;
}
} // namespace

namespace gifframes
{
int getFrameCount (const juce::File& file)
{
    if (! file.existsAsFile())
        return 0;

    auto source = createSource (file);

    if (source == nullptr)
        return 0;

    const auto count = static_cast<int> (CGImageSourceGetCount (source));
    CFRelease (source);
    return count;
}

juce::Image loadFrame (const juce::File& file, int frameIndex)
{
    if (! file.existsAsFile() || frameIndex < 1)
        return {};

    auto source = createSource (file);

    if (source == nullptr)
        return {};

    const auto count = static_cast<int> (CGImageSourceGetCount (source));
    const auto index = juce::jlimit (1, count, frameIndex) - 1;

    auto image = CGImageSourceCreateImageAtIndex (source,
                                                  static_cast<size_t> (index),
                                                  nullptr);
    CFRelease (source);

    if (image == nullptr)
        return {};

    auto result = convertToJuceImage (image);
    CGImageRelease (image);
    return result;
}
} // namespace gifframes
