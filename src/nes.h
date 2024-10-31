#ifndef NES_H
#define NES_H

#include "constants.h"
#include "vec3.h"

// https://forums.nesdev.org/viewtopic.php?p=296732#p296732
// https://github.com/Gumball2415/palgen-persune/blob/main/palgen_persune.py

class nesppusimulation{
public:
    int verbosity = 0;
    bool palmode = false;
    bool docolorburstampcorrection = true;
    double phaseskew26A = 4.5; // degrees
    double lumaboost48C = 1.0; // IRE
    double phaseskewperlumastep = -2.5; // degrees
    double idealizedYUVtoRGBMatrix[3][3];

    // verboselevel: verbostiy level
    // ispal: simulate PAL's alternating phases?
    // cbcorrection: Normalize chroma to NES's non-standard colorburst amplitude?
    // skew26A: Phase skew for hues 0x2, 0x6, and 0xA due to trace design. Degrees. Sane value is ~4.5
    // boost48C: Luma boost for hues 0x4, 0x8 and 0xC due to trace design. IRE. Sane value is 1.0
    // skewstep: Phase skew per luma level. Degrees. Sane value depends on which chip we're simulating:
    //      2C02E: ~-2.5 degrees per luma step
    //      2C02G: ~-5 degrees per luma step
    //      2C07: ~10 dgrees per luma step (but PAL so it cancels out)
    bool Initialize(int verboselevel, bool ispal, bool cbcorrection, double skew26A, double boost48C, double skewstep);
    bool InitializeYUVtoRGBMatrix();
    int pal_phase(int hue);
    bool in_color_phase(int hue, int phase, bool pal);
    double encode_composite(int emphasis, int luma, int hue, int wave_phase, bool backwards);
    vec3 NEStoYUV(int hue, int luma, int emphasis);
    vec3 NEStoRGB(int hue, int luma, int emphasis);

};

#endif
