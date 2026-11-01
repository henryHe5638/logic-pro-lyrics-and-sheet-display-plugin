#pragma once

#include <string>
#include <vector>

namespace lyrsheet
{

struct LyricLine
{
    double timeSec = 0.0;
    std::string text;
};

/** Parses .lrc text (UTF-8) and answers "which line is active at time t".

    The document stores its own [offset:] tag. Callers can add a user offset on
    top; both are expressed in seconds and shift the lyrics later when positive.
*/
class LrcDocument
{
public:
    void parse (const std::string& utf8Text);
    void clear();

    bool empty() const noexcept { return lines_.empty(); }
    const std::vector<LyricLine>& lines() const noexcept { return lines_; }

    double tagOffsetSec() const noexcept { return tagOffsetSec_; }

    /** Index of the last line whose (time + tag offset) is <= t, or -1. */
    int lineIndexAt (double t) const;

    /** Content time of the first timed line, or 0 when empty. */
    double firstLineSec() const;

private:
    std::vector<LyricLine> lines_;
    double tagOffsetSec_ = 0.0;
};

} // namespace lyrsheet
