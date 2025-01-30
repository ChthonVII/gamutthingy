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
#define MAP_VPRC 4

#define RMZONE_DELTA_BASED 0
#define RMZONE_DEST_BASED 1

#define RETURN_SUCCESS 0
#define ERROR_BAD_PARAM_BOOL 1
#define ERROR_BAD_PARAM_STRING 2
#define ERROR_BAD_PARAM_SELECT 3
#define ERROR_BAD_PARAM_FLOAT 4
#define ERROR_BAD_PARAM_INT 5
#define ERROR_BAD_PARAM_SSCANF 6
#define ERROR_BAD_PARAM_COLOR_NOT_SPECIFIED 7
#define ERROR_BAD_PARAM_INVALID_COLOR 8
#define ERROR_BAD_PARAM_FILE_NOT_SPECIFIED 9
#define ERROR_BAD_PARAM_MISSING_VALUE 10
#define ERROR_BAD_PARAM_UNKNOWN_PARAM 11
#define ERROR_BAD_PARAM_IMPOSSIBLE_COMBO 12
#define ERROR_INVERT_MATRIX_FAIL 13
#define ERROR_PNG_FAIL 14
#define ERROR_PNG_WRITE_FAIL 15
#define ERROR_PNG_READ_FAIL 16
#define ERROR_PNG_MEM_FAIL 17
#define ERROR_PNG_OPEN_FAIL 18
#define GAMUT_INITIALIZE_FAIL 19
#define GAMUT_INITIALIZE_FAIL_SPIRAL 20

#define GAMMA_LINEAR 0
#define GAMMA_SRGB 1
#define GAMMA_REC2084 2

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

#define DAYLIGHTLOCUS 0
#define PLANKIANLOCUS 1

#define LOCKRED 0
#define LOCKGREEN 1
#define LOCKBLUE 2

#define CRT_MODULATOR_NONE -1
#define CRT_MODULATOR_CXA1145 0
#define CRT_MODULATOR_CXA1645 1

#define RENORM_DEMOD_NONE 0
#define RENORM_DEMOD_INSANE 1
#define RENORM_DEMOD_ANGLE_NOT_ZERO 2
#define RENORM_DEMOD_GAIN_NOT_ONE 3
#define RENORM_DEMOD_ANY 4

// YIQ scaling factors
#define Udownscale 0.492111
#define Vdownscale 0.877283
#define Uupscale (1.0/Udownscale)
#define Vupscale (1.0/Vdownscale)
// NTSC-J uses different bandwidth, and, accordingly, different scale factors, such that U and V are about 8% bigger
// But we can ignore this since the difference is the same ~8% factor for U and V, and we're normalizing U's gain to 1 anyway
// See Jack, Keith. "Video Demystified: A Handbook for the Digital Engineer." 4th Ed. pp. 432-33 Elsevier. Burlington, MA. (2005) ISBN: 0-7506-7822-4

#define YUV_CONSTANT_PRECISION_CRAP 0 // truncated constants from 1953 standard
#define YUV_CONSTANT_PRECISION_MID 1 // less truncated constants from 1994 SMPTE-C (170M) standard
#define YUV_CONSTANT_PRECISION_FULL 2 // compute precise constants from 1953 primaries and Illuminant C.

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

#define WHITEPOINT_CUSTOM_TEMP -2
#define WHITEPOINT_CUSTOM_COORD -1
#define WHITEPOINT_D65 0
#define WHITEPOINT_9300K27MPCD 1
#define WHITEPOINT_9300K8MPCD 2
#define WHITEPOINT_ILLUMINANTC 3
#define WHITEPOINT_6900K 4
#define WHITEPOINT_7000K 5
#define WHITEPOINT_7100K 6
#define WHITEPOINT_7250K 7
#define WHITEPOINT_D75 8
#define WHITEPOINT_8500K 9
#define WHITEPOINT_8800K 10
#define WHITEPOINT_BOHNSACK 11
#define WHITEPOINT_NEC_MULTISYNC_C400 12
#define WHITEPOINT_KDS_VS19 13
#define WHITEPOINT_93K_FAIRCHILD 14
#define WHITEPOINT_D65_FAIRCHILD 15
#define WHITEPOINT_D65_DISPLAYMATE 16

const std::string whitepointnames[17] = {
    "D65",
    "9300K + 27mpcd",
    "9300K + 8mpcd",
    "Illuminant C",
    "6900K",
    "7000K",
    "7100K",
    "7250K",
    "D75",
    "8500K",
    "8800K",
    "Triniton ~93K (Bohnsack measurement)",
    "NEC Multisync C400 ~93K",
    "KDS VS19 ~93K",
    "Diamond Pro/Trinitron ~93K (Fairchild measurement)",
    "Diamond Pro/Trinitron ~D65 (Fairchild measurement)",
    "Trinitron ~D65 (DisplayMate measurement)"
};

const double whitepoints[17][3] = {
    // D65
    {0.312713, 0.329016, 0.358271},
    // 9300K + 27MPCD
    {0.281, 0.311, 0.408},
    // 9300K + 8MPCD
    // Average of coordinates stated in:
    //      * Yagishita, Shigeru, Nishino, Kenji, Ohta, Katsuhiro, and Ishii, Takashi. "カラーマスターモニター用基準白色内蔵カラーブラウン管 (Color Picture Tube with Built in Reference White for Color Master Monitors)." テレビジョン (Television), Vol. 31, No. 11, pp. 883-888. 1977.
    //      * Nagaoka, Yoshitomi. "テレビジョンの色再現と基準白色 (On the Color Reproduction and Reference White of Color Television)." テレビジョン学会誌 (Journal of the Television Society), Vol. 33, No. 12, pp. 1013-1020. 1979.
    {0.28345, 0.29775, 0.4188},
    // Illuminant C
    {0.310063, 0.316158, 0.373779},
    // 6900K (Dogway claims, without citation, that consumer Euro CRTs' D65 was often 7100K in practice
    // (https://forums.libretro.com/t/dogways-grading-shader-slang/27148/523))
    // DariusG actually measured a PAL CRT and got 6900K
    // https://forums.libretro.com/t/dogways-grading-shader-slang/27148/524
    // coords estimated via xycoordfromfromCCT()
    {0.306769, 0.322990, 0.370241},
    // 7000K (grade claims, without citation, that consumer CRTs' D65 was often 7000K to 7500K in practice (https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang#L109))
    // coords estimated via xycoordfromfromCCT()
    {0.305390, 0.321565, 0.373045},
    // 7100K (Dogway claims, without citation, that consumer US CRTs' D65 was often 7100K in practice
    // (https://forums.libretro.com/t/dogways-grading-shader-slang/27148/523))
    // coords estimated via xycoordfromfromCCT()
    {0.304054, 0.320173, 0.375773},
    // 7250K (grade claims, without citation, that consumer CRTs' D65 was often 7000K to 7500K in practice (https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang#L109))
    // coords estimated via xycoordfromfromCCT()
    {0.302126, 0.318146, 0.379728},
    // D75 (grade claims, without citation, that consumer CRTs' D65 was often 7000K to 7500K in practice (https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang#L109))
    {0.29902, 0.31485, 0.38613},
    // 8500K (grade claims, without citation, that consumer CRTs' 9300K was often ~8500K in practice (https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang#L118 and also #L306))
    // coords estimated via xycoordfromfromCCT()
    {0.289145, 0.303920, 0.406935},
    // 8800K (grade claims, without citation, that consumer CRTs' 9300K was somtimes ~8800K in practice (https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang#L118 and also #L306))
    // coords estimated via xycoordfromfromCCT()
    {0.286707, 0.301135, 0.412158},
    // Trintron Bohnsack, measured whitepoint of 1994 GDM-17SE1 Trinitron computer monitor
    // probably supposed to be 9300K+8MPCD
    {0.2836, 0.2963, 0.4201},
    // NEC_MULTISYNC_C400 computer monitor
    // Probably supposed to be 9300K + 27MPCD
    // from https://www.cs.ucf.edu/courses/cap5725/spring2003/Chromaticity.htm
    {0.28, 0.315, 0.405},
    // KDS_VS19 computer monitor
    // identical to 9300K + 27MPCD
    // from https://www.cs.ucf.edu/courses/cap5725/spring2003/Chromaticity.htm
    {0.281, 0.311, 0.408},
    // Mitsubishi Diamond Pro (unspecified model number) computer monitor with Trinitron tube D93 Fairchild
    // Probably supposed to be 9300K + 27MPCD
    // only 60 cd/m2 ?!
    // from https://web.archive.org/web/20220517234701/https://www.imaging.org//site/PDFS/Papers/1997/RP-0-67/2401.pdf
    {0.2838, 0.3290, 0.3872},
    // Mitsubishi Diamond Pro (unspecified model number) computer monitor with Trinitron tube D65 Fairchild
    // Supposed to be D65, but pretty far off...
    // only 53 cd/m2 ?!
    // from https://web.archive.org/web/20220517234701/https://www.imaging.org//site/PDFS/Papers/1997/RP-0-67/2401.pdf
    {0.3124, 0.2977, 0.3899},
    // Sony PVM-20L5 (2002) measured as 6480K
    // https://www.displaymate.com/ShootOut_Part_1.htm
    // coords estimated via xycoordfromfromCCT()
    {0.313091, 0.329377, 0.357532},
};

#define GAMUT_CUSTOM -1
#define GAMUT_SRGB 0
#define GAMUT_NTSC 1
#define GAMUT_SMPTEC 2
#define GAMUT_EBU 3
#define GAMUT_REC2020 4
#define GAMUT_P22_AVERAGE 5
#define GAMUT_P22_TRINITRON 6
#define GAMUT_P22_EBUISH 7
#define GAMUT_P22_HITACHI 8
#define GAMUT_P22_NEC_MULTISYNC_C400 9
#define GAMUT_P22_KDS_VS19 10
#define GAMUT_DELL 11
#define GAMUT_JAPAN_SPEC 12
#define GAMUT_P22_TRINITRON_RANEY1 13
#define GAMUT_P22_TRINITRON_RANEY2 14
#define GAMUT_P22_TRINITRON_MIXANDMATCH 15
#define GAMUT_P22_TRINITRON_BOHNSACK 16

const std::string gamutnames[17] = {
    "sRGB / bt709 (specification)",
    "NTSC (specification)",
    "SMPTE-C (specification)",
    "EBU (470bg) (specification)",
    "Rec2020 (specification)",
    "P22 phosphors, Average",
    "P22 phosphors, Trinitron",
    "P22 phosphors, EBU-ish",
    "P22 phosphors, Hitachi",
    "P22 phosphors, NEC Multisync C400",
    "P22 phosphors, KDS VS19",
    "P22 phosphors, Dell",
    "P22 phosphors, \"Japan Specific\"",
    "P22 phosphors, Trinitron, Raney Measurement 1",
    "P22 phosphors, Trinitron, Raney Measurement 2",
    "P22 phosphors, Trinitron, Mix-and-Match",
    "P22 phosphors, Trinitron, Bohnsack Measurement",
};

const double gamutpoints[17][3][3] = {
    // srgb_spec
    {
        {0.64, 0.33, 0.03}, //red
        {0.3, 0.6, 0.1}, //green
        {0.15, 0.06, 0.79} //blue
    },
    // ntsc_spec
    {
        {0.67, 0.33, 0.0}, //red
        {0.21, 0.71, 0.08}, //green
        {0.14, 0.08, 0.78} //blue
    },
        // smptec_spec
    {
        {0.63, 0.34, 0.03}, //red
        {0.31, 0.595, 0.095}, //green
        {0.155, 0.07, 0.775} //blue
    },
    // ebu_spec
    {
        {0.64, 0.33, 0.03}, //red
        {0.29, 0.6, 0.11}, //green
        {0.15, 0.06, 0.79} //blue
    },
    // rec2020_spec
    {
        {0.708, 0.292, 0.0}, //red
        {0.17, 0.797, 0.033}, //green
        {0.131, 0.046, 0.823} //blue
    },
    // P22_average
    // ntscj (measurements taken upon television set receiver using P22 phospors, whitepoint 9300K+27mpcd)
    // see: https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang
    // "Mix between averaging KV-20M20, KDS VS19, Dell D93, 4-TR-B09v1_0.pdf and Phosphor Handbook 'P22'
    // Phosphors based on 1975's EBU Tech.3123-E (formerly known as JEDEC-P22)
    // Typical P22 phosphors used in Japanese consumer CRTs with 9300K+27MPCD white point"
    {
        {0.625, 0.350, 0.025}, //red
        {0.280, 0.605, 0.115}, //green
        {0.152, 0.062, 0.786} //blue
    },
    // P22_trinitron
    // NTSC-J with Trinitron P22 phosphors
    // Color.org says that, according to “reference data… provided by the manufacturers,” these are chromaticities of Trinitron computer monitor phosphors. (+/- 0.03)
    // see: https://www.color.org/wpaper1.xalter
    // In 1997, some visual science researchers decided to measure the properties of their 1994-model GDM-17SE1 and got values very close to that.
    // see: https://www.scholars.northwestern.edu/en/publications/characteristics-of-the-sony-multiscan-17se-trinitron-color-graphi
    // Having no better data source, we just **ASSUME** Sony used the same phosphors in its 1994 Trinitron televisions as its 1994 Trinitron computer monitors.
    // Reviewing service manuals on https://crtdatabase.com/ for overlapping tubes and demodulator chips suggests that there was no change of phosphors for Trinitron televisions from at least 1986 to 1999.
    {
        {0.621, 0.34, 0.039}, //red
        {0.281, 0.606, 0.113}, //green
        {0.152, 0.067, 0.781} //blue
    },

    // P22_ebuish_9300K
    // ntscj (EBUish phosphors noted in a 1992 Toshiba patent, whitepoint 9300K+27mpcd)
    // see: https://patents.google.com/patent/US5301017A/en?oq=+5%2c301%2c017
    // The patent describes these as “the phosphor in a cathode ray tube(CRT) used in the typical television receiver”
    // Other sources say EBU phosphors were limited to professional and possibly high-end consumer models.
    {
        {0.657, 0.338, 0.005}, //red
        {0.297, 0.609, 0.094}, //green
        {0.148, 0.054, 0.798} //blue
    },

    // P22_hitachi_9300K
    // NTSC-J with Hitachi P22 phosphors
    // Color.org says that, according to “reference data… provided by the manufacturers,” these are chromaticities of the Hitachi CM2198 computer monitor phosphors. (+/- 0.02)
    // see: https://www.color.org/wpaper1.xalter
    // Hitachi also made a CMT2198 CRT television.
    // Having no better data source, we just **ASSUME** Hitachi used the same phosphors in both the CM2198 and CMT2198.
    // Reviewing the service manual indiciates that the CMT2187, 2196, 2198, and 2199 are substantially identical.
    // Somewhat puzzlingly, it's not clear whether there were distinct JP and US models.
    // 2196 and 2198 are lumped together, collectively showing both US and Japan channel coverage.
    // Both use the same jungle chip (TDA8362), which does not have distinct JP and US modes, and uses just one color correction matrix for NTSC.
    // And that throws the whitepoint into doubt.
    // It's possible that one model was the JP model, and the other the US model, and the whitepoint was controlled by a circuit outside of the jungle chip.
    // It's also possible that models with the same whitepoint (probably 9300K) were sold in both Japan and US.
    {
        {0.624, 0.339, 0.037}, //red
        {0.285, 0.604, 0.111}, //green
        {0.150, 0.065, 0.785} //blue
    },

    // NEC_MULTISYNC_C400 computer monitor
    // from https://www.cs.ucf.edu/courses/cap5725/spring2003/Chromaticity.htm
    // which stole it from https://web.archive.org/web/20190613001950/http://efg2.com/Lab/Graphics/Colors/Chromaticity.htm
    // which short cites a source but forgot to put it in the bibliography
    {
        {0.610, 0.35, 0.04}, //red
        {0.307, 0.595, 0.098}, //green
        {0.15, 0.065, 0.785} //blue
    },

    // KDS_VS19 computer monitor
    // from https://www.cs.ucf.edu/courses/cap5725/spring2003/Chromaticity.htm
    // which stole it from https://web.archive.org/web/20190613001950/http://efg2.com/Lab/Graphics/Colors/Chromaticity.htm
    {
        {0.625, 0.34, 0.035}, //red
        {0.285, 0.605, 0.11}, //green
        {0.15, 0.065, 0.785} //blue
    },

    // Dell computer monitor (all monitors except 21" Mitsubishi p/n 65532)
    // Use with 9300K whitepoint (unclear which)
    // from https://www.cs.ucf.edu/courses/cap5725/spring2003/Chromaticity.htm
    // which stole it from https://web.archive.org/web/20190613001950/http://efg2.com/Lab/Graphics/Colors/Chromaticity.htm
    // which cites https://web.archive.org/web/20171114044833/http://www.efg2.com/Lab/Graphics/Colors/DellInfo.txt
    {
        {0.625, 0.34, 0.035}, //red
        {0.275, 0.605, 0.12}, //green
        {0.15, 0.065, 0.785} //blue
    },

    // "Japan Specific Phosphor" described in ARIB TR B9 v1.0 (1998)
    // https://web.archive.org/web/20130413094712/http://arib.or.jp/english/html/overview/doc/4-TR-B09v1_0.pdf
    {
        {0.618, 0.35, 0.032}, //red
        {0.29, 0.6, 0.11}, //green
        {0.15, 0.06, 0.97} //blue
    },

    // Sony PVM 20M2U (~1996) measured by Keith Raney
    // https://github.com/danmons/colour_matrix_adaptations/blob/main/csv/inputs.csv
    {
        {0.63, 0.345, 0.025}, //red
        {0.285, 0.605, 0.11}, //green
        {0.15, 0.065, 0.785} //blue
    },

    // Sony PVM 20L2MDU (~2002) measured by Keith Raney
    // https://github.com/danmons/colour_matrix_adaptations/blob/main/csv/inputs.csv
    {
        {0.625, 0.345, 0.03}, //red
        {0.28, 0.605, 0.115}, //green
        {0.15, 0.065, 0.785} //blue
    },

    // Sony mix-and-match of best values from different measurements
    // red from Sony PVM 20M2U (~1996) measured by Keith Raney
    // green and blue from color.org
    {
        {0.63, 0.345, 0.025}, //red
        {0.281, 0.606, 0.113}, //green
        {0.152, 0.067, 0.781} //blue
    },

    // Trinitron Bohnsack
    // In 1997, some visual science researchers decided to measure the properties of their 1994-model GDM-17SE1 and got values very close to that.
    // see: https://www.scholars.northwestern.edu/en/publications/characteristics-of-the-sony-multiscan-17se-trinitron-color-graphi
    {
        {0.6233, 0.339, 0.0377}, //red
        {0.2838, 0.5895, 0.1267}, //green
        {0.1519, 0.0669, 0.7812} //blue
    },

    // I am deliberately omitting grade's P22_90s_ph because it is definitely wrong.
    // Grade: https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang#L728
    // Better explanation from Dogway: https://github.com/Dogway/Avisynth-Scripts/blob/258644dea7a1fbc9a71e7f39d6bb62234a80e1c7/TransformsPack%20-%20Main.avsi#L2348
    // A Y2O2S:Eu3+ red phosphor coated with FE2O3 pigment could technically reach an x coord of 0.661, but only with a 50% brightness loss.
    // See figure 1 in Ohno, Katsutoshi and Kusunoki, Tsuneo. "The Effect of Ultrafine Pigment Color Filters on Cathode Ray Tube Brightness, Contrast, and Color Purity." J. Electrochem. Soc., Vol. 143, No. 3, p. 1063 (1996).
    // Dogway likely confused the maximum possible coords result reported in some literature with the realistic coords achievable at acceptable brightness.
    // Green looks suspicious too. That y coord is off the chart for ZnS:Au,Cu,Al.
    // See Yen, William, Shionoya, Shigeo, and Yamamoto, Hajime. "Phosphor Handbook Second Edition," section 6.2.3, table 2. CRC Press, Boca Raton (2007).


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

#define CRT_DEMODULATOR_NONE -1
#define CRT_DEMODULATOR_DUMMY 0
#define CRT_DEMODULATOR_CXA1464AS 1
#define CRT_DEMODULATOR_CXA1465AS 2
#define CRT_DEMODULATOR_CXA1870S_JP 3
#define CRT_DEMODULATOR_CXA1870S_US 4
#define CRT_DEMODULATOR_CXA2060BS_JP 5
#define CRT_DEMODULATOR_CXA2060BS_US 6
#define CRT_DEMODULATOR_CXA2060BS_PAL 7
#define CRT_DEMODULATOR_CXA2025AS_JP 8
#define CRT_DEMODULATOR_CXA2025AS_US 9
#define CRT_DEMODULATOR_CXA1213AS 10
#define CRT_DEMODULATOR_TDA8362 11

const std::string demodulatornames[12] = {
    "Dummy/PAL/SMPTE-C (no color correction)",
    "CXA1464AS (JP)",
    "CXA1465AS (US)",
    "CXA1870S (JP mode)",
    "CXA1870S (US mode)",
    "CXA2060BS (JP mode)",
    "CXA2060BS (US mode)",
    "CXA2060BS (PAL mode)",
    "CXA2025AS (JP mode)",
    "CXA2025AS (US mode)",
    "CXA1213AS",
    "TDA8362"
};

const double demodulatorinfo[12][2][3] = {
    
    // Dummy -- No color correction!
    // Use this for content in the PAL or SMPTE-C that did not use color correction.
    {
        {90, 236, 0}, // angles (degrees)
        {0.56, 0.34, 1.0} // gains

    },

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
    
    // CXA2060BS (PAL mode)
    // Used in Sony Trinitron ~??? (probably around 1997)
    {
        {90, 227, 0}, // angles (degrees)
        {Vupscale/Uupscale, 0.34, 1.0} // gains
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
    // Theoretially, blue at a non-zero angle should mean that gains need renormalized,
    // but I suspect they were not in practice.
    // (0.77, 0.3, 1.0 is more similar to other chips than 0.74, 0.28, 0.96
    {
        {99, 240, 11}, // angles (degrees)
        {0.77, 0.3, 1.0} // gains
    },

    // TDA8362
    // Used in Hitachi CMT2187/2196/2198/2199
    // Very likely match to the Hitachi P22 phosphor constants above.
    // This chip does not have distinct JP and US modes, so one color correction matrix used for both apparently.
    // It's also not clear if televisions sold in the US and Japan shared one whitepoint.
    // These values are pretty wild (especially red gain). Not sure if gains should be renormalized for blue at non-zero angle. (Probably should since not doing it looks pretty bad.)
    {
        {100, 235, -10}, // angles (degrees)
        {1.14, 0.3, 1.14} // gains
    },
};

#endif
