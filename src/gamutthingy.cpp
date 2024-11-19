#include <string.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <math.h>
#include <fstream>

// Include either installed libpng or local copy. Linux should have libpng-dev installed; Windows users can figure stuff out.
//#include "../../png.h"
#include <png.h> 

#include "constants.h"
#include "vec3.h"
#include "matrix.h"
//#include "cielab.h"
#include "jzazbz.h"
#include "gamutbounds.h"
#include "colormisc.h"
#include "crtemulation.h"
#include "nes.h"

void printhelp(){
    printf("Usage is:\n\n`--help` or `-h`: Displays help.\n\n`--color` or `-c`: Specifies a single color to convert. A message containing the result will be printed to stdout. Should be a \"0x\" prefixed hexadecimal representation of an RGB8 color. For example: `0xFABF00`.\n\n`--infile` or `-i`: Specifies an input file. Should be a .png image.\n\n`--outfile` or `-o`: Specifies an input file. Should be a .png image.\n\n`--gamma` or `-g`: Specifies the gamma function (and inverse) to be applied to the input and output. Possible values are `srgb` (default) and `linear`. LUTs for FFNx should be created using linear RGB. Images should generally be converted using the sRGB gamma function.\n\n`--source-gamut` or `-s`: Specifies the source gamut. Possible values are:\n\t`srgb`: The sRGB gamut used by (SDR) modern computer monitors. Identical to the bt709 gamut used for modern HD video.\n\t`ntscj`: alias for `ntscjr`.\n\t`ntscjr`: The variant of the NTSC-J gamut used by Japanese CRT television sets, official specification. (whitepoint 9300K+27mpcd) Default.\n\t`ntscjp22`: NTSC-J gamut as derived from average measurements conducted on Japanese CRT television sets with typical P22 phosphors. (whitepoint 9300K+27mpcd) Deviates significantly from the specification, which was usually compensated for by a \"color correction circuit.\" See readme for details.\n\t`ntscjb`: The variant of the NTSC-J gamut used for SD Japanese television broadcasts, official specification. (whitepoint 9300K+8mpcd)\n\t`smptec`: The SMPTE-C gamut used for American CRT television sets/broadcasts and the bt601 video standard.\n\t`ebu`: The EBU gamut used in the European 470bg television/video standards (PAL).\n\n`--dest-gamut` or `-d`: Specifies the destination gamut. Possible values are the same as for source gamut. Default is `srgb`.\n\n`--adapt` or `-a`: Specifies the chromatic adaptation method to use when changing white points. Possible values are `bradford` and `cat16` (default).\n\n`--map-mode` or `-m`: Specifies gamut mapping mode. Possible values are:\n\t`clip`: No gamut mapping is performed and linear RGB output is simply clipped to 0, 1. Detail in the out-of-bounds range will be lost.\n\t`compress`: Uses a gamut (compression) mapping algorithm to remap out-of-bounds colors to a smaller zone inside the gamut boundary. Also remaps colors originally in that zone to make room. Essentially trades away some colorimetric fidelity in exchange for preserving some of the out-of-bounds detail. Default.\n\t`expand`: Same as `compress` but also applies the inverse of the compression function in directions where the destination gamut boundary exceeds the source gamut boundary. (Also, reverses the order of the steps in the `vp` and `vpr` algorithms.) The only use for this is to prepare an image for a \"roundtrip\" conversion. For example, if you want to display a sRGB image as-is in FFNx's NTSC-J mode, you would convert from sRGB to NTSC-J using `expand` in preparation for FFNx doing the inverse operation.\n\n`--gamut-mapping-algorithm` or `--gma`: Specifies which gamut mapping algorithm to use. (Does nothing if `--map-mode clip`.) Possible values are:\n\t`cusp`: The CUSP algorithm, but with tunable compression parameters. See readme for details.\n\t`hlpcm`: The HLPCM algorithm, but with tunable compression parameters. See readme for details.\n\t`vp`: The VP algorithm, but with linear light scaling and tunable compression parameters. See readme for details.\n\t`vpr`: VPR algorithm, a modification of VP created for gamutthingy. The modifications are explained in the readme. Default.\n\n`--safe-zone-type` or `-z`: Specifies how the outer zone subject to remapping and the inner \"safe zone\" exempt from remapping are defined. Possible values are:\n\t`const-fidelity`: The zones are defined relative to the distance from the \"center of gravity\" to the destination gamut boundary. Yields consistent colorimetric fidelity, with variable detail preservation.\n\t`const-detail`: The remapping zone is defined relative to the difference between the distances from the \"center of gravity\" to the source and destination gamut boundaries. As implemented here, an overriding minimum size for the \"safe zone\" (relative to the destination gamut boundary) may also be enforced. Yields consistent detail preservation, with variable colorimetric fidelity (setting aside the override option). Default.\n\n`--remap-factor` or `--rf`: Specifies the size of the remapping zone relative to the difference between the distances from the \"center of gravity\" to the source and destination gamut boundaries. (Does nothing if `--safe-zone-type const-fidelity`.) Default 0.4.\n\n`--remap-limit` or `--rl`: Specifies the size of the safe zone (exempt from remapping) relative to the distance from the \"center of gravity\" to the destination gamut boundary. If `--safe-zone-type const-detail`, this serves as a minimum size limit when application of `--remap-factor` would lead to a smaller safe zone. Default 0.9.\n\n`--knee` or `-k`: Specifies the type of knee function used for compression, `hard` or `soft`. Default `soft`.\n\n`--knee-factor` or `--kf`: Specifies the width of the soft knee relative to the size of the remapping zone. (Does nothing if `--knee hard`.) Note that the soft knee is centered at the knee point, so half the width extends into the safe zone, thus expanding the area that is remapped. Default 0.4.\n\n`--dither` or `--di`: Specifies whether to apply dithering to the ouput, `true` or `false`. Uses Martin Roberts' quasirandom dithering algorithm. Dithering should be used for images in general, but should not be used for LUTs.  Default `true`.\n\n`--verbosity` or `-v`: Specify verbosity level. Integers 0-5. Default 2.\n");
    return;
}


typedef struct memo{
    bool known;
    vec3 data;
} memo;
    
// keep memos so we don't have to process the same color over and over in file mode
// this has to be global because it's too big for the stack
memo memos[256][256][256];

// Do the full conversion process on a given color
vec3 processcolor(vec3 inputcolor, bool gammamodein, bool gammamodeout, int mapmode, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, int cccfunctiontype, double cccfloor, double cccceiling, double cccexp, double remapfactor, double remaplimit, bool softkneemode, double kneefactor, int mapdirection, int safezonetype, bool spiralcarisma, bool eilutmode, bool nesmode){
    vec3 linearinputcolor = inputcolor;
    if (sourcegamut.crtemumode == CRT_EMU_FRONT){
        if (eilutmode){
            linearinputcolor = sourcegamut.attachedCRT->tolinear1886appx1vec3(inputcolor);
        }
        else {
            linearinputcolor = sourcegamut.attachedCRT->CRTEmulateGammaSpaceRGBtoLinearRGB(inputcolor);
        }
    }
    else if (gammamodein){
        linearinputcolor = vec3(tolinear(inputcolor.x), tolinear(inputcolor.y), tolinear(inputcolor.z));
    }
    
    // Expanded intermediate LUTs expressly contain a ton of out-of-bounds colors
    // XYZtoJzazbz() will force colors to black if luminosity is negative at that point.
    // CUSP and VPR-alike GMA algorithms will pull down stuff that's above 1.0 luminosity.
    // But desaturation-only algorithms (HLPCM) must have luminosity clamped.
    // NES may also have out-of-bounds luminosity
    if ((eilutmode || nesmode) && (mapdirection == MAP_HLPCM)){
        linearinputcolor = sourcegamut.ClampLuminosity(linearinputcolor);
    }

    vec3 outcolor;
        
    if (mapmode == MAP_CLIP){
        vec3 tempcolor = sourcegamut.linearRGBtoXYZ(linearinputcolor);
        outcolor = destgamut.XYZtoLinearRGB(tempcolor);
    }
    else if (mapmode == MAP_CCC_A){
        // take weighted average of corrected and uncorrected color
        // based on YPrPgPb-space proximity to primary/secondary colors
        vec3 tempcolor = sourcegamut.linearRGBtoXYZ(linearinputcolor);
        outcolor = destgamut.XYZtoLinearRGB(tempcolor);
        double maxP = sourcegamut.linearRGBfindmaxP(linearinputcolor);
        double oldweight = 0.0;
        if (cccfunctiontype == CCC_EXPONENTIAL){
            oldweight = powermap(cccfloor, cccceiling, maxP, cccexp);
        }
        else if (cccfunctiontype == CCC_CUBIC_HERMITE){
            oldweight = cubichermitemap(cccfloor, cccceiling, maxP);
        }
        double newweight = 1.0 - oldweight;
        outcolor = (linearinputcolor * oldweight) + (outcolor * newweight);
    }
    else if ((mapmode == MAP_CCC_B) || (mapmode == MAP_CCC_C)){
        
        // apply the "Chunghwa" matrix
        vec3 corrected = multMatrixByColor(destgamut.matrixChunghwa, linearinputcolor);
        
        if (mapmode == MAP_CCC_C){
            vec3 tempcolor = sourcegamut.linearRGBtoXYZ(linearinputcolor);
            vec3 accurate = destgamut.XYZtoLinearRGB(tempcolor);
            double maxP = sourcegamut.linearRGBfindmaxP(linearinputcolor);
            double cccweight = cubichermitemap(0.0, 1.0, maxP);
            double accurateweight = 1.0 - cccweight;
            corrected = (corrected * cccweight) + (accurate * accurateweight); 
        }
        
        // we don't need to clamp here b/c the RGB8 quantizer will clamp for us later
        
        
        outcolor = corrected;
        
    }
    else if ((mapmode == MAP_CCC_D) || (mapmode == MAP_CCC_E)){
        
        // apply the appropriate Kinoshita matrix
        vec3 corrected = sourcegamut.KinoshitaMultiply(linearinputcolor);
        
        if (mapmode == MAP_CCC_C){
            vec3 tempcolor = sourcegamut.linearRGBtoXYZ(linearinputcolor);
            vec3 accurate = destgamut.XYZtoLinearRGB(tempcolor);
            double maxP = sourcegamut.linearRGBfindmaxP(linearinputcolor);
            double cccweight = cubichermitemap(0.0, 1.0, maxP);
            double accurateweight = 1.0 - cccweight;
            corrected = (corrected * cccweight) + (accurate * accurateweight); 
        }
        
        // we don't need to clamp here b/c the RGB8 quantizer will clamp for us later
        
        
        outcolor = corrected;
        
    }
    else {
        outcolor = mapColor(linearinputcolor, sourcegamut, destgamut, (mapmode == MAP_EXPAND), remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, nesmode);
    }
    if (destgamut.crtemumode == CRT_EMU_BACK){
        outcolor = destgamut.attachedCRT->CRTEmulateLinearRGBtoGammaSpaceRGB(outcolor);
    }
    else if (gammamodeout){
        outcolor = vec3(togamma(outcolor.x), togamma(outcolor.y), togamma(outcolor.z));
    }
    return outcolor;
}

// structs for holding our ever-growing list of parameters
typedef struct boolparam{
    std::string paramstring; // parameter's text
    std::string prettyname; // name for pretty printing
    bool* vartobind; // pointer to variable whose value to set
} boolparam;

typedef struct stringparam{
    std::string paramstring; // parameter's text
    std::string prettyname; // name for pretty printing
    char** vartobind;    // pointer to variable whose value to set
    bool* flagtobind; // pointer to flag to set when this variable is set
} stringparam;

typedef struct paramvalue{
    std::string valuestring; // parameter value's text
    int value;              // the value--p;.
} paramvalue;

typedef struct selectparam{
    std::string paramstring; // parameter's text
    std::string prettyname; // name for pretty printing
    int* vartobind; // pointer to variable whose value to set
    const paramvalue* valuetable; // pointer to table of possible values
    int tablesize; // number of items in the table
} selectparam;

typedef struct floatparam{
    std::string paramstring; // parameter's text
    std::string prettyname; // name for pretty printing
    double* vartobind; // pointer to variable whose value to set
} floatparam;

typedef struct intparam{
    std::string paramstring; // parameter's text
    std::string prettyname; // name for pretty printing
    int* vartobind; // pointer to variable whose value to set
} intparam;


int main(int argc, const char **argv){
    
    // ----------------------------------------------------------------------------------------
    // parameter processing

    // defaults
    bool helpmode = false;
    bool filemode = true;
    bool gammamodein = true;
    int gammamodeinalias = 1;
    bool gammamodeout = true;
    int gammamodeoutalias = 1;
    bool softkneemode = true;
    int softkneemodealias = 1;
    bool dither = true;
    int mapdirection = MAP_VPRC;
    int mapmode = MAP_COMPRESS;
    int sourcegamutindex = GAMUT_P22_TRINITRON;
    int destgamutindex = GAMUT_SRGB;
    int sourcewhitepointindex = WHITEPOINT_9300K27MPCD;
    int destwhitepointindex = WHITEPOINT_D65;
    int safezonetype = RMZONE_DELTA_BASED;
    char* inputfilename;
    char* outputfilename;
    char* inputcolorstring;
    vec3 inputcolor;
    bool infileset = false;
    bool outfileset = false;
    bool incolorset = false;
    double remapfactor = 0.4;
    double remaplimit = 0.9;
    double kneefactor = 0.4;
    int verbosity = VERBOSITY_SLIGHT;
    int adapttype = ADAPT_CAT16;
    int cccfunctiontype = CCC_EXPONENTIAL;
    double cccfloor = 0.5;
    double cccceiling = 0.95;
    double cccexp = 1.0;
    bool spiralcarisma = false;
    int scfunctiontype = SC_CUBIC_HERMITE;
    double scfloor = 0.7;
    double scceiling = 1.0;
    double scexp = 1.2;
    double scmax = 1.0;
    int crtemumode = CRT_EMU_NONE;
    double crtblacklevel = 0.0001;
    double crtwhitelevel = 1.71;
    int crtmodindex = CRT_MODULATOR_NONE;
    int crtdemodindex = CRT_DEMODULATOR_NONE;
    int crtdemodrenorm = RENORM_DEMOD_INSANE;
    bool crtdoclamphigh = false;
    double crtclamphigh = 1.2;
    double crtclamplow = -0.1;
    bool lutgen = false;
    bool eilut = false;
    int lutsize = 128;
    bool nesmode = false;
    bool nesispal = false;
    bool nescbc = true;
    double nesskew26A = 4.5;
    double nesboost48C = 1.0;
    double nesskewstep = 2.5;
    bool neswritehtml = false;
    char* neshtmlfilename;
    
    const boolparam params_bool[10] = {
        {
            "--dither",         //std::string paramstring; // parameter's text
            "Dithering",        //std::string prettyname; // name for pretty printing
            &dither             //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--di",             //std::string paramstring; // parameter's text
            "Dithering",        //std::string prettyname; // name for pretty printing
            &dither             //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--spiral-carisma",         //std::string paramstring; // parameter's text
            "Spiral CARISMA",           //std::string prettyname; // name for pretty printing
            &spiralcarisma             //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--sc",                     //std::string paramstring; // parameter's text
            "Spiral CARISMA",           //std::string prettyname; // name for pretty printing
            &spiralcarisma             //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--lutgen",                     //std::string paramstring; // parameter's text
            "LUT Generation",           //std::string prettyname; // name for pretty printing
            &lutgen                  //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--eilut",                     //std::string paramstring; // parameter's text
            "Expanded Intermediate LUT Generation",           //std::string prettyname; // name for pretty printing
            &eilut                  //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--nespalgen",                     //std::string paramstring; // parameter's text
            "NES Palette Generation",           //std::string prettyname; // name for pretty printing
            &nesmode                 //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--nespalmode",                     //std::string paramstring; // parameter's text
            "NES simulation PAL mode",           //std::string prettyname; // name for pretty printing
            &nesispal                 //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--nesburstnorm",                     //std::string paramstring; // parameter's text
            "NES simulation normalize chroma to colorburst",           //std::string prettyname; // name for pretty printing
            &nescbc                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtclamphighrgb",                     //std::string paramstring; // parameter's text
            "CRT Clamp High RGB Output Values",           //std::string prettyname; // name for pretty printing
            &crtdoclamphigh                //bool* vartobind; // pointer to variable whose value to set
        }
    };

    const stringparam params_string[7] = {
        {
            "--infile",             //std::string paramstring; // parameter's text
            "Input Filename",       //std::string prettyname; // name for pretty printing
            &inputfilename,         //char** vartobind;    // pointer to variable whose value to set
            &infileset              //bool* flagtobind; // pointer to flag to set when this variable is set
        },
        {
            "-i",             //std::string paramstring; // parameter's text
            "Input Filename",       //std::string prettyname; // name for pretty printing
            &inputfilename,         //char** vartobind;    // pointer to variable whose value to set
            &infileset              //bool* flagtobind; // pointer to flag to set when this variable is set
        },
        {
            "--outfile",             //std::string paramstring; // parameter's text
            "Output Filename",       //std::string prettyname; // name for pretty printing
            &outputfilename,         //char** vartobind;    // pointer to variable whose value to set
            &outfileset              //bool* flagtobind; // pointer to flag to set when this variable is set
        },
        {
            "-o",             //std::string paramstring; // parameter's text
            "Output Filename",       //std::string prettyname; // name for pretty printing
            &outputfilename,         //char** vartobind;    // pointer to variable whose value to set
            &outfileset              //bool* flagtobind; // pointer to flag to set when this variable is set
        },
        {
            "--neshtmloutputfile",             //std::string paramstring; // parameter's text
            "NES Palette HTML Output Filename",       //std::string prettyname; // name for pretty printing
            &neshtmlfilename,         //char** vartobind;    // pointer to variable whose value to set
            &neswritehtml              //bool* flagtobind; // pointer to flag to set when this variable is set
        },
        {
            "--color",             //std::string paramstring; // parameter's text
            "Input Color",       //std::string prettyname; // name for pretty printing
            &inputcolorstring,         //char** vartobind;    // pointer to variable whose value to set
            &incolorset             //bool* flagtobind; // pointer to flag to set when this variable is set
        },
        {
            "-c",             //std::string paramstring; // parameter's text
            "Input Color",       //std::string prettyname; // name for pretty printing
            &inputcolorstring,         //char** vartobind;    // pointer to variable whose value to set
            &incolorset             //bool* flagtobind; // pointer to flag to set when this variable is set
        }

    };

    const paramvalue gamutlist[8] = {
        {
            "srgb_spec",
            GAMUT_SRGB
        },
        {
            "ntsc_spec",
            GAMUT_NTSC
        },
        {
            "smptec_spec",
            GAMUT_SMPTEC
        },
        {
            "ebu_spec",
            GAMUT_EBU
        },
        {
            "P22_average",
            GAMUT_P22_AVERAGE
        },
        {
            "P22_trinitron",
            GAMUT_P22_TRINITRON
        },
        {
            "P22_ebuish",
            GAMUT_P22_EBUISH
        },
        {
            "P22_hitachi",
            GAMUT_P22_HITACHI
        }
    };

    const paramvalue whitepointlist[4] = {
        {
            "D65",
            WHITEPOINT_D65
        },
        {
            "9300K27mpcd",
            WHITEPOINT_9300K27MPCD
        },
        {
            "9300K8mpcd",
            WHITEPOINT_9300K8MPCD
        },
        {
            "illuminantC",
            WHITEPOINT_ILLUMINANTC
        }
    };

    const paramvalue mapmodelist[3] = {
        {
            "clip",
            MAP_CLIP
        },
        {
            "compress",
            MAP_COMPRESS
        },
        {
            "expand",
            MAP_EXPAND
        }
        // Intentionally omitting ccca/cccb/cccc/cccd/ccce derived from patent filings
        // because they suck and there's no evidence they were ever used for CRTs (or anything else).
        // Leaving them on the backend in case they ever prove useful in the future.
    };

    const paramvalue gmalist[5] = {
        {
            "cusp",
            MAP_GCUSP
        },
        {
            "hlpcm",
            MAP_HLPCM
        },
        {
            "vp",
            MAP_VP
        },
        {
            "vpr",
            MAP_VPR
        },
        {
            "vprc",
            MAP_VPRC
        }
    };

    const paramvalue safezonelist[2] = {
        {
            "const-detail",
            RMZONE_DELTA_BASED
        },
        {
            "const-fidelity",
            RMZONE_DEST_BASED
        },
    };

    const paramvalue kneetypelist[2] = {
        {
            "hard",
            0
        },
        {
            "soft",
            1
        },
    };

    const paramvalue gammatypelist[2] = {
        {
            "linear",
            0
        },
        {
            "srgb",
            1
        },
    };

    const paramvalue adapttypelist[2] = {
        {
            "bradford",
            ADAPT_BRADFORD
        },
        {
            "cat16",
            ADAPT_CAT16
        },
    };

    const paramvalue scfunclist[2] = {
        {
            "exponential",
            SC_EXPONENTIAL
        },
        {
            "cubichermite",
            SC_CUBIC_HERMITE
        },
    };

    const paramvalue crtmodelist[3] = {
        {
            "none",
            CRT_EMU_NONE
        },
        {
            "front",
            CRT_EMU_FRONT
        },
        {
            "back",
            CRT_EMU_BACK
        }
    };

    const paramvalue crtmodulatorlist[3] = {
        {
            "none",
            CRT_MODULATOR_NONE
        },
        {
            "CXA1145",
            CRT_MODULATOR_CXA1145
        },
        {
            "CXA1645",
            CRT_MODULATOR_CXA1645
        }
    };

    const paramvalue crtdemodulatorlist[12] = {
        {
            "none",
            CRT_DEMODULATOR_NONE
        },
        {
            "dummy",
            CRT_DEMODULATOR_DUMMY
        },
        {
            "CXA1464AS",
            CRT_DEMODULATOR_CXA1464AS
        },
        {
            "CXA1465AS",
            CRT_DEMODULATOR_CXA1465AS
        },
        {
            "CXA1870S_JP",
            CRT_DEMODULATOR_CXA1870S_JP
        },
        {
            "CXA1870S_US",
            CRT_DEMODULATOR_CXA1870S_US
        },
        {
            "CXA2060BS_JP",
            CRT_DEMODULATOR_CXA2060BS_JP
        },
        {
            "CXA2060BS_US",
            CRT_DEMODULATOR_CXA2060BS_US
        },
        {
            "CXA2025AS_JP",
            CRT_DEMODULATOR_CXA2025AS_JP
        },
        {
            "CXA2025AS_US",
            CRT_DEMODULATOR_CXA2025AS_US
        },
        {
            "CXA1213AS",
            CRT_DEMODULATOR_CXA1213AS
        },
        {
            "TDA8362",
            CRT_DEMODULATOR_TDA8362
        }
    };

    const paramvalue demodrenormlist[5] = {
        {
            "none",
            RENORM_DEMOD_NONE
        },
        {
            "insane",
            RENORM_DEMOD_INSANE
        },
        {
            "nonzeroangle",
            RENORM_DEMOD_ANGLE_NOT_ZERO
        },
        {
            "gainnot1",
            RENORM_DEMOD_GAIN_NOT_ONE
        },
        {
            "all",
            RENORM_DEMOD_ANY
        }
    };

    const selectparam params_select[28] = {
        {
            "--source-primaries",            //std::string paramstring; // parameter's text
            "Source Primaries",             //std::string prettyname; // name for pretty printing
            &sourcegamutindex,          //int* vartobind; // pointer to variable whose value to set
            gamutlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gamutlist)/sizeof(gamutlist[0])     //int tablesize; // number of items in the table
        },
        {
            "-s",            //std::string paramstring; // parameter's text
            "Source Primaries",             //std::string prettyname; // name for pretty printing
            &sourcegamutindex,          //int* vartobind; // pointer to variable whose value to set
            gamutlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gamutlist)/sizeof(gamutlist[0]) //int tablesize; // number of items in the table
        },
        {
            "--dest-primaries",            //std::string paramstring; // parameter's text
            "Destination Primaries",             //std::string prettyname; // name for pretty printing
            &destgamutindex,          //int* vartobind; // pointer to variable whose value to set
            gamutlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gamutlist)/sizeof(gamutlist[0]) //int tablesize; // number of items in the table
        },
        {
            "-d",            //std::string paramstring; // parameter's text
            "Destination Primaries",             //std::string prettyname; // name for pretty printing
            &destgamutindex,          //int* vartobind; // pointer to variable whose value to set
            gamutlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gamutlist)/sizeof(gamutlist[0])  //int tablesize; // number of items in the table
        },
        {
            "--source-whitepoint",            //std::string paramstring; // parameter's text
            "Source Whitepoint",             //std::string prettyname; // name for pretty printing
            &sourcewhitepointindex,          //int* vartobind; // pointer to variable whose value to set
            whitepointlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(whitepointlist)/sizeof(whitepointlist[0]) //int tablesize; // number of items in the table
        },
        {
            "--sw",            //std::string paramstring; // parameter's text
            "Source Whitepoint",             //std::string prettyname; // name for pretty printing
            &sourcewhitepointindex,          //int* vartobind; // pointer to variable whose value to set
            whitepointlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(whitepointlist)/sizeof(whitepointlist[0]) //int tablesize; // number of items in the table
        },
        {
            "--dest-whitepoint",            //std::string paramstring; // parameter's text
            "Destination Whitepoint",             //std::string prettyname; // name for pretty printing
            &destwhitepointindex,          //int* vartobind; // pointer to variable whose value to set
            whitepointlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(whitepointlist)/sizeof(whitepointlist[0]) //int tablesize; // number of items in the table
        },
        {
            "--dw",            //std::string paramstring; // parameter's text
            "Destination Whitepoint",             //std::string prettyname; // name for pretty printing
            &sourcewhitepointindex,          //int* vartobind; // pointer to variable whose value to set
            whitepointlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(whitepointlist)/sizeof(whitepointlist[0]) //int tablesize; // number of items in the table
        },
        {
            "--map-mode",            //std::string paramstring; // parameter's text
            "Map Mode",             //std::string prettyname; // name for pretty printing
            &mapmode,          //int* vartobind; // pointer to variable whose value to set
            mapmodelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(mapmodelist)/sizeof(mapmodelist[0])  //int tablesize; // number of items in the table
        },
        {
            "-m",            //std::string paramstring; // parameter's text
            "Map Mode",             //std::string prettyname; // name for pretty printing
            &mapmode,          //int* vartobind; // pointer to variable whose value to set
            mapmodelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(mapmodelist)/sizeof(mapmodelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--gamut-mapping-algorithm",            //std::string paramstring; // parameter's text
            "Gamut Mapping Algorithm",             //std::string prettyname; // name for pretty printing
            &mapdirection,          //int* vartobind; // pointer to variable whose value to set
            gmalist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gmalist)/sizeof(gmalist[0])  //int tablesize; // number of items in the table
        },
        {
            "--gma",            //std::string paramstring; // parameter's text
            "Gamut Mapping Algorithm",             //std::string prettyname; // name for pretty printing
            &mapdirection,          //int* vartobind; // pointer to variable whose value to set
            gmalist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gmalist)/sizeof(gmalist[0])  //int tablesize; // number of items in the table
        },
        {
            "--safe-zone-type",            //std::string paramstring; // parameter's text
            "Gamut Compression Safe Zone Type",             //std::string prettyname; // name for pretty printing
            &safezonetype,          //int* vartobind; // pointer to variable whose value to set
            safezonelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(safezonelist)/sizeof(safezonelist[0])  //int tablesize; // number of items in the table
        },
        {
            "-z",            //std::string paramstring; // parameter's text
            "Gamut Compression Safe Zone Type",             //std::string prettyname; // name for pretty printing
            &safezonetype,          //int* vartobind; // pointer to variable whose value to set
            safezonelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(safezonelist)/sizeof(safezonelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--knee",            //std::string paramstring; // parameter's text
            "Gamut Compression Knee Type",             //std::string prettyname; // name for pretty printing
            &softkneemodealias,          //int* vartobind; // pointer to variable whose value to set
            kneetypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(kneetypelist)/sizeof(kneetypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "-k",            //std::string paramstring; // parameter's text
            "Gamut Compression Knee Type",             //std::string prettyname; // name for pretty printing
            &softkneemodealias,          //int* vartobind; // pointer to variable whose value to set
            kneetypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(kneetypelist)/sizeof(kneetypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--gamma-in",            //std::string paramstring; // parameter's text
            "Input/Output Gamma Function",             //std::string prettyname; // name for pretty printing
            &gammamodeinalias,          //int* vartobind; // pointer to variable whose value to set
            gammatypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gammatypelist)/sizeof(gammatypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--gin",            //std::string paramstring; // parameter's text
            "Input/Output Gamma Function",             //std::string prettyname; // name for pretty printing
            &gammamodeinalias,          //int* vartobind; // pointer to variable whose value to set
            gammatypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gammatypelist)/sizeof(gammatypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--gamma-out",            //std::string paramstring; // parameter's text
            "Input/Output Gamma Function",             //std::string prettyname; // name for pretty printing
            &gammamodeoutalias,          //int* vartobind; // pointer to variable whose value to set
            gammatypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gammatypelist)/sizeof(gammatypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--gout",            //std::string paramstring; // parameter's text
            "Input/Output Gamma Function",             //std::string prettyname; // name for pretty printing
            &gammamodeoutalias,          //int* vartobind; // pointer to variable whose value to set
            gammatypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gammatypelist)/sizeof(gammatypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--adapt",            //std::string paramstring; // parameter's text
            "Chromatic Adaptation Algorithm",             //std::string prettyname; // name for pretty printing
            &adapttype,          //int* vartobind; // pointer to variable whose value to set
            adapttypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(adapttypelist)/sizeof(adapttypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "-a",            //std::string paramstring; // parameter's text
            "Chromatic Adaptation Algorithm",             //std::string prettyname; // name for pretty printing
            &adapttype,          //int* vartobind; // pointer to variable whose value to set
            adapttypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(adapttypelist)/sizeof(adapttypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--scfunction",            //std::string paramstring; // parameter's text
            "Spiral CARISMA Interpolation Function",             //std::string prettyname; // name for pretty printing
            &scfunctiontype,          //int* vartobind; // pointer to variable whose value to set
            scfunclist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(scfunclist)/sizeof(scfunclist[0])  //int tablesize; // number of items in the table
        },
        {
            "--scf",            //std::string paramstring; // parameter's text
            "Spiral CARISMA Interpolation Function",             //std::string prettyname; // name for pretty printing
            &scfunctiontype,          //int* vartobind; // pointer to variable whose value to set
            scfunclist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(scfunclist)/sizeof(scfunclist[0])  //int tablesize; // number of items in the table
        },
        {
            "--crtemu",            //std::string paramstring; // parameter's text
            "CRT Emulation Mode",             //std::string prettyname; // name for pretty printing
            &crtemumode,          //int* vartobind; // pointer to variable whose value to set
            crtmodelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(crtmodelist)/sizeof(crtmodelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--crtmod",            //std::string paramstring; // parameter's text
            "Game Console Modulator Chip",             //std::string prettyname; // name for pretty printing
            &crtmodindex,          //int* vartobind; // pointer to variable whose value to set
            crtmodulatorlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(crtmodulatorlist)/sizeof(crtmodulatorlist[0])  //int tablesize; // number of items in the table
        },
        {
            "--crtdemod",            //std::string paramstring; // parameter's text
            "CRT Demodulator Chip",             //std::string prettyname; // name for pretty printing
            &crtdemodindex,          //int* vartobind; // pointer to variable whose value to set
            crtdemodulatorlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(crtdemodulatorlist)/sizeof(crtdemodulatorlist[0])  //int tablesize; // number of items in the table
        },
        {
            "--crtdemodrenorm",            //std::string paramstring; // parameter's text
            "Renormalize CRT Demodulator Chip Condition",             //std::string prettyname; // name for pretty printing
            &crtdemodrenorm,          //int* vartobind; // pointer to variable whose value to set
            demodrenormlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(demodrenormlist)/sizeof(demodrenormlist[0])  //int tablesize; // number of items in the table
        }
    };


    const floatparam params_float[21] = {
        {
            "--remap-factor",         //std::string paramstring; // parameter's text
            "Gamut Compression Remap Factor",        //std::string prettyname; // name for pretty printing
            &remapfactor            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--rf",         //std::string paramstring; // parameter's text
            "Gamut Compression Remap Factor",        //std::string prettyname; // name for pretty printing
            &remapfactor            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--remap-limit",         //std::string paramstring; // parameter's text
            "Gamut Compression Remap Limit",        //std::string prettyname; // name for pretty printing
            &remaplimit            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--rl",         //std::string paramstring; // parameter's text
            "Gamut Compression Remap Limit",        //std::string prettyname; // name for pretty printing
            &remaplimit            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--knee-factor",         //std::string paramstring; // parameter's text
            "Gamut Compression Knee Factor",        //std::string prettyname; // name for pretty printing
            &kneefactor            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--kf",         //std::string paramstring; // parameter's text
            "Gamut Compression Knee Factor",        //std::string prettyname; // name for pretty printing
            &kneefactor            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--scfloor",         //std::string paramstring; // parameter's text
            "Spiral CARISMA interpolation floor",        //std::string prettyname; // name for pretty printing
            &scfloor             //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--scfl",         //std::string paramstring; // parameter's text
            "Spiral CARISMA interpolation floor",        //std::string prettyname; // name for pretty printing
            &scfloor             //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--scceiling",         //std::string paramstring; // parameter's text
            "Spiral CARISMA interpolation ceiling",        //std::string prettyname; // name for pretty printing
            &scceiling            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--sccl",         //std::string paramstring; // parameter's text
            "Spiral CARISMA interpolation ceiling",        //std::string prettyname; // name for pretty printing
            &scceiling            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--scexponent",         //std::string paramstring; // parameter's text
            "Spiral CARISMA interpolation exponent",        //std::string prettyname; // name for pretty printing
            &scexp            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--scxp",         //std::string paramstring; // parameter's text
            "Spiral CARISMA interpolation exponent",        //std::string prettyname; // name for pretty printing
            &scexp            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--scmax",         //std::string paramstring; // parameter's text
            "Spiral CARISMA Max Strength",        //std::string prettyname; // name for pretty printing
            &scmax            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--scm",         //std::string paramstring; // parameter's text
            "Spiral CARISMA Max Strength",        //std::string prettyname; // name for pretty printing
            &scmax            //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtblack",         //std::string paramstring; // parameter's text
            "CRT Black Level (100x cd/m^2)",        //std::string prettyname; // name for pretty printing
            &crtblacklevel           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtwhite",         //std::string paramstring; // parameter's text
            "CRT White Level (100x cd/m^2)",        //std::string prettyname; // name for pretty printing
            &crtwhitelevel           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--nesskew26a",         //std::string paramstring; // parameter's text
            "NES hue 2/6/A phase skew (degrees)",        //std::string prettyname; // name for pretty printing
            &nesskew26A           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--nesboost48c",         //std::string paramstring; // parameter's text
            "NES hue 4/8/C luma boost (IRE)",        //std::string prettyname; // name for pretty printing
            &nesboost48C           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--nesperlumaskew",         //std::string paramstring; // parameter's text
            "NES per luma phase skew (degrees)",        //std::string prettyname; // name for pretty printing
            &nesskewstep           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtclamphigh",         //std::string paramstring; // parameter's text
            "CRT RGB Output Clamp High",        //std::string prettyname; // name for pretty printing
            &crtclamphigh         //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtclamplow",         //std::string paramstring; // parameter's text
            "CRT RGB Output Clamp Low",        //std::string prettyname; // name for pretty printing
            &crtclamplow           //double* vartobind; // pointer to variable whose value to set
        }


        // Intentionally omitting cccfloor, cccceiling, cccexp for color correction methods derived from patent filings
        // because they suck and there's no evidence they were ever used for CRTs (or anything else).
        // Leaving them on the backend in case they ever prove useful in the future.
    };

    const intparam params_int[3] = {
        {
            "--verbosity",         //std::string paramstring; // parameter's text
            "Verbosity",        //std::string prettyname; // name for pretty printing
            &verbosity            //int* vartobind; // pointer to variable whose value to set
        },
        {
            "-v",         //std::string paramstring; // parameter's text
            "Verbosity",        //std::string prettyname; // name for pretty printing
            &verbosity            //int* vartobind; // pointer to variable whose value to set
        },
        {
            "--lutsize",         //std::string paramstring; // parameter's text
            "LUT Size",        //std::string prettyname; // name for pretty printing
            &lutsize            //int* vartobind; // pointer to variable whose value to set
        },
    };

    int nextparamtype = 0;
    int listsize = 0;
    int lastj = 0;
    bool* nextboolptr = nullptr;
    char** nextstringptr = nullptr;
    int* nextintptr = nullptr;
    const paramvalue* nexttable = nullptr;
    int nexttablesize = 0;
    double* nextfloatptr = nullptr;
    bool selectfound = false;
    bool breakout = false;
    for (int i=1; i<argc; i++){
        switch (nextparamtype){
            // expecting a parameter switch
            case 0:
                // rest temp vars
                listsize = 0;
                lastj = 0;
                nextboolptr = nullptr;
                nextstringptr = nullptr;
                nextintptr = nullptr;
                nexttable = nullptr;
                nexttablesize = 0;
                nextfloatptr = nullptr;
                selectfound = false;
                breakout = false;

                // iterate over each param type and check them
                listsize = sizeof(params_bool)/sizeof(params_bool[0]);
                for (int j=0; j<listsize; j++){
                    if (strcmp(argv[i], params_bool[j].paramstring.c_str()) == 0){
                        nextboolptr = params_bool[j].vartobind;
                        nextparamtype = 1;
                        lastj = j;
                        breakout = true;
                        break;
                    }
                }
                if (breakout){break;}

                listsize = sizeof(params_string)/sizeof(params_string[0]);
                for (int j=0; j<listsize; j++){
                    if (strcmp(argv[i],params_string[j].paramstring.c_str()) == 0){
                        nextstringptr = params_string[j].vartobind;
                        nextboolptr = params_string[j].flagtobind;
                        nextparamtype = 2;
                        lastj = j;
                        breakout = true;
                        break;
                    }
                }
                if (breakout){break;}

                listsize = sizeof(params_select)/sizeof(params_select[0]);
                for (int j=0; j<listsize; j++){
                    if (strcmp(argv[i],params_select[j].paramstring.c_str()) == 0){
                        nextintptr = params_select[j].vartobind;
                        nexttable = params_select[j].valuetable;
                        nexttablesize = params_select[j].tablesize;
                        nextparamtype = 3;
                        lastj = j;
                        breakout = true;
                        break;
                    }
                }
                if (breakout){break;}

                listsize = sizeof(params_int)/sizeof(params_int[0]);
                for (int j=0; j<listsize; j++){
                    if (strcmp(argv[i], params_int[j].paramstring.c_str()) == 0){
                        nextintptr = params_int[j].vartobind;
                        nextparamtype = 4;
                        lastj = j;
                        breakout = true;
                        break;
                    }
                }
                if (breakout){break;}

                listsize = sizeof(params_float)/sizeof(params_float[0]);
                for (int j=0; j<listsize; j++){
                    if (strcmp(argv[i], params_float[j].paramstring.c_str()) == 0){
                        nextfloatptr = params_float[j].vartobind;
                        nextparamtype = 5;
                        lastj = j;
                        breakout = true;
                        break;
                    }
                }
                if (breakout){break;}

                // check for help flag
                if (strcmp(argv[i], "-h") == 0){
                    helpmode = true;
                    break;
                }
                else if (strcmp(argv[i], "--help") == 0){
                    helpmode = true;
                    break;
                }

                // if we have not broken out by here, we have an unknown parameter
                printf("Invalid parameter: \"%s\"\n", argv[i]);
                return ERROR_BAD_PARAM_UNKNOWN_PARAM;

                break;
            // expecting a bool param
            case 1:
                if (strcmp(argv[i], "true") == 0){
                    *nextboolptr = true;
                }
                else if (strcmp(argv[i], "false") == 0){
                    *nextboolptr = false;
                }
                else {
                    printf("Invalid value for parameter %s (%s). Expecting \"true\" or \"false\".\n", params_bool[lastj].paramstring.c_str(), params_bool[lastj].prettyname.c_str());
                    return ERROR_BAD_PARAM_BOOL;
                }
                nextparamtype = 0;
                break;
            // expecting a string param
            case 2:
                *nextstringptr = const_cast<char*>(argv[i]);
                if (nextboolptr != nullptr){
                    *nextboolptr = true;
                }
                nextparamtype = 0;
                break;
            // expecting to select a string from a table
            case 3:
                selectfound = false;
                for (int listindex=0; listindex<nexttablesize; listindex++){
                    if (strcmp(argv[i], nexttable[listindex].valuestring.c_str()) == 0){
                        *nextintptr = nexttable[listindex].value;
                        selectfound = true;
                        break;
                    }
                }
                if (!selectfound){
                    printf("Invalid value for parameter %s (%s). Expecting ", params_select[lastj].paramstring.c_str(), params_select[lastj].prettyname.c_str());
                    for (int listindex=0; listindex<nexttablesize; listindex++){
                        if (listindex == nexttablesize - 1){
                            printf(" or \"%s\".\n", nexttable[listindex].valuestring.c_str());
                        }
                        else {
                            printf(" \"%s\",", nexttable[listindex].valuestring.c_str());
                        }
                    }
                    return ERROR_BAD_PARAM_SELECT;
                }
                nextparamtype = 0;
                break;
            // expecting a int param
            case 4:
                {
                    char* endptr;
                    errno = 0; //make sure errno is 0 before strtol()
                    long int input = strtol(argv[i], &endptr, 0);
                    bool inputok = true;
                    // are there any chacters left in the input string?
                    if (*endptr != '\0'){
                        inputok = false;
                    }
                    // is errno set?
                    else if (errno != 0){
                        inputok = false;
                    }
                    if (inputok){
                        *nextintptr = input;
                    }
                    else {
                        printf("Invalid value for parameter %s (%s). Expecting integer numerical value.", params_int[lastj].paramstring.c_str(), params_int[lastj].prettyname.c_str());
                        return ERROR_BAD_PARAM_INT;
                    }
                    nextparamtype = 0;
                    break;
                }
            // expecting an int param
            case 5:
                {
                    char* endptr;
                    errno = 0; //make sure errno is 0 before strtol()
                    double input = strtod(argv[i], &endptr);
                    bool inputok = true;
                    // are there any chacters left in the input string?
                    if (*endptr != '\0'){
                        inputok = false;
                    }
                    // is errno set?
                    else if (errno != 0){
                        inputok = false;
                    }
                    if (inputok){
                        *nextfloatptr = input;
                    }
                    else {
                        printf("Invalid value for parameter %s (%s). Expecting floating-point numerical value.", params_float[lastj].paramstring.c_str(), params_float[lastj].prettyname.c_str());
                        return ERROR_BAD_PARAM_FLOAT;
                    }
                    nextparamtype = 0;
                    break;
                }
            default:
                break;
        };
    } // end for (int i=1; i<argc; i++)

    // if we reached the end of the parameter list and didn't end on nextparamtype == 0, then we're missing the final parameter.
    if (nextparamtype != 0){
        switch (nextparamtype){
            case 1:
                printf("Missing value for parameter %s (%s). Expecting \"true\" or \"false\".\n", params_bool[lastj].paramstring.c_str(), params_bool[lastj].prettyname.c_str());
                break;
            case 2:
                printf("Missing value for parameter %s (%s). Expecting text string.\n", params_string[lastj].paramstring.c_str(), params_string[lastj].prettyname.c_str());
                break;
            case 3:
                printf("Missing value for parameter %s (%s). Expecting ", params_select[lastj].paramstring.c_str(), params_select[lastj].prettyname.c_str());
                for (int listindex=0; listindex<nexttablesize; listindex++){
                    if (listindex == nexttablesize - 1){
                        printf(" or \"%s\".\n", nexttable[listindex].valuestring.c_str());
                    }
                    else {
                        printf(" \"%s\",", nexttable[listindex].valuestring.c_str());
                    }
                }
                break;
            case 4:
                printf("Missing value for parameter %s (%s). Expecting integer numerical value.\n", params_int[lastj].paramstring.c_str(), params_int[lastj].prettyname.c_str());
                break;
            case 5:
               printf("Missing value for parameter %s (%s). Expecting floating-point numerical value.\n", params_float[lastj].paramstring.c_str(), params_float[lastj].prettyname.c_str());
               break;
            default:
                break;
        };
        return ERROR_BAD_PARAM_MISSING_VALUE;
    }

    // -------------------------------------------------------------------
    // Sanity check the parameters and make changes if needed
    
    // if helpmode, show help then bail
    if (helpmode){
        printhelp();
        return RETURN_SUCCESS;
    }


    softkneemode = (softkneemodealias == 0) ? false : true;
    gammamodein = (gammamodeinalias == 0) ? false : true;
    gammamodeout = (gammamodeoutalias == 0) ? false : true;

    if (verbosity < VERBOSITY_SILENT){
        verbosity = VERBOSITY_SILENT;
    }
    if (verbosity > VERBOSITY_EXTREME){
        verbosity = VERBOSITY_EXTREME;
    }

    // single color mode should override other input modes
    // (and must, b/c filemode is true by default
    if (incolorset){
        if (filemode){
            filemode = false;
            if (infileset){
                printf("\nIgnoring input file because single-color input mode specified.\n");
            }
        }
        if (lutgen){
            printf("\nForcing lutgen to false because single-color input mode specified.\n");
            lutgen = false;
        }
        if (nesmode){
            printf("\nForcing lutgen to false because single-color input mode specified.\n");
            nesmode = false;
        }
    }

    if (nesmode && lutgen){
        printf("\nForcing lutgen to false because nespalgen is true.\n");
        lutgen = false;
    }
    if (nesmode && filemode){
        if (infileset){
            printf("\nIgnoring input file because nespalgen is true.\n");
            infileset = false;
        }
        filemode = false;
    }
    if (nesmode && spiralcarisma){
        printf("\nForcing spiral-carisma to false because nespalgen is true.\n");
        spiralcarisma = false;
    }
    if (!nesmode && neswritehtml){
        neswritehtml = false;
    }
    if (nesmode && (crtemumode != CRT_EMU_FRONT)){
        printf("\nForcing crtemu to front because nespalgen is true.\n");
        crtemumode = CRT_EMU_FRONT;
        if (crtdemodindex == CRT_DEMODULATOR_NONE){
            printf("\nForcing crt demodulator to dummy because none specifed.\n");
            crtdemodindex = CRT_DEMODULATOR_DUMMY;
        }
    }
    if (nesmode && (crtmodindex != CRT_MODULATOR_NONE)){
        printf("\nForcing crt modulator to none because nespalgen is true.\n");
        crtmodindex = CRT_MODULATOR_NONE;
    }

    if (lutgen){
        if (lutsize < 2){
            lutsize = 2;
            printf("\nWARNING: LUT size cannot be less than 2. Changing to 2.\n");
        }
        else if (lutsize > 128){
            printf("\nWARNING: LUT size is %i. Some programs, e.g. retroarch, cannot handle extra-large LUTs.\n", lutsize);
        }
        if (infileset){
            printf("\nIgnoring input file because lutgen is true.\n");
            infileset = false;
        }
    }

    if (eilut && !lutgen){
        printf("\nForcing eilut to false because lutgen is false.\n");
        eilut = false;
    }

    if (filemode || lutgen || nesmode){
        bool failboat = false;
        if (!infileset && !lutgen && !nesmode){
            printf("Input file not specified.\n");
            failboat = true;
        }
        if (!outfileset){
            printf("Output file not specified.\n");
            failboat = true;
        }
        if (failboat){
            return ERROR_BAD_PARAM_FILE_NOT_SPECIFIED;
        }
        if (lutgen){
            filemode = true; // piggyback LUT generation on the file i/o
            if (dither){
                printf("\nForcing dither to false because lutgen is true.\n");
                dither = false;
            }
            if (gammamodeout){
                printf("\nNOTE: You are saving a LUT using sRGB gamma. The program using this LUT will have to linearize values before interpolating. Are you sure this is what you want?\n");
            }
        }
        if (nesmode && dither){
            printf("\nForcing dither to false because nespalgen is true.\n");
            dither = false;
        }
    }
    else{
        if (!incolorset){
            printf("Input color not specified.\n");
            return ERROR_BAD_PARAM_COLOR_NOT_SPECIFIED;
        }
        char* endptr;
        errno = 0; //make sure errno is 0 before strtol()
        long int input = strtol(inputcolorstring, &endptr, 0);
        bool inputok = true;
        // did we consume exactly 8 characters?
        if (endptr - inputcolorstring != 8){
            inputok = false;
        }
        // are there any chacters left in the input string?
        else if (*endptr != '\0'){
            inputok = false;
        }
        // is errno set?
        else if (errno != 0){
            inputok = false;
        }
        if (!inputok){
            printf("Invalid input color. Format is  \"0xRRGGBB\" a 0x-prefixed hexadecimal representation of a RGB8 pixel value.\n");
            return ERROR_BAD_PARAM_INVALID_COLOR;
        }
        inputcolor.x = (input >> 16) / 255.0;
        inputcolor.y = ((input & 0x0000FF00) >> 8) / 255.0;
        inputcolor.z = (input & 0x000000FF) / 255.0;
    }

    if (crtclamphigh < 1.0){
        crtclamphigh = 1.0;
        printf("Cannot clamp CRT R'G'B' high output below 100%%; clamping to 100%% instead.\n");
    }
    if (crtclamplow > 0.0){
        crtclamplow = 0.0;
        printf("Cannot clamp CRT R'G'B' low output above 0%%; clamping to 0%% instead.\n");
    }
    else if (crtclamplow < -0.1){
        crtclamplow = -0.1;
        printf("CRT R'G'B' low output clamp must be at least -0.1; clamping to -0.1 instead.\n");
    }

    
    // ---------------------------------------------------------------------
    // Screen barf the params in verbose mode
    // TODO: refactor this

    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n\n----------\nParameters are:\n");
        if (filemode){
            if (lutgen){
                printf("LUT generation.\nLUT size: %i\nOutput file: %s\n", lutsize, outputfilename);
                if (eilut){
                    printf("LUT type: Expanded intermediate LUT\n");
                }
                else {
                    printf("LUT type: Normal LUT\n");
                }
            }
            else {
                printf("Input file: %s\nOutput file: %s\n", inputfilename, outputfilename);
            }
        }
        else if (nesmode){
            printf("NES palette generation.\nOutput file: %s\n", outputfilename);
            if (neswritehtml){
                printf("HTML output file: %s\n", neshtmlfilename);
            }
        }
        else {
            printf("Input color: %s\n", inputcolorstring);
        }
        if (gammamodein){
            printf("Input Gamma function: srgb\n");
        }
        else {
            printf("Input Gamma function: linear\n");
        }
        if (gammamodeout){
            printf("Output Gamma function: srgb\n");
        }
        else {
            printf("Output Gamma function: linear\n");
        }
        printf("Source primaries: %s\n", gamutnames[sourcegamutindex].c_str());
        printf("Source whitepoint: %s\n", whitepointnames[sourcewhitepointindex].c_str());
        printf("Destination primaries: %s\n", gamutnames[destgamutindex].c_str());
        printf("Destination whitepoint: %s\n", whitepointnames[destwhitepointindex].c_str());

        printf("Gamut mapping mode: ");
        switch(mapmode){
            case MAP_CLIP:
                printf("clip\n");
                break;
            case MAP_CCC_A:
                printf("pseudo color correction circuit A\n");
                break;
            case MAP_COMPRESS:
                printf("compress\n");
                break;
            case MAP_EXPAND:
                printf("expand\n");
                break;
            default:
                break;
        };
        if (mapmode >= MAP_FIRST_COMPRESS){
            printf("Gamut mapping algorithm: ");
            switch(mapdirection){
                case MAP_GCUSP:
                    printf("cusp\n");
                    break;
                case MAP_HLPCM:
                    printf("hlpcm\n");
                    break;
                case MAP_VP:
                    printf("vp\n");
                    break;
                 case MAP_VPR:
                    printf("vpr\n");
                    break;
                case MAP_VPRC:
                    printf("vprc\n");
                    break;
                default:
                    break;
            };
            printf("Safe zone type: ");
            switch(safezonetype){
                case RMZONE_DELTA_BASED:
                    printf("const-detail\n");
                    break;
                case RMZONE_DEST_BASED:
                    printf("const-fidelity\n");
                    break;
                default:
                    break;
            };
            if (safezonetype == RMZONE_DELTA_BASED){
                printf("Remap Factor: %f\n", remapfactor);
            }
            printf("Remap Limit: %f\n", remaplimit);
            if (softkneemode){
                printf("Knee type: soft\nKnee factor: %f\n", kneefactor);
            }
            else{
                printf("Knee type: hard\n");
            }
        }
        if (filemode){
            if (dither){
                printf("Dither: true\n");
            }
            else {
                printf("Dither: false\n");
            }
        }
        printf("Chromatic adapation type: ");
        switch(adapttype){
            case ADAPT_BRADFORD:
                printf("Bradford\n");
                break;
            case ADAPT_CAT16:
                printf("CAT16\n");
                break;
            default:
                break;
        };
        if (mapmode == MAP_CCC_A){
            printf("CCC function type: ");
            switch(cccfunctiontype){
                case CCC_EXPONENTIAL:
                    printf("exponential\nCCC exponent: %f\n", cccexp);
                    break;
                case CCC_CUBIC_HERMITE:
                    printf("cubic hermite\nCCC floor: %f\nCCC ceiling: %f\n", cccfloor, cccceiling);
                    break;
                default:
                    break;
            }
        }
        if (spiralcarisma){
            printf("Spiral CARISMA: true\nSpiral CARISMA scaling function type: ");
            switch(scfunctiontype){
                case SC_EXPONENTIAL:
                    printf("exponential, exponent: %f\n", scexp);
                    break;
                case SC_CUBIC_HERMITE:
                    printf("cubic hermite\n");
                    break;
                default:
                    break;
            };
            printf("Spiral CARISMA scaling floor %f and ceiling %f.\n", scfloor, scceiling);
        }
        else {
            printf("Spiral CARISMA: false\n");
        }
        printf("CRT Emulation: ");
        bool printcrtdetails = false;
        switch(crtemumode){
            case CRT_EMU_NONE:
                printf("none\n");
                break;
            case CRT_EMU_FRONT:
                printf("front (before gamut conversion)\n");
                printcrtdetails = true;
                break;
            case CRT_EMU_BACK:
                printf("back (after gamut conversion)\n");
                printcrtdetails = true;
                break;
            default:
                break;
        };
        if (printcrtdetails){
            printf("CRT black level %f x100 cd/m^2\n", crtblacklevel);
            printf("CRT white level %f x100 cd/m^2\n", crtwhitelevel);
            printf("Game console R'G'B' to YIQ modulator chip: ");
            if (crtmodindex == CRT_MODULATOR_NONE){
                printf("none\n");
            }
            else {
                 printf("%s\n", modulatornames[crtmodindex].c_str());
            }
            printf("CRT YIQ to R'G'B' demodulator chip: ");
            if (crtdemodindex == CRT_DEMODULATOR_NONE){
                printf("none\n");
            }
            else {
                 printf("%s\n", demodulatornames[crtdemodindex].c_str());
            }
            printf("CRT R'G'B' high low values clamped to %f.\n", crtclamplow);
            if (crtdoclamphigh){
                printf("CRT R'G'B' high output values clamped to %f.\n", crtclamphigh);
            }
            else {
                printf("CRT R'G'B' high output values not clamped. (Out-of-bounds values resolved by gamut compression algorithm.)\n");
            }
        }
        if (nesmode){
            printf("NES simulate PAL phase alternation: %i\n", nesispal);
            printf("NES normalize chroma to colorburst: %i\n", nescbc);
            printf("NES phase skew for hues 0x2, 0x6, and 0xA: %f degrees\n", nesskew26A);
            printf("NES luma boost for hues 0x4, 0x8, and 0xC: %f IRE\n", nesboost48C);
            printf("NES phase skew per luma step: %f degrees\n", nesskewstep);
        }
        printf("Verbosity: %i\n", verbosity);
        printf("----------\n\n");
    }
    
    // ------------------------------------------------------------------------
    // Initialize stuff
    
    if (!initializeInverseMatrices()){
        printf("Unable to initialize inverse Jzazbz matrices. WTF error!\n");
        return ERROR_INVERT_MATRIX_FAIL;
    }
    
    crtdescriptor emulatedcrt;
    int sourcegamutcrtsetting = CRT_EMU_NONE;
    int destgamutcrtsetting = CRT_EMU_NONE;
    if (crtemumode != CRT_EMU_NONE){
        emulatedcrt.Initialize(crtblacklevel, crtwhitelevel, crtmodindex, crtdemodindex, crtdemodrenorm, crtdoclamphigh, crtclamplow, crtclamphigh, verbosity);
        if (crtemumode == CRT_EMU_FRONT){
            sourcegamutcrtsetting = CRT_EMU_FRONT;
        }
        else if (crtemumode == CRT_EMU_BACK){
            destgamutcrtsetting = CRT_EMU_BACK;
        }
    }
    
    vec3 sourcewhite = vec3(whitepoints[sourcewhitepointindex][0], whitepoints[sourcewhitepointindex][1], whitepoints[sourcewhitepointindex][2]);
    vec3 sourcered = vec3(gamutpoints[sourcegamutindex][0][0], gamutpoints[sourcegamutindex][0][1], gamutpoints[sourcegamutindex][0][2]);
    vec3 sourcegreen = vec3(gamutpoints[sourcegamutindex][1][0], gamutpoints[sourcegamutindex][1][1], gamutpoints[sourcegamutindex][1][2]);
    vec3 sourceblue = vec3(gamutpoints[sourcegamutindex][2][0], gamutpoints[sourcegamutindex][2][1], gamutpoints[sourcegamutindex][2][2]);
    
    vec3 destwhite = vec3(whitepoints[destwhitepointindex][0], whitepoints[destwhitepointindex][1], whitepoints[destwhitepointindex][2]);
    vec3 destred = vec3(gamutpoints[destgamutindex][0][0], gamutpoints[destgamutindex][0][1], gamutpoints[destgamutindex][0][2]);
    vec3 destgreen = vec3(gamutpoints[destgamutindex][1][0], gamutpoints[destgamutindex][1][1], gamutpoints[destgamutindex][1][2]);
    vec3 destblue = vec3(gamutpoints[destgamutindex][2][0], gamutpoints[destgamutindex][2][1], gamutpoints[destgamutindex][2][2]);
    
    bool compressenabled = (mapmode >= MAP_FIRST_COMPRESS);
    
    gamutdescriptor sourcegamut;
    bool srcOK = sourcegamut.initialize(gamutnames[sourcegamutindex], sourcewhite, sourcered, sourcegreen, sourceblue, destwhite, true, verbosity, adapttype, compressenabled, sourcegamutcrtsetting, &emulatedcrt);
    
    gamutdescriptor destgamut;
    bool destOK = destgamut.initialize(gamutnames[destgamutindex], destwhite, destred, destgreen, destblue, sourcewhite, false, verbosity, adapttype, compressenabled, destgamutcrtsetting, &emulatedcrt);
    
    if ((mapmode == MAP_CCC_B) || (mapmode == MAP_CCC_C)){
        destgamut.initializeMatrixChunghwa(sourcegamut, verbosity);
    }
    else if ((mapmode == MAP_CCC_D) || (mapmode == MAP_CCC_E)){
        srcOK &= sourcegamut.initializeKinoshitaStuff(destgamut, verbosity);
    }
    
    if (!srcOK || !destOK){
        printf("Gamut descriptor initialization failed. All is lost. Abandon ship.\n");
        return GAMUT_INITIALIZE_FAIL;
    }

    // if spiral CARISMA is enabled, we need some more initialization
    if (spiralcarisma){
        srcOK = sourcegamut.initializePolarPrimaries(true, scfloor, scceiling, scexp, scfunctiontype, verbosity);
        destOK = destgamut.initializePolarPrimaries(false, scfloor, scceiling, scexp, scfunctiontype, verbosity);
        if (! srcOK || !destOK){
            printf("Gamut descriptor initialization failed in primary/secondary rotation. All is lost. Abandon ship.\n");
            return GAMUT_INITIALIZE_FAIL_SPIRAL;
        }
        sourcegamut.FindPrimaryRotations(destgamut, scmax, verbosity, (mapmode == MAP_EXPAND), remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype);
        
        sourcegamut.WarpBoundaries();
        
        if (verbosity >= VERBOSITY_SLIGHT){
            printf("\n----------\n");
        }
    }
    
    // ---------------------------------------------------------------------------
    // Do actual color processing

    // this mode converts a single color and printfs the result
    if (!filemode && !nesmode){
        int redout;
        int greenout;
        int blueout;
        
        vec3 outcolor = processcolor(inputcolor, gammamodein, gammamodeout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, false, false);

        redout = toRGB8nodither(outcolor.x);
        greenout = toRGB8nodither(outcolor.y);
        blueout = toRGB8nodither(outcolor.z);
        if (verbosity >= VERBOSITY_MINIMAL){
            printf("Input color %s (red %f, green %f, blue %f; red %i, green %i, blue %i) converts to 0x%02X%02X%02X (red %f, green %f, blue %f; red: %i, green: %i, blue: %i)\n", inputcolorstring, inputcolor.x, inputcolor.y, inputcolor.z,  toRGB8nodither(inputcolor.x), toRGB8nodither(inputcolor.y), toRGB8nodither(inputcolor.z), redout, greenout, blueout, outcolor.x, outcolor.y, outcolor.z, redout, greenout, blueout);
        }
        else {
            printf("%02X%02X%02X", redout, greenout, blueout);
        }
        return RETURN_SUCCESS;
    }
    // this mode generates a NES palette
    else if (nesmode){

        nesppusimulation nessim;
        nessim.Initialize(verbosity, nesispal, nescbc, nesskew26A, nesboost48C, nesskewstep);
        printf("Generating NES palette and saving result to %s...\n", outputfilename);

        std::ofstream palfile(outputfilename, std::ios::out | std::ios::binary);
        if (!palfile.is_open()){
            printf("Unable to open %s for writing.\n", outputfilename);
            return ERROR_PNG_OPEN_FAIL;
        }
        std::ofstream htmlfile;
        if (neswritehtml){
            printf("and saving result as html to %s...\n", neshtmlfilename);
            htmlfile.open(neshtmlfilename, std::ios::out);
            if (!htmlfile.is_open()){
                printf("Unable to open %s for writing.\n", neshtmlfilename);
                return ERROR_PNG_OPEN_FAIL;
            }
            htmlfile << "<html>\n\t<head>\n\t\t<title>NES Palette</title>\n\t<head>\n\t<body>\n\t\t<div style=\"margin-left:auto; margin-right:auto; margin-top: 1em; margin-bottom: 1em; width:60%;\">\n";

            htmlfile << "\t\t\tPalette saved to: " << outputfilename << "<BR>\n";

            htmlfile << "\t\t\tParameters:<BR>\n";

            htmlfile << "\t\t\tNES simulate PAL phase alternation: " << (nesispal ? "true" : "false") << "<BR>\n";
            htmlfile << "\t\t\tNES normalize chroma to colorburst: " << (nescbc ? "true" : "false") << "<BR>\n";
            htmlfile << "\t\t\tNES phase skew for hues 0x2, 0x6, and 0xA: " << nesskew26A << " degrees<BR>\n";
            htmlfile << "\t\t\tNES luma boost for hues 0x4, 0x8, and 0xC: " << nesboost48C << " IRE<BR>\n";
            htmlfile << "\t\t\tNES phase skew per luma step: " << nesskewstep << " degrees<BR>\n";

            htmlfile << "\t\t\tCRT YIQ to R'G'B' demodulator chip: ";
            if (crtdemodindex == CRT_DEMODULATOR_NONE){
                htmlfile << "none<BR>\n";
            }
            else {
                 htmlfile << demodulatornames[crtdemodindex] << "<BR>\n";
            }

            htmlfile << "\t\t\tCRT bt1886 Appendix1 EOTF function calibrated to:<BR>\n";
            htmlfile << "\t\t\tCRT black level: " << crtblacklevel << " x100 cd/m^2<BR>\n";
            htmlfile << "\t\t\tCRT white level: " << crtwhitelevel << " x100 cd/m^2<BR>\n";

            htmlfile << "\t\t\tSource primaries: " << gamutnames[sourcegamutindex] << "<BR>\n";
            htmlfile << "\t\t\tSource whitepoint: " << whitepointnames[sourcewhitepointindex] << "<BR>\n";
            htmlfile << "\t\t\tDestination primaries: " << gamutnames[destgamutindex] << "<BR>\n";
            htmlfile << "\t\t\tDestination whitepoint: " << whitepointnames[destwhitepointindex] << "<BR>\n";

            if (sourcegamut.needschromaticadapt || destgamut.needschromaticadapt){
                htmlfile << "\t\t\tChromatic adapation type: ";
                switch(adapttype){
                    case ADAPT_BRADFORD:
                        htmlfile << "Bradford";
                        break;
                    case ADAPT_CAT16:
                        htmlfile << "CAT16";
                        break;
                    default:
                    break;
                };
                htmlfile << "<BR>\n";
            }


            htmlfile << "\t\t\tGamut mapping mode: ";
            switch(mapmode){
                case MAP_CLIP:
                    htmlfile << "clip";
                    break;
                case MAP_CCC_A:
                    htmlfile << "pseudo color correction circuit A"; // this should be disabled and unreachable
                    break;
                case MAP_COMPRESS:
                    htmlfile << "compress";
                    break;
                case MAP_EXPAND:
                    htmlfile << "expand";
                    break;
                default:
                    break;
            };
            htmlfile << "<BR>\n";

            if (mapmode >= MAP_FIRST_COMPRESS){
                htmlfile << "\t\t\tGamut mapping algorithm: ";
                switch(mapdirection){
                    case MAP_GCUSP:
                        htmlfile << "CUSP";
                        break;
                    case MAP_HLPCM:
                        htmlfile << "HLPCM";
                        break;
                    case MAP_VP:
                        htmlfile << "VP";
                        break;
                    case MAP_VPR:
                        htmlfile << "VPR";
                        break;
                    case MAP_VPRC:
                        htmlfile << "VPRC";
                        break;
                    default:
                        break;
                };
                htmlfile << "<BR>\n";
            }

            htmlfile << "\t\t\tOutput gamma function: " << (gammamodeout ? "sRGB" : "linear RGB") << "<BR>\n";

            htmlfile << "\t\t</div>\n\t\t<table style=\"margin-left:auto; margin-right:auto; border:0px; border-collapse: collapse;\">\n";
        }


        for (int emp=0; emp<8; emp++){
            if (neswritehtml){
                    htmlfile << "\t\t\t<tr>\n\t\t\t\t<td colspan=\"16\" style=\"text-align:left; background-color: #ddd; color:#000; padding-top: 1em;\">Emphasis 0x" << emp << "</td>\n\t\t\t</tr>\n";
                }
            for (int luma=0; luma<4; luma++){
                if (neswritehtml){
                    htmlfile << "\t\t\t<tr>\n";
                }
                for (int hue=0; hue < 16; hue++){
                    vec3 nesrgb = nessim.NEStoRGB(hue,luma, emp);
                    vec3 outcolor = processcolor(nesrgb, gammamodein, gammamodeout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, eilut, true);
                    // for now screen barf
                    //printf("NES palette: Luma %i, hue %i, emp %i yeilds RGB: ", luma, hue, emp);
                    //nesrgb.printout();
                    //outcolor.printout();
                    unsigned char redout = toRGB8nodither(outcolor.x);
                    unsigned char greenout = toRGB8nodither(outcolor.y);
                    unsigned char blueout = toRGB8nodither(outcolor.z);
                    palfile.write(reinterpret_cast<const char*>(&redout), 1);
                    palfile.write(reinterpret_cast<const char*>(&greenout), 1);
                    palfile.write(reinterpret_cast<const char*>(&blueout), 1);
                    if (neswritehtml){
                        bool useblack = (0.5 <= (outcolor.x * 0.212649342720653) + (outcolor.y * 0.715169135705904) + (outcolor.z * 0.0721815215734433));
                        char thiscolorstring[7];
                        sprintf(thiscolorstring, "%02X%02X%02X", redout, greenout, blueout);
                        char indexstring[3];
                        sprintf(indexstring, "%X%X", luma, hue);
                        htmlfile << "\t\t\t\t<td style=\"border:0px; padding:1em; background-color:#" << thiscolorstring << ";text-align:center; color:";
                        if (useblack){
                            htmlfile << "#000";
                        }
                        else {
                            htmlfile << "#fff";
                        }
                        htmlfile << "\">$" << indexstring << "<BR>" << thiscolorstring << "</td>\n";
                    }
                }
                if (neswritehtml){
                    htmlfile << "\t\t\t</tr>\n";
                }
            }
        }
        palfile.flush();
        palfile.close();
        if (neswritehtml){
            htmlfile << "</table>\n\t</body>\n</html>";
            htmlfile.flush();
            htmlfile.close();
        }
        printf("done.\n");

        return RETURN_SUCCESS;
    }

    // if we didn't just return, we are in file mode or LUT mode

    int result = ERROR_PNG_FAIL;
    png_image image;
    
    /* Only the image structure version number needs to be set. */
    memset(&image, 0, sizeof image);
    image.version = PNG_IMAGE_VERSION;

    // we also need dimensions if we're generating a LUT
    if (lutgen){
        image.width = lutsize * lutsize;
        image.height = lutsize;
    }

    // check for lutgen first to short-circuit reading from file that isn't there
    if (lutgen || png_image_begin_read_from_file(&image, inputfilename)){
        png_bytep buffer;

        /* Change this to try different formats!  If you set a colormap format
        * then you must also supply a colormap below.
        */
        image.format = PNG_FORMAT_RGBA;

        size_t buffsize;
        if (lutgen){
            buffsize = lutsize * lutsize * lutsize * 4;
        }
        else {
            buffsize = PNG_IMAGE_SIZE(image);
        }
        buffer = (png_bytep) malloc(buffsize);  //c++ wants an explict cast

        if (buffer != NULL){
            // check for lutgen first to short-circuit reading from file that isn't there
            if (lutgen || png_image_finish_read(&image, NULL/*background*/, buffer, 0/*row_stride*/, NULL/*colormap for PNG_FORMAT_FLAG_COLORMAP */)){
                
                // ------------------------------------------------------------------------------------------------------------------------------------------
                // Begin actual color conversion code
                
                // zero the memos
                memset(&memos, 0, 256 * 256 * 256 * sizeof(memo));
                
                if (verbosity >= VERBOSITY_MINIMAL){
                    if (lutgen){
                        printf("Doing gamut conversion on LUT and saving result to %s...\n", outputfilename);
                    }
                    else {
                        printf("Doing gamut conversion on %s and saving result to %s...\n", inputfilename, outputfilename);
                    }
                }
                
                // iterate over every pixel
                int width = image.width;
                int height = image.height;
                for (int y=0; y<height; y++){
                    if (verbosity >= VERBOSITY_HIGH){
                        printf("\trow %i of %i...\n", y+1, height);
                    }
                    else if (verbosity >= VERBOSITY_MINIMAL){
                        if (y == 0){
                            printf("0%%... ");
                            fflush(stdout);
                        }
                        else if ((y < height -1) && ((((y+1)*20)/height) > ((y*20)/height))){
                            printf("%i%%... ", ((y+1)*100)/height);
                            if (((y+1)*100)/height == 50){
                                printf("\n");
                            }
                            fflush(stdout);
                        }
                    }
                    for (int x=0; x<width; x++){
                        
                        // read bytes from buffer (unless doing LUT)

                        png_byte redin;
                        png_byte greenin;
                        png_byte bluein;
                        if (!lutgen){
                            redin = buffer[ ((y * width) + x) * 4];
                            greenin = buffer[ (((y * width) + x) * 4) + 1 ];
                            bluein = buffer[ (((y * width) + x) * 4) + 2 ];
                        }

                        vec3 outcolor;
                        
                        // if we've already processed the same input color, just recall the memo
                        if (!lutgen && memos[redin][greenin][bluein].known){
                            outcolor = memos[redin][greenin][bluein].data;
                        }
                        else {
                        
                            double redvalue;
                            double greenvalue;
                            double bluevalue;

                            if (lutgen){
                                redvalue = (double)(x % lutsize) / ((double)(lutsize - 1));
                                greenvalue = (double)y / ((double)(lutsize - 1));
                                bluevalue = (double)(x / lutsize) / ((double)(lutsize - 1));

                                // expanded intermediate LUT uses range of -0.5 to 1.5
                                if (eilut){
                                    redvalue = (redvalue * 2.0) - 0.5;
                                    greenvalue = (greenvalue * 2.0) - 0.5;
                                    bluevalue = (bluevalue * 2.0) - 0.5;
                                }
                            }
                            else {
                                // convert to double
                                redvalue = redin/255.0;
                                greenvalue = greenin/255.0;
                                bluevalue = bluein/255.0;
                                // don't touch alpha value
                            }

                            vec3 inputcolor = vec3(redvalue, greenvalue, bluevalue);
                            
                            outcolor = processcolor(inputcolor, gammamodein, gammamodeout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, eilut, false);

                            // blank the out-of-bounds stuff for sanity checking extended intermediate LUTSs
                            /*
                            if ((redvalue < 0.0) || (greenvalue < 0.0) || (bluevalue < 0.0) || (redvalue > 1.0) || (greenvalue > 1.0) || (bluevalue > 1.0)){
                                outcolor = vec3(1.0, 1.0, 1.0);
                            }
                            */

                            // memoize the result of the conversion so we don't need to do it again for this input color
                            if (!lutgen){
                                memos[redin][greenin][bluein].known = true;
                                memos[redin][greenin][bluein].data = outcolor;
                            }
                        }

                        png_byte redout, greenout, blueout;
                        
                        // dither and back to RGB8 if enabled
                        if (dither){
                            // use inverse x coord for red and inverse y coord for blue to decouple dither patterns across channels
                            // see https://blog.kaetemi.be/2015/04/01/practical-bayer-dithering/
                            redout = quasirandomdither(outcolor.x, width - x - 1, y);
                            greenout = quasirandomdither(outcolor.y, x, y);
                            blueout = quasirandomdither(outcolor.z, x, height - y - 1);
                        }
                        // otherwise just back to RGB 8
                        else {
                            redout = toRGB8nodither(outcolor.x);
                            greenout = toRGB8nodither(outcolor.y);
                            blueout = toRGB8nodither(outcolor.z);
                        }
                        
                        // save back to buffer
                        buffer[ ((y * width) + x) * 4] = redout;
                        buffer[ (((y * width) + x) * 4) + 1 ] = greenout;
                        buffer[ (((y * width) + x) * 4) + 2 ] = blueout;
                        // we need to set opacity data id generating a LUT; if reading an image, leave it unchanged
                        if (lutgen){
                            buffer[ (((y * width) + x) * 4) + 3 ] = 255;
                        }
                                                
                    }
                }
                if ((verbosity >= VERBOSITY_MINIMAL) && (verbosity < VERBOSITY_HIGH)){
                    printf("100%%\n");
                }
                
                // End actual color conversion code
                // ------------------------------------------------------------------------------------------------------------------------------------------
                
                
                if (png_image_write_to_file(&image, outputfilename, 0/*convert_to_8bit*/, buffer, 0/*row_stride*/, NULL/*colormap*/)){
                    result = RETURN_SUCCESS;
                    printf("done.\n");
                }

                else {
                    fprintf(stderr, "gamutthingy: write %s: %s\n", outputfilename, image.message);
                    result = ERROR_PNG_WRITE_FAIL;
                }
            }

            else {
                fprintf(stderr, "gamutthingy: read %s: %s\n", inputfilename, image.message);
                result = ERROR_PNG_READ_FAIL;
            }

            free(buffer);
            buffer = NULL;
        }

        else {
            fprintf(stderr, "gamutthingy: out of memory: %lu bytes\n", (unsigned long)PNG_IMAGE_SIZE(image));
            result = ERROR_PNG_MEM_FAIL;

            /* This is the only place where a 'free' is required; libpng does
                * the cleanup on error and success, but in this case we couldn't
                * complete the read because of running out of memory and so libpng
                * has not got to the point where it can do cleanup.
                */
            png_image_free(&image);
        }
    }

    else {
        /* Failed to read the input file argument: */
        fprintf(stderr, "gamutthingy: %s: %s\n", inputfilename, image.message);
        result = ERROR_PNG_OPEN_FAIL;
    }
   
   return result;
}
