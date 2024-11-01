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
    
// this has to be global because it's too big for the stack
memo memos[256][256][256];

vec3 processcolor(vec3 inputcolor, bool gammamode, int mapmode, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, int cccfunctiontype, double cccfloor, double cccceiling, double cccexp, double remapfactor, double remaplimit, bool softkneemode, double kneefactor, int mapdirection, int safezonetype, bool spiralcarisma, bool eilutmode, bool nesmode){
    vec3 linearinputcolor = inputcolor;
    if (sourcegamut.crtemumode == CRT_EMU_FRONT){
        if (eilutmode){
            linearinputcolor = sourcegamut.attachedCRT->tolinear1886appx1vec3(inputcolor);
        }
        else {
            linearinputcolor = sourcegamut.attachedCRT->CRTEmulateGammaSpaceRGBtoLinearRGB(inputcolor);
        }
    }
    else if (gammamode){
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
    else if (gammamode){
        outcolor = vec3(togamma(outcolor.x), togamma(outcolor.y), togamma(outcolor.z));
    }
    return outcolor;
}

int main(int argc, const char **argv){
    
    // parameter processing -------------------------------------------------------------------
    
    if ((argc < 2) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "-h") == 0)){
        printhelp();
        return 0;
    }
    
    // defaults
    bool filemode = true;
    bool gammamode = true;
    bool softkneemode = true;
    bool dither = true;
    int mapdirection = MAP_VPR;
    int mapmode = MAP_COMPRESS;
    int sourcegamutindex = GAMUT_NTSCJ_R;
    int destgamutindex = GAMUT_SRGB;
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
    bool lutgen = false;
    bool eilut = false;
    unsigned int lutsize = 128;
    bool nesmode = false;
    bool nesispal = false;
    bool nescbc = true;
    double nesskew26A = 4.5;
    double nesboost48C = 1.0;
    double nesskewstep = 2.5;
    
    int expect = 0;
    for (int i=1; i<argc; i++){
        //printf("processing: %s expect is %i\n", argv[i], expect);
        if (expect == 1){ // expecting input filename
            inputfilename = const_cast<char*>(argv[i]);
            expect  = 0;
            infileset = true;
        }
        else if (expect == 2){ // expecting output filename
            outputfilename = const_cast<char*>(argv[i]);
            expect  = 0;
            outfileset = true;
        }
        else if (expect == 3){ // expecting a color
            inputcolorstring = const_cast<char*>(argv[i]);
            expect  = 0;
            incolorset = true;
        }
        else if (expect == 4){ // expecting source gamut
            if ((strcmp(argv[i], "srgb") == 0)){
                sourcegamutindex = GAMUT_SRGB;
            }
            else if ((strcmp(argv[i], "ntscj") == 0) || (strcmp(argv[i], "ntscjr") == 0)){
                sourcegamutindex = GAMUT_NTSCJ_R;
            }
            else if (strcmp(argv[i], "ntscjb") == 0){
                sourcegamutindex = GAMUT_NTSCJ_B;
            }
            else if (/*(strcmp(argv[i], "ntscj") == 0) ||*/ (strcmp(argv[i], "ntscjp22") == 0)){
                sourcegamutindex = GAMUT_NTSCJ_P22;
            }
            else if (strcmp(argv[i], "ntscjebu") == 0){
                sourcegamutindex = GAMUT_NTSCJ_EBU;
            }
            else if ((strcmp(argv[i], "ntscjp22trinitron") == 0)){
                sourcegamutindex = GAMUT_NTSCJ_P22_TRINITRON;
            }
            else if ((strcmp(argv[i], "ntscup22trinitron") == 0)){
                sourcegamutindex = GAMUT_NTSCU_P22_TRINITRON;
            }
            else if ((strcmp(argv[i], "smptecp22trinitron") == 0)){
                sourcegamutindex = GAMUT_SMPTEC_P22_TRINITRON;
            }
            else if (strcmp(argv[i], "smptec") == 0){
                sourcegamutindex = GAMUT_SMPTEC;
            }
            else if (strcmp(argv[i], "ebu") == 0){
                sourcegamutindex = GAMUT_EBU;
            }
            else if (strcmp(argv[i], "ntsc1953") == 0){
                sourcegamutindex = GAMUT_NTSC_1953;
            }
            else {
                printf("Invalid parameter for source gamut. Expecting \"srgb\", \"ntsc1953\", \"ntscj\", \"ntscjr\", \"ntscjb\", \"ntscjp22\", \"ntscjebu\", \"ntscjp22trinitron\", \"ntscup22trinitron\", \"smptecp22trinitron\", \"smptec\", or \"ebu\".\n");
                return ERROR_BAD_PARAM_SOURCE_GAMUT;
            }
            expect  = 0;
        }
        else if (expect == 5){ // expecting source gamut
            if (strcmp(argv[i], "srgb") == 0){
                destgamutindex = GAMUT_SRGB;
            }
            else if ((strcmp(argv[i], "ntscj") == 0) || (strcmp(argv[i], "ntscjr") == 0)){
                destgamutindex = GAMUT_NTSCJ_R;
            }
            else if (strcmp(argv[i], "ntscjb") == 0){
                destgamutindex = GAMUT_NTSCJ_B;
            }
            else if (/*(strcmp(argv[i], "ntscj") == 0) ||*/ (strcmp(argv[i], "ntscjp22") == 0)){
                destgamutindex = GAMUT_NTSCJ_P22;
            }
            else if (strcmp(argv[i], "ntscjebu") == 0){
                destgamutindex = GAMUT_NTSCJ_EBU;
            }
            else if ((strcmp(argv[i], "ntscjp22trinitron") == 0)){
                destgamutindex = GAMUT_NTSCJ_P22_TRINITRON;
            }
            else if ((strcmp(argv[i], "ntscup22trinitron") == 0)){
                destgamutindex = GAMUT_NTSCU_P22_TRINITRON;
            }
            else if ((strcmp(argv[i], "smptecp22trinitron") == 0)){
                destgamutindex = GAMUT_SMPTEC_P22_TRINITRON;
            }
            else if (strcmp(argv[i], "smptec") == 0){
                destgamutindex = GAMUT_SMPTEC;
            }
            else if (strcmp(argv[i], "ebu") == 0){
                destgamutindex = GAMUT_EBU;
            }
            else if (strcmp(argv[i], "ntsc1953") == 0){
                destgamutindex = GAMUT_NTSC_1953;
            }
            else {
                printf("Invalid parameter for destination gamut. Expecting \"srgb\", \"ntsc1953\", \"ntscj\", \"ntscjr\", \"ntscjb\", \"ntscjp22\", \"ntscjebu\", \"ntscjp22trinitron\", \"ntscup22trinitron\", \"smptecp22trinitron\", \"smptec\", or \"ebu\".\n");
                return ERROR_BAD_PARAM_DEST_GAMUT;
            }
            expect  = 0;
        }
        else if (expect == 6){ // expecting map mode
            if (strcmp(argv[i], "clip") == 0){
                mapmode = MAP_CLIP;
            }
            else if (strcmp(argv[i], "compress") == 0){
                mapmode = MAP_COMPRESS;
            }
            else if (strcmp(argv[i], "expand") == 0){
                mapmode = MAP_EXPAND;
            }
            else if (strcmp(argv[i], "ccca") == 0){
                mapmode = MAP_CCC_A;
            }
            else if (strcmp(argv[i], "cccb") == 0){
                mapmode = MAP_CCC_B;
            }
            else if (strcmp(argv[i], "cccc") == 0){
                mapmode = MAP_CCC_C;
            }
            else if (strcmp(argv[i], "cccd") == 0){
                mapmode = MAP_CCC_D;
            }
            else if (strcmp(argv[i], "ccce") == 0){
                mapmode = MAP_CCC_E;
            }
            else {
                printf("Invalid parameter for mapping mode. Expecting \"clip\", \"compress\", \"expand\", or \"ccca\".\n");
                return ERROR_BAD_PARAM_MAPPING_MODE;
            }
            expect  = 0;
        }
        // expecting a number
        else if ((expect == 7) || (expect == 8) || (expect == 9) || (expect == 18) || (expect == 19) || (expect == 20) || (expect == 23) || (expect == 24) || (expect == 25) || (expect == 28) || (expect == 29) || (expect == 38) || (expect == 39) || (expect == 40)){
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
                switch (expect){
                    case 7:
                        remapfactor = input;
                        break;
                    case 8:
                        remaplimit = input;
                        break;
                    case 9:
                        kneefactor = input;
                        break;
                    case 18:
                        cccfloor = input;
                        break;
                    case 19:
                        cccceiling = input;
                        break;
                    case 20:
                        cccexp = input;
                        break;
                    case 23:
                        scfloor = input;
                        break;
                    case 24:
                        scceiling = input;
                        break;
                    case 25:
                        scexp = input;
                        break;
                    case 26:
                        scmax = input;
                        break;
                    case 28:
                        crtblacklevel = input;
                        break;
                    case 29:
                        crtwhitelevel = input;
                        break;
                    case 38:
                        nesskew26A = input;
                        break;
                    case 39:
                        nesboost48C = input;
                        break;
                    case 40:
                        nesskewstep = input;
                        break;
                    default:
                        break;
                };
            }
            else {
                printf("Invalid parameter for numerical value. (Malformed float.)");
                return ERROR_BAD_PARAM_MAPPING_FLOAT;
            }
            expect = 0;
        }
        else if (expect == 10){ // expecting map direction
            if (strcmp(argv[i], "cusp") == 0){
                mapdirection = MAP_GCUSP;
            }
            else if (strcmp(argv[i], "hlpcm") == 0){
                mapdirection = MAP_HLPCM;
            }
            else if (strcmp(argv[i], "vp") == 0){
                mapdirection = MAP_VP;
            }
            else if (strcmp(argv[i], "vpr") == 0){
                mapdirection = MAP_VPR;
            }
            else if (strcmp(argv[i], "vprc") == 0){
                mapdirection = MAP_VPRC;
            }
            else {
                printf("Invalid parameter for mapping direction. Expecting \"cusp\", \"hlpcm\", \"vp\", \"vpr\", or \"vprc\".\n");
                return ERROR_BAD_PARAM_MAPPING_DIRECTION;
            }
            expect  = 0;
        }
        else if (expect == 11){ // safe zone type
            if (strcmp(argv[i], "const-detail") == 0){
                safezonetype = RMZONE_DELTA_BASED;
            }
            else if (strcmp(argv[i], "const-fidelity") == 0){
                safezonetype = RMZONE_DEST_BASED;
            }
            else {
                printf("Invalid parameter for safe zone type. Expecting \"const-detail\" or \"const-fidelity\".\n");
                return ERROR_BAD_PARAM_ZONE_TYPE;
            }
            expect  = 0;
        }
        else if (expect == 12){ // knee type
            if (strcmp(argv[i], "hard") == 0){
                softkneemode = false;
            }
            else if (strcmp(argv[i], "soft") == 0){
                softkneemode = true;
            }
            else {
                printf("Invalid parameter for knee type. Expecting \"soft\" or \"hard\".\n");
                return ERROR_BAD_PARAM_KNEE_TYPE;
            }
            expect  = 0;
        }
        else if (expect == 13){ // gamma type
            if (strcmp(argv[i], "srgb") == 0){
                gammamode = true;
            }
            else if (strcmp(argv[i], "linear") == 0){
                gammamode = false;
            }
            else {
                printf("Invalid parameter for gamma function. Expecting \"srgb\" or \"linear\".\n");
                return ERROR_BAD_PARAM_GAMMA_TYPE;
            }
            expect  = 0;
        }
        else if (expect == 14){ // dither
            if (strcmp(argv[i], "true") == 0){
                dither = true;
            }
            else if (strcmp(argv[i], "false") == 0){
                dither = false;
            }
            else {
                printf("Invalid parameter for dither. Expecting \"true\" or \"false\".\n");
                return ERROR_BAD_PARAM_DITHER;
            }
            expect  = 0;
        }
        else if (expect == 15){ //verbosity
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
                if (input < VERBOSITY_SILENT){
                    input = VERBOSITY_SILENT;
                }
                if (input > VERBOSITY_EXTREME){
                    input = VERBOSITY_EXTREME;
                }
                verbosity = input;
            }
            else {
                printf("Invalid parameter for verbosity. (Malformed int.)");
                return ERROR_BAD_PARAM_MALFORMEDINT;
            }
            expect = 0;
        }
        else if (expect == 16){ // adapt type
            if (strcmp(argv[i], "bradford") == 0){
                adapttype = ADAPT_BRADFORD;
            }
            else if (strcmp(argv[i], "cat16") == 0){
                adapttype = ADAPT_CAT16;
            }
            else {
                printf("Invalid parameter for chomatic adapation type. Expecting \"bradford\" or \"cat16\".\n");
                return ERROR_BAD_PARAM_ADAPT_TYPE;
            }
            expect  = 0;
        }
        else if (expect == 17){ // ccc fucntion type
            if (strcmp(argv[i], "exponential") == 0){
                cccfunctiontype = CCC_EXPONENTIAL;
            }
            else if (strcmp(argv[i], "cubichermite") == 0){
                cccfunctiontype = CCC_CUBIC_HERMITE;
            }
            else {
                printf("Invalid parameter for ccc function type. Expecting \"exponential\" or \"cubichermite\".\n");
                return ERROR_BAD_PARAM_CCC_FUNCTION_TYPE;
            }
            expect  = 0;
        }
        else if (expect == 21){ // spiralcarisma
            if (strcmp(argv[i], "true") == 0){
                spiralcarisma = true;
            }
            else if (strcmp(argv[i], "false") == 0){
                spiralcarisma = false;
            }
            else {
                printf("Invalid parameter for spiralcarisma. Expecting \"true\" or \"false\".\n");
                return ERROR_BAD_PARAM_DITHER;
            }
            expect  = 0;
        }
        else if (expect == 22){ // sc fucntion type
            if (strcmp(argv[i], "exponential") == 0){
                scfunctiontype = SC_EXPONENTIAL;
            }
            else if (strcmp(argv[i], "cubichermite") == 0){
                scfunctiontype = SC_CUBIC_HERMITE;
            }
            else {
                printf("Invalid parameter for sc function type. Expecting \"exponential\" or \"cubichermite\".\n");
                return ERROR_BAD_PARAM_SC_FUNCTION_TYPE;
            }
            expect  = 0;
        }
        else if (expect == 27){ // crt emulation mode
            if (strcmp(argv[i], "none") == 0){
                crtemumode = CRT_EMU_NONE;
            }
            else if (strcmp(argv[i], "front") == 0){
                crtemumode = CRT_EMU_FRONT;
            }
            else if (strcmp(argv[i], "back") == 0){
                crtemumode = CRT_EMU_BACK;
            }
            else {
                printf("Invalid parameter for CRT emulation mode. Expecting \"none\", \"front\", or \"back\".\n");
                return ERROR_BAD_PARAM_CRT_EMU_MODE;
            }
            expect  = 0;
        }
        else if (expect == 30){ // crt modulator chip
            if (strcmp(argv[i], "none") == 0){
                crtmodindex = CRT_MODULATOR_NONE;
            }
            else if (strcmp(argv[i], "CXA1145") == 0){
                crtmodindex = CRT_MODULATOR_CXA1145;
            }
            else if (strcmp(argv[i], "CXA1645") == 0){
                crtmodindex = CRT_MODULATOR_CXA1645;
            }
            else {
                printf("Invalid parameter for CRT emulation modulator chip ID. Expecting \"none\", \"CXA1145\", or \"CXA1645\".\n");
                return ERROR_BAD_PARAM_CRT_EMU_MODE;
            }
            expect  = 0;
        }
        else if (expect == 31){ // crt demodulator chip

            /*
                #define CRT_DEMODULATOR_NONE -1
                #define CRT_DEMODULATOR_DUMMY 0
                #define CRT_DEMODULATOR_CXA1464AS 1
                #define CRT_DEMODULATOR_CXA1465AS 2
                #define CRT_DEMODULATOR_CXA1870S_JP 3
                #define CRT_DEMODULATOR_CXA1870S_US 4
                #define CRT_DEMODULATOR_CXA2060BS_JP 5
                #define CRT_DEMODULATOR_CXA2060BS_US 6
                #define CRT_DEMODULATOR_CXA2025AS_JP 7
                #define CRT_DEMODULATOR_CXA2025AS_US 8
                #define CRT_DEMODULATOR_CXA1213AS 9
             */

            if (strcmp(argv[i], "none") == 0){
                crtdemodindex = CRT_DEMODULATOR_NONE;
            }
            else if (strcmp(argv[i], "dummy") == 0){
                crtdemodindex = CRT_DEMODULATOR_DUMMY;
            }
            else if (strcmp(argv[i], "CXA1464AS") == 0){
                crtdemodindex = CRT_DEMODULATOR_CXA1464AS;
            }
            else if (strcmp(argv[i], "CXA1465AS") == 0){
                crtdemodindex = CRT_DEMODULATOR_CXA1465AS;
            }
            else if (strcmp(argv[i], "CXA1870S_JP") == 0){
                crtdemodindex = CRT_DEMODULATOR_CXA1870S_JP;
            }
            else if (strcmp(argv[i], "CXA1870S_US") == 0){
                crtdemodindex = CRT_DEMODULATOR_CXA1870S_US;
            }
            else if (strcmp(argv[i], "CXA2060BS_JP") == 0){
                crtdemodindex = CRT_DEMODULATOR_CXA2060BS_JP;
            }
            else if (strcmp(argv[i], "CXA2060BS_US") == 0){
                crtdemodindex = CRT_DEMODULATOR_CXA2060BS_US;
            }
            else if (strcmp(argv[i], "CXA2025AS_JP") == 0){
                crtdemodindex = CRT_DEMODULATOR_CXA2025AS_JP;
            }
            else if (strcmp(argv[i], "CXA2025AS_US") == 0){
                crtdemodindex = CRT_DEMODULATOR_CXA2025AS_JP;
            }
            /* skip this for now b/c the gains are wrong and need math
            else if (strcmp(argv[i], "CXA1213AS") == 0){
                crtdemodindex = CRT_DEMODULATOR_CXA1213AS;
            }
            */
            else {
                printf("Invalid parameter for CRT emulation modulator chip ID. Expecting \"none\", \"dummy\", \"CXA1464AS\", \"CXA1465AS\", \"CXA1870S_JP\", \"CXA1870S_US\", \"CXA2060BS_JP\", \"CXA2060BS_US\", \"CXA2025AS_JP\", or \"CXA2025AS_US\".\n");
                return ERROR_BAD_PARAM_CRT_EMU_MODE;
            }
            expect  = 0;
        }
        else if (expect == 32){ // lutgen
            if (strcmp(argv[i], "true") == 0){
                lutgen = true;
            }
            else if (strcmp(argv[i], "false") == 0){
                lutgen = false;
            }
            else {
                printf("Invalid parameter for lutgen. Expecting \"true\" or \"false\".\n");
                return ERROR_BAD_PARAM_LUTGEN;
            }
            expect  = 0;
        }
        else if (expect == 33){ // lutsize
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
                if (input < 2){
                    input = 2;
                    printf("\nWARNING: LUT size cannot be less than 2. Changing to 2.\n");
                }
                else if (input > 128){
                    printf("\nWARNING: LUT size is %li. Some programs, e.g. retroarch, cannot handle extra-large LUTs.\n", input);
                }
                lutsize = input;
            }
            else {
                printf("Invalid parameter for lutsize. (Malformed int.)");
                return ERROR_BAD_PARAM_MALFORMEDINT;
            }
            expect = 0;
        }
        else if (expect == 34){ // eilut
            if (strcmp(argv[i], "true") == 0){
                eilut = true;
            }
            else if (strcmp(argv[i], "false") == 0){
                eilut = false;
            }
            else {
                printf("Invalid parameter for eilut. Expecting \"true\" or \"false\".\n");
                return ERROR_BAD_PARAM_EILUT;
            }
            expect  = 0;
        }
        else if (expect == 35){ // nespalgen
            if (strcmp(argv[i], "true") == 0){
                nesmode = true;
            }
            else if (strcmp(argv[i], "false") == 0){
                nesmode = false;
            }
            else {
                printf("Invalid parameter for nespalgen. Expecting \"true\" or \"false\".\n");
                return ERROR_BAD_PARAM_NES;
            }
            expect  = 0;
        }
        else if (expect == 36){ // nespalmode
            if (strcmp(argv[i], "true") == 0){
                nesispal = true;
            }
            else if (strcmp(argv[i], "false") == 0){
                nesispal = false;
            }
            else {
                printf("Invalid parameter for nespalmode. Expecting \"true\" or \"false\".\n");
                return ERROR_BAD_PARAM_NESPAL;
            }
            expect  = 0;
        }
        else if (expect == 37){ // nesburstnorm
            if (strcmp(argv[i], "true") == 0){
                nescbc = true;
            }
            else if (strcmp(argv[i], "false") == 0){
                nescbc = false;
            }
            else {
                printf("Invalid parameter for nesburstnorm. Expecting \"true\" or \"false\".\n");
                return ERROR_BAD_PARAM_NESBURST;
            }
            expect  = 0;
        }
        else {
            if ((strcmp(argv[i], "--infile") == 0) || (strcmp(argv[i], "-i") == 0)){
                filemode = true;
                expect = 1;
            }
            else if ((strcmp(argv[i], "--outfile") == 0) || (strcmp(argv[i], "-o") == 0)){
                expect = 2;
            }
            else if ((strcmp(argv[i], "--color") == 0) || (strcmp(argv[i], "-c") == 0)){
                filemode = false;
                expect = 3;
            }
            else if ((strcmp(argv[i], "--gamma") == 0) || (strcmp(argv[i], "-g") == 0)){
                expect = 13;
            }
            else if ((strcmp(argv[i], "--source-gamut") == 0) || (strcmp(argv[i], "-s") == 0)){
                expect = 4;
            }
            else if ((strcmp(argv[i], "--dest-gamut") == 0) || (strcmp(argv[i], "-d") == 0)){
                expect = 5;
            }
            else if ((strcmp(argv[i], "--map-mode") == 0) || (strcmp(argv[i], "-m") == 0)){
                expect = 6;
            }
            else if ((strcmp(argv[i], "--remap-factor") == 0) || (strcmp(argv[i], "--rf") == 0)){
                expect = 7;
            }
            else if ((strcmp(argv[i], "--remap-limit") == 0) || (strcmp(argv[i], "--rl") == 0)){
                expect = 8;
            }
            else if ((strcmp(argv[i], "--knee-factor") == 0) || (strcmp(argv[i], "--kf") == 0)){
                expect = 9;
            }
            else if ((strcmp(argv[i], "--knee") == 0) || (strcmp(argv[i], "-k") == 0)){
                expect = 12;
            }
            else if ((strcmp(argv[i], "--gamut-mapping-algorithm") == 0) || (strcmp(argv[i], "--gma") == 0)){
                expect = 10;
            }
            else if ((strcmp(argv[i], "--safe-zone-type") == 0) || (strcmp(argv[i], "-z") == 0)){
                expect = 11;
            }
            else if ((strcmp(argv[i], "--dither") == 0) || (strcmp(argv[i], "--di") == 0)){
                expect = 14;
            }
            else if ((strcmp(argv[i], "--verbosity") == 0) || (strcmp(argv[i], "-v") == 0)){
                expect = 15;
            }
            else if ((strcmp(argv[i], "--adapt") == 0) || (strcmp(argv[i], "-a") == 0)){
                expect = 16;
            }
            else if ((strcmp(argv[i], "--cccfunction") == 0) || (strcmp(argv[i], "--cccf") == 0)){
                expect = 17;
            }
            else if ((strcmp(argv[i], "--cccfloor") == 0) || (strcmp(argv[i], "--cccfl") == 0)){
                expect = 18;
            }
            else if ((strcmp(argv[i], "--cccceiling") == 0) || (strcmp(argv[i], "--ccccl") == 0)){
                expect = 19;
            }
            else if ((strcmp(argv[i], "--cccexponent") == 0) || (strcmp(argv[i], "--cccxp") == 0)){
                expect = 20;
            }
            else if ((strcmp(argv[i], "--spiral-carisma") == 0) || (strcmp(argv[i], "--sc") == 0)){
                expect = 21;
            }
            else if ((strcmp(argv[i], "--scfunction") == 0) || (strcmp(argv[i], "--scf") == 0)){
                expect = 22;
            }
            else if ((strcmp(argv[i], "--scfloor") == 0) || (strcmp(argv[i], "--scfl") == 0)){
                expect = 23;
            }
            else if ((strcmp(argv[i], "--scceiling") == 0) || (strcmp(argv[i], "--sccl") == 0)){
                expect = 24;
            }
            else if ((strcmp(argv[i], "--scexponent") == 0) || (strcmp(argv[i], "--scxp") == 0)){
                expect = 25;
            }
            else if ((strcmp(argv[i], "--scmax") == 0) || (strcmp(argv[i], "--scm") == 0)){
                expect = 26;
            }
            else if ((strcmp(argv[i], "--crtemu") == 0)){
                expect = 27;
            }
            else if ((strcmp(argv[i], "--crtblack") == 0)){
                expect = 28;
            }
            else if ((strcmp(argv[i], "--crtwhite") == 0)){
                expect = 29;
            }
            else if ((strcmp(argv[i], "--crtmod") == 0)){
                expect = 30;
            }
            else if ((strcmp(argv[i], "--crtdemod") == 0)){
                expect = 31;
            }
            else if ((strcmp(argv[i], "--lutgen") == 0)){
                expect = 32;
            }
            else if ((strcmp(argv[i], "--lutsize") == 0)){
                expect = 33;
            }
            else if ((strcmp(argv[i], "--eilut") == 0)){
                expect = 34;
            }
            else if ((strcmp(argv[i], "--nespalgen") == 0)){
                expect = 35;
            }
            else if ((strcmp(argv[i], "--nespalmode") == 0)){
                expect = 36;
            }
            else if ((strcmp(argv[i], "--nesburstnorm") == 0)){
                expect = 37;
            }
            else if ((strcmp(argv[i], "--nesskew26a") == 0)){
                expect = 38;
            }
            else if ((strcmp(argv[i], "--nesboost48c") == 0)){
                expect = 39;
            }
            else if ((strcmp(argv[i], "--nesperlumaskew") == 0)){
                expect = 40;
            }
            else {
                printf("Invalid parameter: ||%s||\n", argv[i]);
                return ERROR_BAD_PARAM_UNKNOWN_PARAM;
            }
        }
    }
    
    if (expect > 0){
        printf("Missing parameter. Expecting ");
        switch (expect){
            case 1:
                printf("input file name.\n");
                break;
            case 2:
                printf("output file name.\n");
                break;
            case 3:
                printf("input color.\n");
                break;
            case 4:
                printf("souce gamut name.\n");
                break;
            case 5:
                printf("destination gamut name.\n");
                break;
            case 6:
                printf("mapping mode.\n");
                break;
            case 7:
                printf("remap factor.\n");
                break;
            case 8:
                printf("remap limit.\n");
                break;
            case 9:
                printf("knee factor.\n");
                break;
             case 10:
                printf("remap direction.\n");
                break;
             case 11:
                printf("safe zone type.\n");
                break;
            case 12:
                printf("knee type.\n");
                break;
            case 13:
                printf("gamma function.\n");
                break;
            case 14:
                printf("dither setting.\n");
                break;
            case 15:
                printf("verbosity level.\n");
                break;
            case 16:
                printf("chromatic adapation type.\n");
                break;
            case 17:
                printf("ccc function.\n");
                break;
            case 18:
                printf("ccc floor.\n");
                break;
            case 19:
                printf("ccc ceiling.\n");
                break;
            case 20:
                printf("ccc exponent.\n");
                break;
            case 21:
                printf("Spiral CARISMA setting.\n");
                break;
            case 22:
                printf("Spiral CARISMA scaling function.\n");
                break;
            case 23:
                printf("Spiral CARISMA scaling function floor.\n");
                break;
            case 24:
                printf("Spiral CARISMA scaling function ceiling.\n");
                break;
            case 25:
                printf("Spiral CARISMA scaling function exponent.\n");
                break;
            case 26:
                printf("Spiral CARISMA max rotation multiplier.\n");
                break;
            case 27:
                printf("CRT emulation mode.\n");
                break;
            case 28:
                printf("CRT emulation black level.\n");
                break;
            case 29:
                printf("CRT emulation white level.\n");
                break;
            case 30:
                printf("CRT emulation modulator chip ID.\n");
                break;
            case 31:
                printf("CRT emulation demodulator chip ID.\n");
                break;
            case 32:
                printf("LUT generation mode (true/false).\n");
                break;
            case 33:
                printf("LUT size (integer).\n");
                break;
            case 34:
                printf("expanded intermediate LUT mode (true/false).\n");
                break;
            case 35:
                printf("NES palette generation mode (true/false).\n");
                break;
            case 36:
                printf("NES simulate PAL phase alternation (true/false).\n");
                break;
            case 37:
                printf("NES normalize chroma to colorburst (true/false).\n");
                break;
            case 38:
                printf("NES phase skew for hues 0x2, 0x6, and 0xA.\n");
                break;
            case 39:
                printf("NES luma boost for hues 0x4, 0x8, and 0xC.\n");
                break;
            case 40:
                printf("NES luma-dependent phase skew.\n");
                break;
            default:
                printf("oh... er... wtf error!.\n");
        }
        return ERROR_BAD_PARAM_MISSING_VALUE;
    }
    
    if (nesmode && lutgen){
        printf("\nForcing lutgen to false because nespalgen is true.\n");
        lutgen = false;
    }
    if (nesmode && filemode){
        printf("\nIgnoring input file because nespalgen is true.\n");
        filemode = false;
    }
    if (nesmode && spiralcarisma){
        printf("\nForcing spiral-carisma to false because nespalgen is true.\n");
        spiralcarisma = false;
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
            if (gammamode){
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
    
    //TODO: screen barf params in verbos emode
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
        }
        else {
            printf("Input color: %s\n", inputcolorstring);
        }
        if (gammamode){
            printf("Gamma function: srgb\n");
        }
        else {
            printf("Gamma function: linear\n");
        }
        printf("Source gamut: %s\n", gamutnames[sourcegamutindex].c_str());
        printf("Destination gamut: %s\n", gamutnames[destgamutindex].c_str());
        /*
        printf("Source gamut: ");
        switch(sourcegamutindex){
            case GAMUT_SRGB:
                printf("srgb\n");
                break;
            case GAMUT_NTSCJ_R:
                printf("ntscjr (ntscj)\n");
                break;
            case GAMUT_NTSCJ_B:
                printf("ntscjb\n");
                break;
            case GAMUT_NTSCJ_P22:
                printf("ntscj p22\n");
                break;
            case GAMUT_SMPTEC:
                printf("smptec\n");
                break;
            case GAMUT_EBU:
                printf("ebu\n");
                break;
            default:
                break;
        };
        printf("Destination gamut: ");
        switch(destgamutindex){
            case GAMUT_SRGB:
                printf("srgb\n");
                break;
            case GAMUT_NTSCJ_R:
                printf("ntscjr (ntscj)\n");
                break;
            case GAMUT_NTSCJ_B:
                printf("ntscjb\n");
                break;
            case GAMUT_NTSCJ_P22:
                printf("ntscj p22\n");
                break;
            case GAMUT_SMPTEC:
                printf("smptec\n");
                break;
            case GAMUT_EBU:
                printf("ebu\n");
                break;
            default:
                break;
        };
        */
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
    
    
    if (!initializeInverseMatrices()){
        printf("Unable to initialize inverse Jzazbz matrices. WTF error!\n");
        return ERROR_INVERT_MATRIX_FAIL;
    }
    
    crtdescriptor emulatedcrt;
    int sourcegamutcrtsetting = CRT_EMU_NONE;
    int destgamutcrtsetting = CRT_EMU_NONE;
    if (crtemumode != CRT_EMU_NONE){
        emulatedcrt.Initialize(crtblacklevel, crtwhitelevel, crtmodindex, crtdemodindex, verbosity);
        if (crtemumode == CRT_EMU_FRONT){
            sourcegamutcrtsetting = CRT_EMU_FRONT;
        }
        else if (crtemumode == CRT_EMU_BACK){
            destgamutcrtsetting = CRT_EMU_BACK;
        }
    }
    
    vec3 sourcewhite = vec3(gamutpoints[sourcegamutindex][0][0], gamutpoints[sourcegamutindex][0][1], gamutpoints[sourcegamutindex][0][2]);
    vec3 sourcered = vec3(gamutpoints[sourcegamutindex][1][0], gamutpoints[sourcegamutindex][1][1], gamutpoints[sourcegamutindex][1][2]);
    vec3 sourcegreen = vec3(gamutpoints[sourcegamutindex][2][0], gamutpoints[sourcegamutindex][2][1], gamutpoints[sourcegamutindex][2][2]);
    vec3 sourceblue = vec3(gamutpoints[sourcegamutindex][3][0], gamutpoints[sourcegamutindex][3][1], gamutpoints[sourcegamutindex][3][2]);
    
    vec3 destwhite = vec3(gamutpoints[destgamutindex][0][0], gamutpoints[destgamutindex][0][1], gamutpoints[destgamutindex][0][2]);
    vec3 destred = vec3(gamutpoints[destgamutindex][1][0], gamutpoints[destgamutindex][1][1], gamutpoints[destgamutindex][1][2]);
    vec3 destgreen = vec3(gamutpoints[destgamutindex][2][0], gamutpoints[destgamutindex][2][1], gamutpoints[destgamutindex][2][2]);
    vec3 destblue = vec3(gamutpoints[destgamutindex][3][0], gamutpoints[destgamutindex][3][1], gamutpoints[destgamutindex][3][2]);
    
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
    

    // this mode converts a single color and printfs the result
    if (!filemode && !nesmode){
        int redout;
        int greenout;
        int blueout;
        
        vec3 outcolor = processcolor(inputcolor, gammamode, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, false, false);
        /*
        vec3 linearinputcolor = (gammamode) ? vec3(tolinear(inputcolor.x), tolinear(inputcolor.y), tolinear(inputcolor.z)) : inputcolor;
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
        else {
            outcolor = mapColor(linearinputcolor, sourcegamut, destgamut, (mapmode == MAP_EXPAND), remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype);
        }
        if (gammamode){
            outcolor = vec3(togamma(outcolor.x), togamma(outcolor.y), togamma(outcolor.z));
        }
        */
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
    else if (nesmode){

        printf("Generating NES palette and saving result to %s...\n", outputfilename);

        std::ofstream palfile(outputfilename, std::ios::out | std::ios::binary);
        if (!palfile.is_open()){
            printf("Unable to open %s for writing.\n", outputfilename);
            return ERROR_PNG_OPEN_FAIL;
        }

        nesppusimulation nessim;
        nessim.Initialize(verbosity, nesispal, nescbc, nesskew26A, nesboost48C, nesskewstep);
        for (int emp=0; emp<8; emp++){
            for (int luma=0; luma<4; luma++){
                for (int hue=0; hue < 16; hue++){
                    vec3 nesrgb = nessim.NEStoRGB(hue,luma, emp);
                    vec3 outcolor = processcolor(nesrgb, gammamode, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, eilut, true);
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
                }
            }
        }
        palfile.flush();
        palfile.close();
        printf("done.\n");
        return RETURN_SUCCESS;
    }

    // if we didn't just return, we are in file mode

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
                            
                            outcolor = processcolor(inputcolor, gammamode, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, eilut, false);

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
                    fprintf(stderr, "ntscjpng: write %s: %s\n", outputfilename, image.message);
                    result = ERROR_PNG_WRITE_FAIL;
                }
            }

            else {
                fprintf(stderr, "ntscjpng: read %s: %s\n", inputfilename, image.message);
                result = ERROR_PNG_READ_FAIL;
            }

            free(buffer);
            buffer = NULL;
        }

        else {
            fprintf(stderr, "ntscjpng: out of memory: %lu bytes\n", (unsigned long)PNG_IMAGE_SIZE(image));
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
        fprintf(stderr, "ntscjpng: %s: %s\n", inputfilename, image.message);
        result = ERROR_PNG_OPEN_FAIL;
    }
   
   return result;
}
