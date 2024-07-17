#include <string.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <math.h>

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

vec3 processcolor(vec3 inputcolor, bool gammamode, int mapmode, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, int cccfunctiontype, double cccfloor, double cccceiling, double cccexp, double remapfactor, double remaplimit, bool softkneemode, double kneefactor, int mapdirection, int safezonetype, bool spiralcarisma){
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
        outcolor = mapColor(linearinputcolor, sourcegamut, destgamut, (mapmode == MAP_EXPAND), remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma);
    }
    if (gammamode){
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
    double crtblacklevel = 0.001;
    double crtwhitelevel = 1.0;
    
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
        else if ((expect == 7) || (expect == 8) || (expect == 9) || (expect == 18) || (expect == 19) || (expect == 20) || (expect == 23) || (expect == 24) || (expect == 25) || (expect == 28) || (expect == 29)){
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
                    default:
                        break;
                };
            }
            else {
                printf("Invalid parameter for remap factor, remap limit, or knee factor. (Malformed float.)");
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
            else {
                printf("Invalid parameter for mapping direction. Expecting \"cusp\", \"hlpcm\", \"vp\", or \"vpr\".\n");
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
                return ERROR_BAD_PARAM_VERBOSITY;
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
                scfunctiontype = CRT_EMU_BACK;
            }
            else {
                printf("Invalid parameter for CRT emulation mode. Expecting \"none\", \"front\", or \"back\".\n");
                return ERROR_BAD_PARAM_CRT_EMU_MODE;
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
            default:
                printf("oh... er... wtf error!.\n");
        }
        return ERROR_BAD_PARAM_MISSING_VALUE;
    }
    
    if (filemode){
        bool failboat = false;
        if (!infileset){
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
            printf("Input file: %s\nOutput file: %s\n", inputfilename, outputfilename);
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
            // TODO: print modulator and demodulator info here
        }
        printf("Verbosity: %i\n", verbosity);
        printf("----------\n\n");
    }
    
    
    if (!initializeInverseMatrices()){
        printf("Unable to initialize inverse Jzazbz matrices. WTF error!\n");
        return ERROR_INVERT_MATRIX_FAIL;
    }
    
    crtdescriptor emulatedcrt;
    if (crtemumode != CRT_EMU_NONE){
        emulatedcrt.Initialize(crtblacklevel, crtwhitelevel, 1, verbosity);
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
    bool srcOK = sourcegamut.initialize(gamutnames[sourcegamutindex], sourcewhite, sourcered, sourcegreen, sourceblue, destwhite, true, verbosity, adapttype, compressenabled);
    
    gamutdescriptor destgamut;
    bool destOK = destgamut.initialize(gamutnames[destgamutindex], destwhite, destred, destgreen, destblue, sourcewhite, false, verbosity, adapttype, compressenabled);
    
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
    if (!filemode){
        int redout;
        int greenout;
        int blueout;
        
        vec3 outcolor = processcolor(inputcolor, gammamode, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma);
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

    // if we didn't just return, we are in file mode
    
    int result = ERROR_PNG_FAIL;
    png_image image;

    
    /* Only the image structure version number needs to be set. */
    memset(&image, 0, sizeof image);
    image.version = PNG_IMAGE_VERSION;

    if (png_image_begin_read_from_file(&image, inputfilename)){
        png_bytep buffer;

        /* Change this to try different formats!  If you set a colormap format
        * then you must also supply a colormap below.
        */
        image.format = PNG_FORMAT_RGBA;

        buffer = (png_bytep) malloc(PNG_IMAGE_SIZE(image));  //c++ wants an explict cast

        if (buffer != NULL){
            if (png_image_finish_read(&image, NULL/*background*/, buffer, 0/*row_stride*/, NULL/*colormap for PNG_FORMAT_FLAG_COLORMAP */)){
                
                // ------------------------------------------------------------------------------------------------------------------------------------------
                // Begin actual color conversion code
                
                // zero the memos
                memset(&memos, 0, 256 * 256 * 256 * sizeof(memo));
                
                if (verbosity >= VERBOSITY_MINIMAL){
                    printf("Doing gamut conversion on %s and saving result to %s...\n", inputfilename, outputfilename); 
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
                        
                        // read bytes from buffer
                        png_byte redin = buffer[ ((y * width) + x) * 4];
                        png_byte greenin = buffer[ (((y * width) + x) * 4) + 1 ];
                        png_byte bluein = buffer[ (((y * width) + x) * 4) + 2 ];
                        
                        vec3 outcolor;
                        
                        // if we've already processed the same input color, just recall the memo
                        if (memos[redin][greenin][bluein].known){
                            outcolor = memos[redin][greenin][bluein].data;
                        }
                        else {
                        
                            // convert to double
                            double redvalue = redin/255.0;
                            double greenvalue = greenin/255.0;
                            double bluevalue = bluein/255.0;
                            // don't touch alpha value
                            
                            vec3 inputcolor = vec3(redvalue, greenvalue, bluevalue);
                            
                            outcolor = processcolor(inputcolor, gammamode, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma);
                            
                            /*
                            // to linear RGB
                            if (gammamode){
                                // The FF7 videos had banding near black when decoded with any piecewise "toe slope" gamma function, suggesting that a pure curve function was needed. May need to try this if such banding appears.
                                redvalue = tolinear(redvalue);
                                greenvalue = tolinear(greenvalue);
                                bluevalue = tolinear(bluevalue);
                            }                           
                            
                            vec3 linearRGB = vec3(redvalue, greenvalue, bluevalue);
                            
                            // if clipping, just do the matrix math
                            // (the gamutdescriptors know if they need to do a Bradford (Von Kries) transform
                            if (mapmode == MAP_CLIP){
                                vec3 tempcolor = sourcegamut.linearRGBtoXYZ(linearRGB);
                                outcolor = destgamut.XYZtoLinearRGB(tempcolor);
                            }
                            else if (mapmode == MAP_CCC_A){
                                vec3 tempcolor = sourcegamut.linearRGBtoXYZ(linearRGB);
                                outcolor = destgamut.XYZtoLinearRGB(tempcolor);
                                double maxP = sourcegamut.linearRGBfindmaxP(linearRGB);
                                double oldweight = 0.0;
                                if (cccfunctiontype == CCC_EXPONENTIAL){
                                    oldweight = powermap(cccfloor, cccceiling, maxP, cccexp);
                                }
                                else if (cccfunctiontype == CCC_CUBIC_HERMITE){
                                    oldweight = cubichermitemap(cccfloor, cccceiling, maxP);
                                }
                                double newweight = 1.0 - oldweight;
                                outcolor = (linearRGB * oldweight) + (outcolor * newweight);
                            }
                            // otherwise fire up the gamut mapping algorithm
                            else {
                                outcolor = mapColor(linearRGB, sourcegamut, destgamut, (mapmode == MAP_EXPAND), remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype);
                            }
                            
                            // back to sRGB
                            if (gammamode){
                                outcolor = vec3(togamma(outcolor.x), togamma(outcolor.y), togamma(outcolor.z));
                            }
                            */
                            
                            // memoize the result of the conversion so we don't need to do it again for this input color
                            memos[redin][greenin][bluein].known = true;
                            memos[redin][greenin][bluein].data = outcolor;
                                                    
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
