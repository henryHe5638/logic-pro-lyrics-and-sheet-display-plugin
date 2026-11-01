#pragma once

#include <vector>

namespace lyrsheet
{

/** A page-turn cue, expressed in content seconds (the LRC timeline). */
struct PageCue
{
    double timeSec = 0.0;
    int page = 1;
};

/** Keeps page cues sorted and resolves "which page should be visible now".

    Resolution is a binary search rather than a cursor, so seeks and loop
    jumps land on the correct page without any replay of missed events.
*/
class CueModel
{
public:
    void add (double timeSec, int page, double mergeToleranceSec = 0.05);
    bool removeAt (int index);
    void clear();

    int size() const noexcept { return static_cast<int> (cues_.size()); }
    const PageCue& at (int index) const { return cues_[static_cast<std::size_t> (index)]; }
    const std::vector<PageCue>& cues() const noexcept { return cues_; }

    /** Last cue at or before t, or -1 when t precedes every cue. */
    int pageAt (double t) const;
    int indexAt (double t) const;

    bool save (const std::vector<PageCue>& replacement);
    void sort();

private:
    std::vector<PageCue> cues_;
};

} // namespace lyrsheet
