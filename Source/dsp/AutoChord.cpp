#include "AutoChord.h"
#include <algorithm>

AutoChord::Result AutoChord::detect (const std::vector<int>& heldNotesInZone)
{
    Result r;
    if (heldNotesInZone.empty())
        return r;

    // Root = highest (right-most) key in the chord zone.
    const int root = *std::max_element (heldNotesInZone.begin(), heldNotesInZone.end());

    // Count additional keys strictly to the LEFT of (lower than) the root.
    int additional = 0;
    for (int n : heldNotesInZone)
        if (n < root)
            ++additional;

    r.valid = true;
    r.rootNote = root;
    r.bassNote = root; // dedicated bass voice tracks the root (highest priority)

    switch (additional)
    {
        case 0:  r.type = Type::Major;        break;
        case 1:  r.type = Type::Minor;        break;
        case 2:  r.type = Type::Seventh;      break;
        default: r.type = Type::MinorSeventh; break; // 3 or more
    }

    // Realise chord tones from the truth table.
    switch (r.type)
    {
        case Type::Major:        r.tones = { root, root + 4, root + 7, 0 };       r.numTones = 3; break;
        case Type::Minor:        r.tones = { root, root + 3, root + 7, 0 };       r.numTones = 3; break;
        case Type::Seventh:      r.tones = { root, root + 4, root + 7, root + 10 }; r.numTones = 4; break;
        case Type::MinorSeventh: r.tones = { root, root + 3, root + 7, root + 10 }; r.numTones = 4; break;
        default: break;
    }

    return r;
}

const char* AutoChord::typeName (Type t) noexcept
{
    switch (t)
    {
        case Type::Major:        return "Major";
        case Type::Minor:        return "Minor";
        case Type::Seventh:      return "Seventh";
        case Type::MinorSeventh: return "Minor Seventh";
        default:                 return "None";
    }
}
