#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "vec3.h"
#include <string>

#define VERBOSITY_SILENT 0
#define VERBOSITY_MINIMAL 1
#define VERBOSITY_SLIGHT 2
#define VERBOSITY_HIGH 3
#define VERBOSITY_EXTREME 4

#define EPSILON 1e-6 // this is about where rounding errors start to creep in
#define EPSILONZERO 1e-10 // for checking zero
#define EPSILONDONTCARE 2e-4 // this is for silencing errors on the between() function

#define GAMUT_SRGB 0
#define GAMUT_NTSCJ_R 1
#define GAMUT_NTSCJ_B 2
#define GAMUT_NTSCJ_P22 3
#define GAMUT_SMPTEC 4
#define GAMUT_EBU 5
    
#define MAP_CLIP 0
#define MAP_CCC_A 1
#define MAP_COMPRESS 2
#define MAP_EXPAND 3
#define MAP_FIRST_COMPRESS MAP_COMPRESS // lowest numbered option involving gamut compression

#define MAP_GCUSP 0
#define MAP_HLPCM 1
#define MAP_VP 2
#define MAP_VPR 3

#define RMZONE_DELTA_BASED 0
#define RMZONE_DEST_BASED 1

#define RETURN_SUCCESS 0
#define ERROR_BAD_PARAM_SOURCE_GAMUT 1
#define ERROR_BAD_PARAM_DEST_GAMUT 2
#define ERROR_BAD_PARAM_MAPPING_MODE 3
#define ERROR_BAD_PARAM_MAPPING_FLOAT 4
#define ERROR_BAD_PARAM_MAPPING_DIRECTION 5
#define ERROR_BAD_PARAM_MISSING_VALUE 6
#define ERROR_BAD_PARAM_UNKNOWN_PARAM 7
#define ERROR_BAD_PARAM_FILE_NOT_SPECIFIED 8
#define ERROR_BAD_PARAM_COLOR_NOT_SPECIFIED 9
#define ERROR_BAD_PARAM_INVALID_COLOR 10
#define ERROR_BAD_PARAM_ZONE_TYPE 11
#define ERROR_BAD_PARAM_KNEE_TYPE 12
#define ERROR_BAD_PARAM_GAMMA_TYPE 13
#define ERROR_BAD_PARAM_DITHER 14
#define ERROR_BAD_PARAM_VERBOSITY 15
#define ERROR_BAD_PARAM_ADAPT_TYPE 16
#define ERROR_BAD_PARAM_CCC_FUNCTION_TYPE 17
#define ERROR_INVERT_MATRIX_FAIL 18
#define ERROR_PNG_FAIL 19
#define ERROR_PNG_WRITE_FAIL 20
#define ERROR_PNG_READ_FAIL 21
#define ERROR_PNG_MEM_FAIL 22
#define ERROR_PNG_OPEN_FAIL 23
#define GAMUT_INITIALIZE_FAIL 24


extern const vec3 D65;

#define ADAPT_BRADFORD 0
#define ADAPT_CAT16 1

#define CCC_EXPONENTIAL 0
#define CCC_CUBIC_HERMITE 1

// see:
// K.M. Lam, “Metamerism and Colour Constancy,” Ph.D. Thesis, University of Bradford, 1985.
// http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html
// https://cran.r-project.org/web/packages/spacesXYZ/vignettes/adaptation.html
const double BradfordMatrix[3][3] = {
    {0.8951, 0.2664, -0.1614},
    {-0.7502, 1.7135, 0.0367},
    {0.0389, -0.0685, 1.0296}
};

// see:
// Li Changjun, et al., A Revision of CIECAM02 and its CAT and UCS (2016) (https://library.imaging.org/admin/apis/public/api/ist/website/downloadArticle/cic/24/1/art00035)
// https://github.com/LeaVerou/color.js/blob/f505c1b5230e1eabf5893f6a8eefb335936e55e0/src/CATs.js
const double CAT16Matrix[3][3] = {
    {0.401288, 0.650173, -0.051461},
    {-0.250268, 1.204414, 0.045854},
    {-0.002079, 0.048952, 0.953127}
};



const std::string gamutnames[6] = {
    "sRGB / bt709",
    "NTSC-J (television set receiver)",
    "NTSC-J (broadcast)",
    "NTSC-J (P22 phosphors)",
    "SMPTE-C",
    "EBU (470bg)"
};

const double gamutpoints[6][4][3] = {
    // srgb
    {
        {0.312713, 0.329016, 0.358271}, //white
        {0.64, 0.33, 0.03}, //red
        {0.3, 0.6, 0.1}, //green
        {0.15, 0.06, 0.79} //blue
    },
    // ntscj (television set receiver, whitepoint 9300K+27mpcd)
    {
        {0.281, 0.311, 0.408}, //white
        {0.67, 0.33, 0.0}, //red
        {0.21, 0.71, 0.08}, //green
        {0.14, 0.08, 0.78} //blue
    },
    // ntscj (broadcast, whitepoint 9300K+8mpcd)
    {
        {0.2838, 0.2981, 0.4181}, //white
        {0.67, 0.33, 0.0}, //red
        {0.21, 0.71, 0.08}, //green
        {0.14, 0.08, 0.78} //blue
    },
    // ntscj (measurements taken upon television set receiver using P22 phospors, whitepoint 9300K+27mpcd)
    // see: https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang
    // "Mix between averaging KV-20M20, KDS VS19, Dell D93, 4-TR-B09v1_0.pdf and Phosphor Handbook 'P22'
    // Phosphors based on 1975's EBU Tech.3123-E (formerly known as JEDEC-P22)
    // Typical P22 phosphors used in Japanese consumer CRTs with 9300K+27MPCD white point"
    {
        {0.281, 0.311, 0.408}, //white
        {0.625, 0.350, 0.025}, //red
        {0.280, 0.605, 0.115}, //green
        {0.152, 0.062, 0.786} //blue
    },
    // smptec
    {
        {0.312713, 0.329016, 0.358271}, //white
        {0.63, 0.34, 0.03}, //red
        {0.31, 0.595, 0.095}, //green
        {0.155, 0.07, 0.775} //blue
    },
    // ebu
    {
        {0.312713, 0.329016, 0.358271}, //white
        {0.64, 0.33, 0.03}, //red
        {0.29, 0.6, 0.11}, //green
        {0.15, 0.06, 0.79} //blue
    }
};


#endif
