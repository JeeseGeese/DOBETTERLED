/**
 * PaletteManager.cpp
 * -----------------------------------------------------------------------
 * See PaletteManager.h for the contract. All palette data lives here as
 * plain RgbColor stop arrays -- no FastLED gradient types involved.
 * -----------------------------------------------------------------------
 */

#include "PaletteManager.h"

namespace
{
    constexpr uint8_t kStopsPerPalette = 16;

    struct PaletteDefinition
    {
        RgbColor stops[kStopsPerPalette];
        const char* name;
    };

    // Each palette is 16 evenly-spaced color stops across the 0-255
    // position range; getColorAt() linearly interpolates between the
    // two nearest stops for any requested position.
    const PaletteDefinition kPalettes[static_cast<uint8_t>(PaletteId::Count)] =
    {
        // Rainbow
        {{
            {255,0,0}, {255,64,0}, {255,128,0}, {255,192,0},
            {255,255,0}, {128,255,0}, {0,255,0}, {0,255,128},
            {0,255,255}, {0,128,255}, {0,0,255}, {64,0,255},
            {128,0,255}, {192,0,255}, {255,0,255}, {255,0,128},
        }, "Rainbow"},

        // Ocean
        {{
            {0,0,64}, {0,20,90}, {0,40,120}, {0,70,150},
            {0,100,170}, {0,130,190}, {0,160,200}, {20,180,210},
            {40,200,220}, {60,210,225}, {90,220,230}, {120,230,235},
            {150,240,240}, {180,245,245}, {210,250,250}, {255,255,255},
        }, "Ocean"},

        // Forest
        {{
            {10,30,10}, {15,45,12}, {20,60,15}, {30,80,20},
            {40,100,25}, {55,120,30}, {70,140,35}, {90,150,40},
            {110,120,40}, {130,100,35}, {120,80,30}, {100,65,25},
            {80,55,20}, {60,45,18}, {40,35,15}, {20,25,10},
        }, "Forest"},

        // Lava
        {{
            {0,0,0}, {20,0,0}, {50,0,0}, {90,0,0},
            {130,10,0}, {160,20,0}, {190,40,0}, {210,60,0},
            {230,90,0}, {240,120,0}, {250,150,0}, {255,180,0},
            {255,210,20}, {255,230,60}, {255,245,120}, {255,255,220},
        }, "Lava"},

        // Cloud
        {{
            {200,210,220}, {210,218,226}, {220,225,232}, {228,232,238},
            {235,238,242}, {240,243,246}, {245,247,250}, {250,251,253},
            {255,255,255}, {250,251,253}, {245,247,250}, {235,238,242},
            {220,225,232}, {205,215,225}, {190,205,220}, {180,198,215},
        }, "Cloud"},

        // Sunset
        {{
            {20,10,40}, {45,15,60}, {80,20,80}, {120,25,90},
            {160,35,90}, {200,50,80}, {230,70,70}, {250,100,60},
            {255,130,50}, {255,160,60}, {255,190,80}, {255,210,110},
            {255,225,140}, {255,200,170}, {230,150,180}, {180,100,160},
        }, "Sunset"},

        // Ice
        {{
            {220,240,255}, {200,230,255}, {180,220,255}, {160,215,255},
            {140,210,255}, {120,205,255}, {100,200,255}, {90,210,255},
            {110,225,255}, {140,235,255}, {170,245,255}, {200,250,255},
            {220,252,255}, {235,253,255}, {245,254,255}, {255,255,255},
        }, "Ice"},

        // Neon
        {{
            {255,0,128}, {255,0,200}, {220,0,255}, {160,0,255},
            {100,0,255}, {40,40,255}, {0,120,255}, {0,200,255},
            {0,255,220}, {0,255,140}, {0,255,60}, {80,255,0},
            {180,255,0}, {255,220,0}, {255,140,0}, {255,60,60},
        }, "Neon"},

        // Pastel
        {{
            {255,214,214}, {255,224,204}, {255,235,204}, {245,245,214},
            {224,245,214}, {204,245,224}, {204,240,240}, {204,224,240},
            {214,214,245}, {224,204,240}, {240,204,235}, {245,204,224},
            {250,214,214}, {255,224,214}, {255,235,224}, {255,214,214},
        }, "Pastel"},

        // Halloween
        {{
            {0,0,0}, {20,0,20}, {40,0,40}, {60,0,50},
            {90,10,40}, {130,20,20}, {170,60,0}, {210,100,0},
            {255,140,0}, {255,110,0}, {210,70,0}, {150,40,10},
            {90,20,30}, {50,0,50}, {20,0,30}, {0,0,0},
        }, "Halloween"},

        // Christmas
        {{
            {200,0,0}, {220,0,0}, {255,0,0}, {220,20,20},
            {180,60,40}, {120,120,40}, {60,160,40}, {0,180,40},
            {0,150,30}, {0,170,50}, {60,190,80}, {150,210,150},
            {255,255,255}, {220,230,220}, {120,180,120}, {0,150,30},
        }, "Christmas"},
    };

    inline const PaletteDefinition& definitionFor(PaletteId id)
    {
        uint8_t index = static_cast<uint8_t>(id);
        if (index >= static_cast<uint8_t>(PaletteId::Count))
        {
            index = 0; // defensive fallback to Rainbow on an invalid ID
        }
        return kPalettes[index];
    }
}

RgbColor PaletteManager::blend(const RgbColor& a, const RgbColor& b, uint8_t ratio)
{
    const uint16_t inv = 255 - static_cast<uint16_t>(ratio);
    const uint16_t r = static_cast<uint16_t>(a.r) * inv + static_cast<uint16_t>(b.r) * ratio;
    const uint16_t g = static_cast<uint16_t>(a.g) * inv + static_cast<uint16_t>(b.g) * ratio;
    const uint16_t bl = static_cast<uint16_t>(a.b) * inv + static_cast<uint16_t>(b.b) * ratio;

    return RgbColor(
        static_cast<uint8_t>(r / 255),
        static_cast<uint8_t>(g / 255),
        static_cast<uint8_t>(bl / 255));
}

RgbColor PaletteManager::getColorAt(PaletteId id, uint8_t position) const
{
    const PaletteDefinition& def = definitionFor(id);

    // Map position (0-255) onto a fractional stop index (0.0 - 15.0).
    constexpr float kSegment = 255.0f / static_cast<float>(kStopsPerPalette - 1);
    const float scaledPos = static_cast<float>(position) / kSegment;

    uint8_t lowerIndex = static_cast<uint8_t>(scaledPos);
    if (lowerIndex >= kStopsPerPalette - 1)
    {
        return def.stops[kStopsPerPalette - 1];
    }

    const float frac = scaledPos - static_cast<float>(lowerIndex);
    const uint8_t ratio = static_cast<uint8_t>(frac * 255.0f);

    return blend(def.stops[lowerIndex], def.stops[lowerIndex + 1], ratio);
}

PaletteId PaletteManager::getNextPaletteId(PaletteId current) const
{
    uint8_t next = static_cast<uint8_t>(current) + 1;
    if (next >= static_cast<uint8_t>(PaletteId::Count))
    {
        next = 0;
    }
    return static_cast<PaletteId>(next);
}

const char* PaletteManager::getPaletteName(PaletteId id) const
{
    return definitionFor(id).name;
}
