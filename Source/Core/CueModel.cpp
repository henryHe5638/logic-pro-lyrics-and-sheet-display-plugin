#include "CueModel.h"

#include <algorithm>
#include <cmath>

namespace lyrsheet
{

namespace
{
bool cueLess (const PageCue& a, const PageCue& b)
{
    return a.timeSec < b.timeSec;
}
} // namespace

void CueModel::sort()
{
    std::stable_sort (cues_.begin(), cues_.end(), cueLess);
}

void CueModel::add (double timeSec, int page, double mergeToleranceSec)
{
    page = std::max (1, page);

    for (auto& cue : cues_)
    {
        if (std::abs (cue.timeSec - timeSec) <= mergeToleranceSec)
        {
            cue.page = page;
            sort();
            return;
        }
    }

    cues_.push_back ({ timeSec, page });
    sort();
}

bool CueModel::removeAt (int index)
{
    if (index < 0 || index >= size())
        return false;

    cues_.erase (cues_.begin() + index);
    return true;
}

void CueModel::clear()
{
    cues_.clear();
}

int CueModel::indexAt (double t) const
{
    if (cues_.empty())
        return -1;

    // Find the first cue strictly after t.
    auto it = std::upper_bound (cues_.begin(), cues_.end(), t,
                                [] (double value, const PageCue& cue)
                                {
                                    return value < cue.timeSec;
                                });

    if (it == cues_.begin())
        return -1;

    return static_cast<int> (std::distance (cues_.begin(), it)) - 1;
}

int CueModel::pageAt (double t) const
{
    const auto index = indexAt (t);
    return index < 0 ? -1 : cues_[static_cast<std::size_t> (index)].page;
}

bool CueModel::save (const std::vector<PageCue>& replacement)
{
    cues_.clear();

    for (const auto& cue : replacement)
    {
        if (! std::isfinite (cue.timeSec))
            return false;

        cues_.push_back ({ cue.timeSec, std::max (1, cue.page) });
    }

    sort();
    return true;
}

} // namespace lyrsheet
