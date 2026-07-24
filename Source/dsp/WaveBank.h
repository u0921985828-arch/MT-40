#pragma once

#include <array>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// WaveBank — additive, band-limited wavetables for the melodic "vowel" body
// of the ST-40 consonant-vowel engine (§3.1).
//
// Each harmonic family (organ, reed, brass, string, flute, plucked, mallet,
// ...) is baked once into a set of mip tables, each capped at a rising
// harmonic count. Playback picks the finest table whose highest partial still
// sits below ~17 kHz, so high notes never alias. Tables are shared read-only
// across every voice and built a single time on the first call to instance()
// (which the voices force from prepare(), i.e. off the audio thread).
// ---------------------------------------------------------------------------
class WaveBank
{
public:
    static constexpr int NT   = 1024;   // single-cycle table length (power of two)
    static constexpr int NMIP = 5;
    static constexpr int MIPS[NMIP] = { 2, 4, 8, 16, 32 }; // harmonic caps

    struct Tables { std::array<std::vector<float>, NMIP> mip; };

    static const WaveBank& instance()
    {
        static const WaveBank bank;
        return bank;
    }

    // Resolve a family's tables by name (falls back to the first family).
    const Tables* family (const std::string& name) const
    {
        auto it = fams.find (name);
        return (it != fams.end()) ? &it->second : &fams.begin()->second;
    }

    // Finest mip index whose top partial stays under ~17 kHz for frequency f.
    static int pickMip (double f) noexcept
    {
        const double lim = 17000.0 / (f > 1.0 ? f : 1.0);
        int j = 0;
        for (int m = 0; m < NMIP; ++m) if (MIPS[m] <= lim) j = m;
        return j;
    }

    // Linear-interpolated single-cycle read at normalised phase [0,1).
    static inline float read (const std::vector<float>& tab, double ph) noexcept
    {
        const double x  = ph * NT;
        const int    i  = static_cast<int> (x);
        const float  fr = static_cast<float> (x - i);
        const float  a  = tab[i & (NT - 1)];
        const float  b  = tab[(i + 1) & (NT - 1)];
        return a + (b - a) * fr;
    }

private:
    WaveBank()
    {
        using Fn = std::function<double(int)>;
        // Table-driven family: amplitude of harmonic k (1-based).
        auto tbl = [] (std::vector<double> a) {
            return [a] (int k) -> double { return (k - 1) < (int) a.size() ? a[k - 1] : 0.0; };
        };
        std::map<std::string, Fn> FAM;
        FAM["organ"]    = tbl ({ 1, 0.5, 0.7, 0.35, 0, 0.2, 0, 0.12 });
        FAM["pipe"]     = tbl ({ 1, 0.7, 0.5, 0.4, 0.28, 0.2, 0.14, 0.1, 0.07, 0.05, 0.04, 0.03 });
        FAM["clarinet"] = [] (int k) { return (k % 2 == 1) ? 1.0 / k : 0.0; };
        FAM["reed"]     = [] (int k) { return (k % 2 == 1) ? 1.0 / k : 0.4 / k; };
        FAM["brass"]    = [] (int k) { return (1.0 / k) * std::pow (0.93, k - 1); };
        FAM["string"]   = [] (int k) { return 1.0 / std::pow ((double) k, 0.9); };
        FAM["flute"]    = tbl ({ 1, 0.22, 0.05, 0.02 });
        FAM["pluck"]    = [] (int k) { return 1.0 / k; };
        FAM["harpsi"]   = [] (int k) { return (1.0 / std::sqrt ((double) k)) * std::pow (0.96, k - 1); };
        FAM["mallet"]   = tbl ({ 1, 0.15, 0.6, 0.1, 0.35, 0, 0.2 });
        FAM["epiano"]   = tbl ({ 1, 0.3, 0.12, 0.5, 0.05, 0, 0.18 });
        FAM["fuzz"]     = [] (int k) { return (1.0 / std::sqrt ((double) k)) * std::pow (0.97, k - 1); };

        for (auto& kv : FAM) fams.emplace (kv.first, build (kv.second));
    }

    static Tables build (const std::function<double(int)>& hf)
    {
        Tables t;
        std::vector<double> acc (NT, 0.0);
        const int maxH = MIPS[NMIP - 1];
        for (int k = 1; k <= maxH; ++k)
        {
            const double a = hf (k);
            if (a != 0.0)
            {
                const double wk = 2.0 * M_PI * k / NT;
                for (int i = 0; i < NT; ++i) acc[i] += a * std::sin (wk * i);
            }
            // Snapshot at each cap boundary: a mip with cap C is the partial
            // sum of the first C harmonics.
            for (int m = 0; m < NMIP; ++m)
                if (MIPS[m] == k)
                {
                    std::vector<float> tab (NT);
                    double peak = 1.0e-9;
                    for (int i = 0; i < NT; ++i) peak = std::max (peak, std::fabs (acc[i]));
                    for (int i = 0; i < NT; ++i) tab[i] = static_cast<float> (acc[i] / peak);
                    t.mip[m] = std::move (tab);
                }
        }
        return t;
    }

    std::map<std::string, Tables> fams;
};
