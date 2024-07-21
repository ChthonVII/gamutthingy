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
#define CRT_MODULATOR_CXA1645 1

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

const std::string modulatornames[4] = {
    "CXA1145",
    "CXA1645",
    "MB3514",
    "CXA1219"
};

const double modulatorinfo[4][3][3] = {
    // Sony CXA1145
    // Used in most 1st generation and some 2nd generation Sega Genesis
    // See this link for detailed Sega Genesis notes: https://consolemods.org/wiki/Genesis:Video_Output_Notes
    // Used in Sega Master System II
    // Used in NEO GEO AES (see: https://www.retrosix.wiki/cxa-video-encoder-neo-geo-aes)
    // Reported used in Amiga consoles
    // Reportedly used for SNK consoles (see: https://gamesx.com/wiki/doku.php?id=av:sony_cxa_series)
    // Very common -- Safe guess for most early 90s consoles 
    {
        {104, 241, 347}, // angles (degrees)
        {3.16, 2.95, 2.24}, // ratios
        {2.0/7.0, 5.0/7.0, 0.0} // burst vpp, white v, dummy (assume 0.29vpp burst is really 40 IRE = 2/7vpp; assume 0.71v white is really 100 IRE = 5/7v)
    },
    // Sony CXA1645
    // Upgrade to CXA1145
    // Used in some 2nd generation and all 3rd generation Sega Genesis
    // Used in Sony Playstation 1
    // Used in Genesis 3
    // Used in Sega Saturn
    // Used in NeoGeo CD/CDZ
    {
        {104, 241, 347}, // angles (degrees)
        {3.16, 2.95, 2.24}, // ratios
        {0.25, 5.0/7.0, 0.0} // burst vpp, white v, dummy (assume 0.71v white is really 100 IRE = 5/7v)
    },
    // Fujitsu MB3514
    // Clone of CXA1145
    // Used in some 1st and 2nd generation Sega Gensis
    // Used in some PAL Master System II
    // Data sheet says burst is 0.57vpp. But that's insanely high.
    // Assuming data sheet author accidentally doubled vpp value mistaken thinking it was v value.
    // That would work out to theorically correct 2/7. (Also, idential to the CXA1145.)
    {
        {104, 241, 347}, // angles (degrees)
        {3.16, 2.95, 2.24}, // ratios
        {2.0/7.0, 5.0/7.0, 0.0} // burst vpp, white v, dummy (assume 0.71v white is really 100 IRE = 5/7v)
    },
    // Sony CXA1219 ~1992
    // CXA1229 should have same values
    {
        {104, 241, 347}, // angles (degrees)
        {2.92, 2.74, 2.08}, // ratios
        {2.0/7.0, 5.0/7.0, 0.0} // burst vpp, white v, dummy (assume 0.71v white is really 100 IRE = 5/7v)
    }
    
    // TODO:
    // Samsung KA2195D - Sega Gensis 2nd generation - inferior CXA1145 clone
    // ROHM BH7236AF - PAL megadrive II - CXA1645 clone?
    // ES71145 ITRI clones of 1145
    // CXA2075M upgrade to 1645
    
    // Master system 1  Sony V7040 https://www.smspower.org/Development/VideoOutput
    // ROHM BA6592F very first SNES
    // S-ENC early SNES, likely rebranded BA6592F or BA6594F
    // S-ENC B mid SNES rebranded BA6594F
    // S-RGB late SNES rebranded BA6595F
    // S-RGB A last SNES rebranded BA6596F
    // ENC-NUS early N64 rebraded BA7242F
    // BA7232FS likely a BA6595F or BA6596F in different form factor
    // BA6591AF likely similar to BA6592F

};

const std::string demodulatornames[98] = {
    "CXA1464AS (JP)",
    "CXA1465AS (US)",
    "CXA1870S (JP mode)",
    "CXA1870S (US mode)",
    "CXA2060BS (JP mode)",
    "CXA2060BS (US mode)",
    "CXA2025AS (JP mode)",
    "CXA2025AS (US mode)",
    "CXA1213AS"
};

const double demodulatorinfo[9][2][3] = {
    
    // CXA1464AS (JP)
    // Used in Sony Trinitron ~1993 - ~1995
    {
        {98, 243, 0}, // angles (degrees)
        {0.78, 0.31, 1.0} // gains

    },
    
    // CXA1465AS (US)
    // Used in Sony Trinitron ~1993 - ~1995
    {
        {114, 255, 0}, // angles (degrees)
        {0.78, 0.31, 1.0} // gains

    },
    
    // CXA1870S (JP mode)
    // Used in Sony Trinitron ~1996
    {
        {96, 240, 0}, // angles (degrees)
        {0.8, 0.3, 1.0} // gains
    },
    
    // CXA1870S (US mode)
    // Used in Sony Trinitron ~1996
    {
        {105, 252, 0}, // angles (degrees)
        {0.8, 0.3, 1.0} // gains
    },
    
    // CXA2061S (JP and US modes)
    // Used in Sony Trinitron ~1997 - ~1999
    // But datasheet doesn't state axis info! Possibly similar to CXA2060BS below.
    
    // CX20192
    // Used in Sony Trinitron "late 80's/early 90's"
    // Can't find datasheet
    
    // CXA1013AS
    // Used in Sony Trinitron ~1993
    // Can't find datasheet
    
    // CXA1865S
    // Used in Sony Trinitron
    // Apparently an upgrade to CXA1465AS
    // Can't find datasheet
    
    // Demodulators above this point were *probably* all designed with the same phosphors in mind
    // They can all be found in a cluster of Trinitron models with overlapping tubes and/or demodulators.
    // And those phosphors are *probably* the ones in the Trinitron P22 gamutpoints constants above.
    // Demodulators below this point *might* be designed for use with the same phosphors,
    // or Sony might have changed phosphors at some point -- I don't know.
    
    // CXA2060BS (JP mode)
    // Used in Sony Trinitron ~??? (probably around 1997)
    {
        {95, 236, 0}, // angles (degrees)
        {0.78, 0.33, 1.0} // gains
    },
    
    // CXA2060BS (US mode)
    // Used in Sony Trinitron ~??? (probably around 1997)
    {
        {102, 236, 0}, // angles (degrees)
        {0.78, 0.3, 1.0} // gains
    },
    
    // CXA2025AS (JP mode)
    // Used in Sony Trinitron ~1997
    {
        {95, 240, 0}, // angles (degrees)
        {0.78, 0.3, 1.0} // gains
    },
    
    // CXA2025AS (US mode)
    // Used in Sony Trinitron ~1997
    {
        {112, 252, 0}, // angles (degrees)
        {0.83, 0.3, 1.0} // gains
    },
    
    // CXA1213AS
    // Used in Sony Trinitron(?) ~1992
    // Does not appear to have distinct JP and US modes
    // It's possible this chip is either JP or US and there exists another chip number for the other. 
    // TODO: Blue at a non-zero angle likely means that gains need renormalized.
    {
        {99, 240, 11}, // angles (degrees)
        {0.77, 0.3, 1.0} // gains
    }
};

#endif
