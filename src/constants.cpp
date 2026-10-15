#include "constants.h"
#include "vec3.h"

// populate the D65 whitepoint
const vec3 D65 = vec3(0.312713, 0.329016, 0.358271);

// TMS9118 chip
// Supposedly indistinguishable from TMS9918 chip used in MSX1 computers
// see:
// https://github.com/ChthonVII/gamutthingy/issues/5
// first value is Y on 0 to 1 scale
// second value is IQ chroma amplitude (peak-to-center) on same scale as Y
// third value is chroma phase in degrees
// Sync is -0.4 (theorectically perfect)
// Burst is 0.2 at 180 degress (also theorectically perfect)
const vec3 TMS9188YchValues[16] = {
    {0, 0, 0},
    {0, 0, 0},
    {.53, .267, 237},
    {.67, .200, 235},
    {.40, .300, 354},
    {.53, .267, 354},
    {.47, .233, 114},
    {.67, .300, 295},
    {.53, .300, 114},
    {.67, .300, 114},
    {.73, .233, 173},
    {.80, .167, 173},
    {.46, .233, 235},
    {.53, .200, 53},
    {.80, 0, 0},
    {1.0, 0, 0}
};
