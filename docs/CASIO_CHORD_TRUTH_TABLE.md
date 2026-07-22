# Casio Chord — Minor / 7th Interval Detection Truth Table

*Section 7, Task 3 deliverable. Implemented in `Source/dsp/CasioChord.cpp`.*

## 1. Zone & root selection

The 37-key bed is split (§2):

| Key index | Note range | Zone |
| :-- | :-- | :-- |
| `0 – 14`  | C2 – (split) | **Casio Chord / Bass trigger zone** |
| `15 – 36` | (split) – C5 | Melodic zone |

Key index → MIDI note: `note = 36 + keyIndex` (key 0 = C2 = MIDI 36).
Split boundary: `kSplitNote = 36 + 15 = 51`. Notes `< 51` are in the chord zone.

Within the chord zone the **root is the highest (right-most) key held**. The
dedicated monophonic bass voice (§2, Highest-Note priority) always plays this
root — which, being the highest chord-zone key by construction, satisfies the
Highest-Note rule directly.

## 2. Chord-type selection

Casio Chord is a *one-finger* system. The chord **type** is chosen by **how many
additional keys are held to the LEFT of (lower than) the root**:

| Additional keys left of root | Chord type      | Intervals from root (semitones) | # tones |
| :--------------------------: | :-------------- | :------------------------------ | :-----: |
| `0`                          | **Major**       | `0, 4, 7`                       | 3 |
| `1`                          | **Minor**       | `0, 3, 7`                       | 3 |
| `2`                          | **Seventh (dominant 7)** | `0, 4, 7, 10`          | 4 |
| `≥ 3`                        | **Minor Seventh** | `0, 3, 7, 10`                 | 4 |

The distinguishing intervals requested in §5 ("minor / 7th detection"):

* **Minor third** present ⇔ interval `+3` (types Minor, Minor Seventh).
* **Major third** present ⇔ interval `+4` (types Major, Seventh).
* **Dominant seventh** present ⇔ interval `+10` (types Seventh, Minor Seventh).
* **Perfect fifth** (`+7`) is present in every chord type.

## 3. Worked examples (root = A2, MIDI 45)

| Keys held (MIDI)        | Root | # left of root | Type          | Chord tones (MIDI)      |
| :---------------------- | :--: | :------------: | :------------ | :---------------------- |
| `45`                    | 45   | 0              | Major         | `45, 49, 52`            |
| `44, 45`                | 45   | 1              | Minor         | `45, 48, 52`            |
| `43, 44, 45`            | 45   | 2              | Seventh       | `45, 49, 52, 55`        |
| `42, 43, 44, 45`        | 45   | 3              | Minor Seventh | `45, 48, 52, 55`        |

The *identity* of the additional keys does not matter — only their **count** and
that they are lower than the root. This matches the original hardware behaviour
where any keys to the left of the root are counted, not decoded by pitch.

## 4. Boolean decision logic

```
root      = max(heldKeysInZone)
additional = count(k in heldKeysInZone where k < root)

isMinorThird   = (additional == 1) || (additional >= 3)   // +3 instead of +4
hasDominant7th = (additional == 2) || (additional >= 3)   // adds +10

type =  additional == 0 -> Major
        additional == 1 -> Minor
        additional == 2 -> Seventh
        additional >= 3 -> MinorSeventh
```

Empty chord zone ⇒ no chord (`valid == false`), bass root cleared to `-1`.
