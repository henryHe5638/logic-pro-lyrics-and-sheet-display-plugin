#include "LrcDocument.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace lyrsheet
{

namespace
{

std::string trim (const std::string& s)
{
    const auto first = s.find_first_not_of (" \t\r\n");
    if (first == std::string::npos)
        return {};

    const auto last = s.find_last_not_of (" \t\r\n");
    return s.substr (first, last - first + 1);
}

bool isNumber (const std::string& s)
{
    if (s.empty())
        return false;

    std::size_t i = 0;

    if (s[0] == '+' || s[0] == '-')
        i = 1;

    bool digitSeen = false;

    for (; i < s.size(); ++i)
    {
        if (std::isdigit (static_cast<unsigned char> (s[i])) != 0)
            digitSeen = true;
        else if (s[i] != '.')
            return false;
    }

    return digitSeen;
}

/** Accepts "01:23.45", "01:23.456", "01:23:45" and "1:23". */
bool parseTimestamp (const std::string& raw, double& outSec)
{
    const auto colon = raw.find (':');

    if (colon == std::string::npos || colon == 0)
        return false;

    const auto minutesText = trim (raw.substr (0, colon));
    const auto rest = raw.substr (colon + 1);

    if (! isNumber (minutesText))
        return false;

    const auto separator = rest.find_first_of (".:");

    const auto secondsText = trim (rest.substr (0, separator));
    const auto fractionText = separator == std::string::npos
                                ? std::string()
                                : trim (rest.substr (separator + 1));

    if (! isNumber (secondsText) || (! fractionText.empty() && ! isNumber (fractionText)))
        return false;

    const double minutes = std::stod (minutesText);
    const double seconds = std::stod (secondsText);

    double fraction = 0.0;

    if (! fractionText.empty())
    {
        const double value = std::stod (fractionText);
        fraction = value / std::pow (10.0, static_cast<double> (fractionText.size()));
    }

    outSec = minutes * 60.0 + seconds + fraction;
    return true;
}

std::string stripInlineTags (const std::string& text)
{
    // Enhanced LRC word timings look like <01:23.45>word.
    std::string out;
    out.reserve (text.size());

    bool insideTag = false;

    for (const char c : text)
    {
        if (c == '<')
            insideTag = true;
        else if (c == '>')
            insideTag = false;
        else if (! insideTag)
            out.push_back (c);
    }

    return out;
}

} // namespace

void LrcDocument::clear()
{
    lines_.clear();
    tagOffsetSec_ = 0.0;
}

void LrcDocument::parse (const std::string& utf8Text)
{
    clear();

    std::string text = utf8Text;

    if (text.rfind ("\xEF\xBB\xBF", 0) == 0)
        text.erase (0, 3);

    std::vector<LyricLine> parsed;
    std::size_t start = 0;

    while (start <= text.size())
    {
        const auto end = text.find ('\n', start);
        std::string line = text.substr (start, end == std::string::npos ? std::string::npos : end - start);

        if (! line.empty() && line.back() == '\r')
            line.pop_back();

        start = end == std::string::npos ? text.size() + 1 : end + 1;

        std::vector<double> times;

        std::size_t pos = 0;

        while (pos < line.size() && line[pos] == '[')
        {
            const auto close = line.find (']', pos + 1);

            if (close == std::string::npos)
                break;

            const auto tag = trim (line.substr (pos + 1, close - pos - 1));

            double timestamp = 0.0;

            if (parseTimestamp (tag, timestamp))
            {
                times.push_back (timestamp);
            }
            else
            {
                const auto lowerStart = tag.find_first_of ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

                if (lowerStart != std::string::npos)
                {
                    auto key = tag.substr (lowerStart, tag.find (':', lowerStart) == std::string::npos
                                                    ? std::string::npos
                                                    : tag.find (':', lowerStart) - lowerStart);

                    if (key == "offset")
                    {
                        const auto valuePos = tag.find (':');

                        if (valuePos != std::string::npos)
                        {
                            const auto value = trim (tag.substr (valuePos + 1));

                            if (isNumber (value))
                                tagOffsetSec_ = std::stod (value) / 1000.0;
                        }
                    }
                }
            }

            pos = close + 1;
        }

        if (times.empty())
            continue;

        auto lyricText = trim (stripInlineTags (line.substr (pos)));

        for (const double time : times)
            parsed.push_back ({ time, lyricText });
    }

    std::stable_sort (parsed.begin(), parsed.end(),
                      [] (const LyricLine& a, const LyricLine& b)
                      {
                          return a.timeSec < b.timeSec;
                      });

    lines_ = std::move (parsed);
}

int LrcDocument::lineIndexAt (double t) const
{
    if (lines_.empty())
        return -1;

    const double shifted = t - tagOffsetSec_;

    auto it = std::upper_bound (lines_.begin(), lines_.end(), shifted,
                                [] (double value, const LyricLine& line)
                                {
                                    return value < line.timeSec;
                                });

    if (it == lines_.begin())
        return -1;

    return static_cast<int> (std::distance (lines_.begin(), it)) - 1;
}

double LrcDocument::firstLineSec() const
{
    if (lines_.empty())
        return 0.0;

    return lines_.front().timeSec + tagOffsetSec_;
}

} // namespace lyrsheet
