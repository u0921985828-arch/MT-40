#pragma once

#include <vector>
#include <array>

// ---------------------------------------------------------------------------
// CasioChord — "Casio Chord" one-finger chord detection (§2, §5, §7 task 3).
//
// In the split chord/bass zone (keys 0-14) the ROOT of the chord is the
// highest (right-most) key held. The chord TYPE is selected by how many
// additional keys are held anywhere to the LEFT of the root:
//
//   additional keys left of root | chord type      | intervals from root
//   ---------------------------- | --------------- | -------------------
//              0                  | Major           | 0, 4, 7
//              1                  | Minor           | 0, 3, 7
//              2                  | Seventh (dom7)  | 0, 4, 7, 10
//              3 (or more)        | Minor Seventh   | 0, 3, 7, 10
//
// The dedicated monophonic bass voice plays the ROOT (Highest-Note priority
// resolves to the root by construction). See docs/CASIO_CHORD_TRUTH_TABLE.md
// for the full derivation.
// ---------------------------------------------------------------------------
class CasioChord
{
public:
    enum class Type { None, Major, Minor, Seventh, MinorSeventh };

    struct Result
    {
        bool valid = false;
        Type type = Type::None;
        int rootNote = -1;              // MIDI note of the chord root
        int bassNote = -1;              // MIDI note the bass voice should play
        std::array<int, 4> tones {};    // chord tone MIDI notes
        int numTones = 0;
    };

    // heldNotesInZone: MIDI note numbers currently held within the chord zone.
    // Returns the detected chord (valid == false if the zone is empty).
    static Result detect (const std::vector<int>& heldNotesInZone);

    static const char* typeName (Type t) noexcept;
};
