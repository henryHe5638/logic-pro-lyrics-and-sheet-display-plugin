#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Frame access for GIF files (and any other multi-frame format ImageIO
    exposes). Frames are 1-based to match the sheet page descriptors.
*/
namespace gifframes
{
int getFrameCount (const juce::File& file);
juce::Image loadFrame (const juce::File& file, int frameIndex);
} // namespace gifframes
