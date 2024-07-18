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
#define GAMUT_NTSCJ_EBU 4
#define GAMUT_NTSCJ_P22_TRINITRON 5
#define GAMUT_NTSCU_P22_TRINITRON 6
#define GAMUT_SMPTEC_P22_TRINITRON 7
#define GAMUT_SMPTEC 8
#define GAMUT_EBU 9
#define GAMUT_NTSC_1953 10
    
#define MAP_CLIP 0
#define MAP_CCC_A 1
#define MAP_CCC_B 2
#define MAP_CCC_C 3
#define MAP_CCC_D 4
#define MAP_CCC_E 5
#define MAP_COMPRESS 6
#define MAP_EXPAND 7
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
#define ERROR_BAD_PARAM_SPIRAL_CARISMA 18
#define ERROR_BAD_PARAM_SC_FUNCTION_TYPE 19
#define ERROR_BAD_PARAM_CRT_EMU_MODE 20
#define ERROR_INVERT_MATRIX_FAIL 21
#define ERROR_PNG_FAIL 22
#define ERROR_PNG_WRITE_FAIL 23
#define ERROR_PNG_READ_FAIL 24
#define ERROR_PNG_MEM_FAIL 25
#define ERROR_PNG_OPEN_FAIL 26
#define GAMUT_INITIALIZE_FAIL 27
#define GAMUT_INITIALIZE_FAIL_SPIRAL 28


extern const vec3 D65;

#define ADAPT_BRADFORD 0
#define ADAPT_CAT16 1

#define CRT_EMU_NONE 0
#define CRT_EMU_FRONT 1
#define CRT_EMU_BACK 2

#define CCC_EXPONENTIAL 0
#define CCC_CUBIC_HERMITE 1

#define SC_EXPONENTIAL 0
#define SC_CUBIC_HERMITE 1

#define LOCKRED 0
#define LOCKGREEN 1
#define LOCKBLUE 2

#define CRT_MODULATOR_NONE -1
#define CRT_MODULATOR_CXA1145 0
#define CRT_MODULATOR_CXA1465 1

#define CRT_DEMODULATOR_NONE -1
#define CRT_DEMODULATOR_CXA1464AS 0
#define CRT_DEMODULATOR_CXA1465AS 1



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



const std::string gamutnames[11] = {
    "sRGB / bt709",
    "NTSC-J (television set receiver specification)",
    "NTSC-J (broadcast specification)",
    "NTSC-J (P22 phosphors)",
    "NTSC-J (EBU phosphors)",
    "NTSC-J (Trinitron P22 phosphors)",
    "NTSC-U (Trinitron P22 phosphors)",
    "SMPTE-C (Trinitron P22 phosphors)",
    "SMPTE-C",
    "EBU (470bg)",
    "NTSC 1953"
};

const double gamutpoints[11][4][3] = {
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
    // ntscj (EBU phosphors noted in a 1992 Toshiba patent, whitepoint 9300K+27mpcd)
    // see: https://patents.google.com/patent/US5301017A/en?oq=+5%2c301%2c017
    // The patent describes these as “the phosphor in a cathode ray tube(CRT) used in the typical television receiver”
    // Other sources say EBU phosphors were limited to professional and possibly high-end consumer models.
    {
        {0.281, 0.311, 0.408}, //white
        {0.657, 0.338, 0.005}, //red
        {0.297, 0.609, 0.094}, //green
        {0.148, 0.054, 0.798} //blue
    },
    // NTSC-J with Trinitron P22 phosphors
    // Color.org says that, according to “reference data… provided by the manufacturers,” these are chromaticities of Trinitron computer monitor phosphors. (+/- 0.03)
    // see: https://www.color.org/wpaper1.xalter
    // In 1997, some visual science researchers decided to measure the properties of their 1994-model GDM-17SE1 and got values very close to that.
    // see: https://www.scholars.northwestern.edu/en/publications/characteristics-of-the-sony-multiscan-17se-trinitron-color-graphi
    // Having no better data source, we just **ASSUME** Sony used the same phosphors in its 1994 Trinitron televisions as its 1994 Trinitron computer monitors.
    // Reviewing service manuals on https://crtdatabase.com/ for overlapping tubes and demodulator chips suggests that there was no change of phosphors for Trinitron televisions from at least 1986 to 1999.
    {
        {0.281, 0.311, 0.408}, //white 9300K+27mpcd
        {0.621, 0.34, 0.039}, //red
        {0.281, 0.606, 0.113}, //green
        {0.152, 0.067, 0.781} //blue
    },
    // NTSC-U with Trinitron P22 phosphors
    // same as above, just different white balance
    {
        {0.310063, 0.316158, 0.373779}, // white Illuminant C 
        {0.621, 0.34, 0.039}, //red
        {0.281, 0.606, 0.113}, //green
        {0.152, 0.067, 0.781} //blue
    },
    // SMPTE-C with Trinitron P22 phosphors
    // same as above, just different white balance
    {
        {0.312713, 0.329016, 0.358271}, // white D65
        {0.621, 0.34, 0.039}, //red
        {0.281, 0.606, 0.113}, //green
        {0.152, 0.067, 0.781} //blue
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
    },
    // NTSC 1953
    {
        {0.310063, 0.316158, 0.373779}, //white
        {0.67, 0.33, 0}, //red
        {0.21, 0.71, 0.08}, //green
        {0.14, 0.08, 0.78} //blue
    }
};

const std::string modulatornames[2] = {
    "CXA1145",
    "CXA1645"
};

const double modulatorinfo[2][3][3] = {
    // CXA1145
    {
        {104, 241, 347}, // angles (degrees)
        {3.16, 2.95, 2.24}, // ratios
        {2.0/7.0, 5.0/7.0, 0.0} // burst vpp, white v, dummy (assume 0.29vpp burst is really 40 IRE = 2/7vpp; assume 0.71v white is really 100 IRE = 5/7v)
    },
    // CXA1645
    {
        {104, 241, 347}, // angles (degrees)
        {3.16, 2.95, 2.24}, // ratios
        {0.25, 5.0/7.0, 0.0} // burst vpp, white v, dummy (assume 0.71v white is really 100 IRE = 5/7v)

    }
};

const std::string demodulatornames[2] = {
    "CXA1464AS (JP)",
    "CXA1465AS (US)"
};

const double demodulatorinfo[2][2][3] = {
    // CXA1464AS (JP)
    {
        {98, 243, 0}, // angles (degrees)
        {0.78, 0.31, 1.0} // gains

    },
    // CXA1465AS (US)
    {
        {114, 255, 0}, // angles (degrees)
        {0.78, 0.31, 1.0} // gains

    }
};

#endif
