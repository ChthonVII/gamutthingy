#include <string.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <math.h>
#include <fstream>
#include <deque>
#include <iomanip>
#include <numeric>
#include <mutex>

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
#include "BS_thread_pool.hpp"

void printhelp(){
    printf("THIS HELP IS EXTREMELY OUT OF DATE. REFER TO https://github.com/ChthonVII/gamutthingy/blob/master/README.md INSTEAD!!\n\nUsage is:\n\n`--help` or `-h`: Displays help.\n\n`--color` or `-c`: Specifies a single color to convert. A message containing the result will be printed to stdout. Should be a \"0x\" prefixed hexadecimal representation of an RGB8 color. For example: `0xFABF00`.\n\n`--infile` or `-i`: Specifies an input file. Should be a .png image.\n\n`--outfile` or `-o`: Specifies an input file. Should be a .png image.\n\n`--gamma` or `-g`: Specifies the gamma function (and inverse) to be applied to the input and output. Possible values are `srgb` (default) and `linear`. LUTs for FFNx should be created using linear RGB. Images should generally be converted using the sRGB gamma function.\n\n`--source-gamut` or `-s`: Specifies the source gamut. Possible values are:\n\t`srgb`: The sRGB gamut used by (SDR) modern computer monitors. Identical to the bt709 gamut used for modern HD video.\n\t`ntscj`: alias for `ntscjr`.\n\t`ntscjr`: The variant of the NTSC-J gamut used by Japanese CRT television sets, official specification. (whitepoint 9300K+27mpcd) Default.\n\t`ntscjp22`: NTSC-J gamut as derived from average measurements conducted on Japanese CRT television sets with typical P22 phosphors. (whitepoint 9300K+27mpcd) Deviates significantly from the specification, which was usually compensated for by a \"color correction circuit.\" See readme for details.\n\t`ntscjb`: The variant of the NTSC-J gamut used for SD Japanese television broadcasts, official specification. (whitepoint 9300K+8mpcd)\n\t`smptec`: The SMPTE-C gamut used for American CRT television sets/broadcasts and the bt601 video standard.\n\t`ebu`: The EBU gamut used in the European 470bg television/video standards (PAL).\n\n`--dest-gamut` or `-d`: Specifies the destination gamut. Possible values are the same as for source gamut. Default is `srgb`.\n\n`--adapt` or `-a`: Specifies the chromatic adaptation method to use when changing white points. Possible values are `bradford` and `cat16` (default).\n\n`--map-mode` or `-m`: Specifies gamut mapping mode. Possible values are:\n\t`clip`: No gamut mapping is performed and linear RGB output is simply clipped to 0, 1. Detail in the out-of-bounds range will be lost.\n\t`compress`: Uses a gamut (compression) mapping algorithm to remap out-of-bounds colors to a smaller zone inside the gamut boundary. Also remaps colors originally in that zone to make room. Essentially trades away some colorimetric fidelity in exchange for preserving some of the out-of-bounds detail. Default.\n\t`expand`: Same as `compress` but also applies the inverse of the compression function in directions where the destination gamut boundary exceeds the source gamut boundary. (Also, reverses the order of the steps in the `vp` and `vpr` algorithms.) The only use for this is to prepare an image for a \"roundtrip\" conversion. For example, if you want to display a sRGB image as-is in FFNx's NTSC-J mode, you would convert from sRGB to NTSC-J using `expand` in preparation for FFNx doing the inverse operation.\n\n`--gamut-mapping-algorithm` or `--gma`: Specifies which gamut mapping algorithm to use. (Does nothing if `--map-mode clip`.) Possible values are:\n\t`cusp`: The CUSP algorithm, but with tunable compression parameters. See readme for details.\n\t`hlpcm`: The HLPCM algorithm, but with tunable compression parameters. See readme for details.\n\t`vp`: The VP algorithm, but with linear light scaling and tunable compression parameters. See readme for details.\n\t`vpr`: VPR algorithm, a modification of VP created for gamutthingy. The modifications are explained in the readme. Default.\n\n`--safe-zone-type` or `-z`: Specifies how the outer zone subject to remapping and the inner \"safe zone\" exempt from remapping are defined. Possible values are:\n\t`const-fidelity`: The zones are defined relative to the distance from the \"center of gravity\" to the destination gamut boundary. Yields consistent colorimetric fidelity, with variable detail preservation.\n\t`const-detail`: The remapping zone is defined relative to the difference between the distances from the \"center of gravity\" to the source and destination gamut boundaries. As implemented here, an overriding minimum size for the \"safe zone\" (relative to the destination gamut boundary) may also be enforced. Yields consistent detail preservation, with variable colorimetric fidelity (setting aside the override option). Default.\n\n`--remap-factor` or `--rf`: Specifies the size of the remapping zone relative to the difference between the distances from the \"center of gravity\" to the source and destination gamut boundaries. (Does nothing if `--safe-zone-type const-fidelity`.) Default 0.4.\n\n`--remap-limit` or `--rl`: Specifies the size of the safe zone (exempt from remapping) relative to the distance from the \"center of gravity\" to the destination gamut boundary. If `--safe-zone-type const-detail`, this serves as a minimum size limit when application of `--remap-factor` would lead to a smaller safe zone. Default 0.9.\n\n`--knee` or `-k`: Specifies the type of knee function used for compression, `hard` or `soft`. Default `soft`.\n\n`--knee-factor` or `--kf`: Specifies the width of the soft knee relative to the size of the remapping zone. (Does nothing if `--knee hard`.) Note that the soft knee is centered at the knee point, so half the width extends into the safe zone, thus expanding the area that is remapped. Default 0.4.\n\n`--dither` or `--di`: Specifies whether to apply dithering to the ouput, `true` or `false`. Uses Martin Roberts' quasirandom dithering algorithm. Dithering should be used for images in general, but should not be used for LUTs.  Default `true`.\n\n`--verbosity` or `-v`: Specify verbosity level. Integers 0-5. Default 2.\n");
    return;
}

typedef struct memo{
    bool known;
    vec3 data;
} memo;
    
// keep memos so we don't have to process the same color over and over in file mode
// this has to be global because it's too big for the stack
memo memos[256][256][256];

std::mutex memomtx;
std::mutex printfmtx;
std::mutex progressbarmtx;

// visited list for backwards search
bool inversesearchvisitlist[256][256][256];

// Do the full conversion process on a given color
vec3 processcolor(vec3 inputcolor, int gammamodein, double gammapowin, int gammamodeout, double gammapowout, int mapmode, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, int cccfunctiontype, double cccfloor, double cccceiling, double cccexp, double remapfactor, double remaplimit, bool softkneemode, double kneefactor, int mapdirection, int safezonetype, bool spiralcarisma, int lutmode, bool nesmode, double hdrsdrmaxnits){
    vec3 linearinputcolor = inputcolor;
    if (sourcegamut.crtemumode == CRT_EMU_FRONT){
        if (lutmode == LUTMODE_POSTCC){
            if (sourcegamut.attachedCRT->globalgammaadjust != 1.0){
                bool flip = (linearinputcolor.x < 0.0);
                if (flip){
                    linearinputcolor.x *= -1.0;
                }
                linearinputcolor.x = pow(linearinputcolor.x, sourcegamut.attachedCRT->globalgammaadjust);
                if (flip){
                    linearinputcolor.x *= -1.0;
                }
                flip = (linearinputcolor.y < 0.0);
                if (flip){
                    linearinputcolor.y *= -1.0;
                }
                linearinputcolor.y = pow(linearinputcolor.y, sourcegamut.attachedCRT->globalgammaadjust);
                if (flip){
                    linearinputcolor.y *= -1.0;
                }
                flip = (linearinputcolor.z < 0.0);
                if (flip){
                    linearinputcolor.z *= -1.0;
                }
                linearinputcolor.z = pow(linearinputcolor.z, sourcegamut.attachedCRT->globalgammaadjust);
                if (flip){
                    linearinputcolor.z *= -1.0;
                }
            }
            linearinputcolor = sourcegamut.attachedCRT->tolinear1886appx1vec3(inputcolor);
        }
        else if ((lutmode == LUTMODE_NONE)||(lutmode == LUTMODE_NORMAL)) {
            linearinputcolor = sourcegamut.attachedCRT->CRTEmulateGammaSpaceRGBtoLinearRGB(inputcolor);
        }
        // do nothing if lutmode == LUTMODE_POSTGAMMA, since input color is already linear RGB
    }
    else if (gammamodein == GAMMA_SRGB){
        linearinputcolor = vec3(tolinear(inputcolor.x), tolinear(inputcolor.y), tolinear(inputcolor.z));
    }
    else if (gammamodein == GAMMA_REC2084){
        linearinputcolor = vec3(rec2084tolinear(inputcolor.x, hdrsdrmaxnits), rec2084tolinear(inputcolor.y, hdrsdrmaxnits), rec2084tolinear(inputcolor.z, hdrsdrmaxnits));
    }
    else if (gammamodein == GAMMA_POWER){
        linearinputcolor = vec3(pow(inputcolor.x, gammapowin), pow(inputcolor.y, gammapowin), pow(inputcolor.z, gammapowin));
    }
    
    // Expanded intermediate LUTs expressly contain a ton of out-of-bounds colors
    // XYZtoJzazbz() will force colors to black if luminosity is negative at that point.
    // CUSP and VPR-alike GMA algorithms will pull down stuff that's above 1.0 luminosity.
    // But desaturation-only algorithms (HLPCM) must have luminosity clamped.
    // NES may also have out-of-bounds luminosity
    if (((lutmode == LUTMODE_POSTCC) || nesmode) && (mapdirection == MAP_HLPCM)){
        linearinputcolor = sourcegamut.ClampLuminosity(linearinputcolor);
    }

    //printf("Linear input is ");
    //linearinputcolor.printout();
    //printf("\n");

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
        //printf("Linear output is ");
        //outcolor.printout();
        //printf("\n");
    }
    if (destgamut.crtemumode == CRT_EMU_BACK){
        outcolor = destgamut.attachedCRT->CRTEmulateLinearRGBtoGammaSpaceRGB(outcolor, true);
    }
    else if (gammamodeout == GAMMA_SRGB){
        outcolor = vec3(togamma(outcolor.x), togamma(outcolor.y), togamma(outcolor.z));
    }
    else if (gammamodeout == GAMMA_REC2084){
        outcolor = vec3(rec2084togamma(outcolor.x, hdrsdrmaxnits), rec2084togamma(outcolor.y, hdrsdrmaxnits), rec2084togamma(outcolor.z, hdrsdrmaxnits));
    }
    else if (gammamodeout == GAMMA_POWER){
        double powout = 1.0/gammapowout;
        outcolor = vec3(pow(outcolor.x, powout), pow(outcolor.y, powout), pow(outcolor.z, powout));
    }
    return outcolor;
}

// Search backwards for an input that yields the chosen output when run through processcolor(),
// Or closest possible if none exists.
// WARNING: VERY SLOW!!!
vec3 inverseprocesscolor(vec3 inputcolor, int gammamodein, double gammapowin, int gammamodeout, double gammapowout, int mapmode, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, int cccfunctiontype, double cccfloor, double cccceiling, double cccexp, double remapfactor, double remaplimit, bool softkneemode, double kneefactor, int mapdirection, int safezonetype, bool spiralcarisma, int lutmode, bool nesmode, double hdrsdrmaxnits){

    typedef struct frontiernode{
        unsigned int red;
        unsigned int green;
        unsigned int blue;
    } frontiernode;


    int goalred = toRGB8nodither(inputcolor.x);
    int goalgreen = toRGB8nodither(inputcolor.y);
    int goalblue = toRGB8nodither(inputcolor.z);


    // get our goal into Jzazbz so we can measure error
    vec3 tempgoal = inputcolor;
    // reverse the output gamma
    if (destgamut.crtemumode == CRT_EMU_BACK){
        tempgoal = destgamut.attachedCRT->CRTEmulateGammaSpaceRGBtoLinearRGB(tempgoal);
    }
    else if (gammamodeout == GAMMA_SRGB){
        tempgoal = vec3(tolinear(tempgoal.x), tolinear(tempgoal.y), tolinear(tempgoal.z));
    }
    else if (gammamodeout == GAMMA_REC2084){
        tempgoal = vec3(rec2084tolinear(tempgoal.x, hdrsdrmaxnits), rec2084tolinear(tempgoal.y, hdrsdrmaxnits), rec2084tolinear(tempgoal.z, hdrsdrmaxnits));
    }
    else if (gammamodeout == GAMMA_POWER){
        tempgoal = vec3(pow(tempgoal.x, gammapowout), pow(tempgoal.y, gammapowout), pow(tempgoal.z, gammapowout));
    }
    vec3 goalJzazbz = destgamut.linearRGBtoJzazbz(tempgoal);


    std::deque<frontiernode> frontier;
    frontiernode bestnode;
    double bestdistlist[54];
    double bestdistrgblist[54];
    for (int i=0; i<54; i++){
        bestdistlist[i] = 1000000000.0; //impossibly big
        bestdistrgblist[i] = 500; //impossibly big
    }

    // clear the visited list
    // this is too big for stack, so it's global
    memset(&inversesearchvisitlist, 0, sizeof(bool) * 256 * 256 * 256);

    // start with the goal as the first guess, since it's probably close to the right answer
    frontiernode tempnode;
    tempnode.red = goalred;
    tempnode.green = goalgreen;
    tempnode.blue = goalblue;
    bestnode = tempnode; //initialize to silence compile warning
    /*
    // well, sometimes it's not...
    // if the gamma doesn't match, let's at least fix that...

    // Actually, no. Tweaking the guess gives a nice speed up, but causes artifacts in the resulting LUT, presumably because we're getting stuck in local maxima.
    // So far, every attempt to fix the artifacts just makes more artifacts somewhere else.
    // So I'm just disabling this.
    if ((sourcegamut.crtemumode == CRT_EMU_FRONT) || (destgamut.crtemumode == CRT_EMU_BACK) || (gammamodein != gammamodeout)){
        // tempgoal already holds the output reversed back to linear
        vec3 betterguess = tempgoal;
        // now reverse the input gamma
        if (sourcegamut.crtemumode == CRT_EMU_FRONT){
            betterguess = sourcegamut.attachedCRT->CRTEmulateLinearRGBtoGammaSpaceRGB(betterguess, false);
        }
        else if (gammamodein == GAMMA_SRGB){
            betterguess = vec3(togamma(betterguess.x), togamma(betterguess.y), togamma(betterguess.z));
        }
        else if (gammamodein == GAMMA_REC2084){
            betterguess = vec3(rec2084togamma(betterguess.x, hdrsdrmaxnits), rec2084togamma(betterguess.y, hdrsdrmaxnits), rec2084togamma(betterguess.z, hdrsdrmaxnits));
        }
        else if (gammamodein == GAMMA_POWER){
            betterguess = vec3(pow(betterguess.x, 1.0/gammapowin), pow(betterguess.y, 1.0/gammapowin), pow(betterguess.z, 1.0/gammapowin));
        }
        int betterguessred = toRGB8nodither(betterguess.x);
        int betterguessgreen = toRGB8nodither(betterguess.y);
        int betterguessblue = toRGB8nodither(betterguess.z);
        tempnode.red = betterguessred;
        tempnode.green = betterguessgreen;
        tempnode.blue = betterguessblue;
    }
    // move the initial guess away from the edges so we're less likely to get stuck on a local maximum in a convex spot
    // but not if the guess has low chroma, since we don't want to mess with black and white
    unsigned int maxguess = (tempnode.red > tempnode.green) ? tempnode.red : tempnode.green;
    maxguess = (maxguess > tempnode.blue) ? maxguess : tempnode.blue;
    unsigned int minguess = (tempnode.red < tempnode.green) ? tempnode.red : tempnode.green;
    minguess = (minguess < tempnode.blue) ? minguess : tempnode.blue;
    unsigned int chroma = maxguess - minguess;
    if (chroma > 30){
        tempnode.red = (tempnode.red > 245) ? 245 : tempnode.red;
        tempnode.green = (tempnode.green > 245) ? 245 : tempnode.green;
        tempnode.blue = (tempnode.blue > 245) ? 245 : tempnode.blue;
        tempnode.red = (tempnode.red < 10) ? 10 : tempnode.red;
        tempnode.green = (tempnode.green < 10) ? 10 : tempnode.green;
        tempnode.blue = (tempnode.blue < 10) ? 10 : tempnode.blue;
    }
    */
    frontier.push_back(tempnode);


    //printf("\nstarting search. goal is %i, %i, %i, (Jzazbz: %f, %f, %f)\n", goalred, goalgreen, goalblue, goalJzazbz.x, goalJzazbz.y, goalJzazbz.z);
    // for as long as we have something left to check in the frontier, check one
    while(!frontier.empty()){
        // pop the front of the queue
        frontiernode examnode = frontier.front();
        frontier.pop_front();
        //printf("popped %i, %i, %i\n", examnode.red, examnode.green, examnode.blue);

        // skip if we've already visited this node (this should never happen)
        if (inversesearchvisitlist[examnode.red][examnode.green][examnode.blue]){
            //printf("\tskipping because visited\n");
            continue;
        }

        // mark as visited
        inversesearchvisitlist[examnode.red][examnode.green][examnode.blue] = true;

        // evaluate the current color
        vec3 testcolor;

        testcolor.x = examnode.red/255.0;
        testcolor.y = examnode.green/255.0;
        testcolor.z = examnode.blue/255.0;

        vec3 testtresult = processcolor(testcolor, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, nesmode, hdrsdrmaxnits);

        // quantize and see how far off we are in RGB space
        int testresultred = toRGB8nodither(testtresult.x);
        int testresultgreen = toRGB8nodither(testtresult.y);
        int testresultblue = toRGB8nodither(testtresult.z);

        // did we hit exactly?
        if ((testresultred == goalred) && (testresultgreen == goalgreen) && (testresultblue == goalblue)){
            bestnode = examnode;
            //printf("\tEXACT HIT!\n");
            break;
        }

        // are we the best so far?
        // figure rgb error
        int deltared = testresultred - goalred;
        int deltagreen = testresultgreen - goalgreen;
        int deltablue = testresultblue - goalblue;
        double testdistancergb = sqrt((deltared * deltared) + (deltagreen * deltagreen) + (deltablue * deltablue));
        // figure Jzazbz error
        // reverse the output gamma
        //printf("\trgb errors: %i, %i, %i, overall: %f\n", deltared, deltagreen, deltablue, testdistancergb);
        vec3 testtresultlinear = testtresult;
        if (destgamut.crtemumode == CRT_EMU_BACK){
            testtresultlinear = destgamut.attachedCRT->CRTEmulateGammaSpaceRGBtoLinearRGB(testtresultlinear);
        }
        else if (gammamodeout == GAMMA_SRGB){
            testtresultlinear = vec3(tolinear(testtresultlinear.x), tolinear(testtresultlinear.y), tolinear(testtresultlinear.z));
        }
        else if (gammamodeout == GAMMA_REC2084){
            testtresultlinear = vec3(rec2084tolinear(testtresultlinear.x, hdrsdrmaxnits), rec2084tolinear(testtresultlinear.y, hdrsdrmaxnits), rec2084tolinear(testtresultlinear.z, hdrsdrmaxnits));
        }
        else if (gammamodeout == GAMMA_POWER){
            testtresultlinear = vec3(pow(testtresultlinear.x, gammapowout), pow(testtresultlinear.y, gammapowout), pow(testtresultlinear.z, gammapowout));
        }
        vec3 testresultJzazbz = destgamut.linearRGBtoJzazbz(testtresultlinear);
        double deltaJz = testresultJzazbz.x - goalJzazbz.x;
        double deltaaz = testresultJzazbz.y - goalJzazbz.y;
        double deltabz = testresultJzazbz.z - goalJzazbz.z;
        double testdistance = sqrt((deltaJz * deltaJz) + (deltaaz * deltaaz) + (deltabz * deltabz));
        //printf("\tJzazbz is %f, %f, %f, off by %f\n", testresultJzazbz.x, testresultJzazbz.y, testresultJzazbz.z, testdistance);
        bool isbest = false;
        for (int i=0; i<54; i++){
            if (testdistance < bestdistlist[i]){
                if (i==0){
                    isbest = true;
                    bestnode = examnode;
                }
                for (int j=53; j>i; j--){
                    bestdistlist[j] = bestdistlist[j-1];
                    bestdistrgblist[j] = bestdistrgblist[j-1];
                }
                bestdistlist[i]=testdistance;
                bestdistrgblist[i]=testdistancergb;
                break;
            }
        }

        // Are we too far off the best to continue searching this direction?
        if (!isbest &&
                (
                    ((testdistance > (bestdistlist[0] * 1.2) && (testdistancergb > ceil(bestdistrgblist[0])+3.5)))
                    ||
                    ((testdistance > bestdistlist[53]) && (testdistancergb > bestdistrgblist[53]))
                )
        ){
            //printf("\tNOT queuing neighbors.\n");
            continue;
        }

        // If we're still here, then add neighbbors to the frontier
        //printf("\tqueuing neighbors\n");

        // figure out which rgb error is worst, and in what direction
        bool redplus = (deltared > 0);
        bool greenplus = (deltagreen > 0);
        bool blueplus = (deltablue > 0);
        bool redminus = (deltared < 0);
        bool greenminus = (deltagreen < 0);
        bool blueminus = (deltablue < 0);
        bool redzero = (deltared == 0);
        bool greenzero = (deltagreen == 0);
        bool bluezero = (deltablue == 0);
        deltared = abs(deltared);
        deltagreen = abs(deltagreen);
        deltablue = abs(deltablue);
        bool redworst = ((deltared > deltagreen) && (deltared > deltablue));
        bool greenworst = ((deltared < deltagreen) && (deltagreen > deltablue));
        bool blueworst = ((deltablue > deltagreen) && (deltared < deltablue));
        bool redonly = (redworst && ((deltared > deltagreen+2) && (deltared > deltablue+2)));
        bool greenonly = (greenworst && ((deltared+2 < deltagreen) && (deltagreen > deltablue+2)));
        bool blueonly = (blueworst && ((deltablue > deltagreen+2) && (deltared+2 < deltablue)));

        // if one axis has a much bigger error than the others, go that way and don't bother with other neighbors
        bool onedirection = false;
        frontiernode onedirectionnode = examnode;
        if (redonly){
            if (redplus){
                onedirectionnode.red--;
            }
            else if (redminus){
                onedirectionnode.red++;
            }
            if ((onedirectionnode.red >= 0) && (onedirectionnode.red <= 255)){
                onedirection = true;
            }
        }
        else if (greenonly){
            if (greenplus){
                onedirectionnode.green--;
            }
            else if (greenminus){
                onedirectionnode.green++;
            }
            if ((onedirectionnode.green >= 0) && (onedirectionnode.green <= 255)){
                onedirection = true;
            }
        }
        else if (blueonly){
            if (blueplus){
                onedirectionnode.blue--;
            }
            else if (blueminus){
                onedirectionnode.blue++;
            }
            if ((onedirectionnode.blue >= 0) && (onedirectionnode.blue <= 255)){
                onedirection = true;
            }
        }
        if (onedirection){
            //printf("\t\tWant to push %i, %i, %i as the ONLY DIRECTION\n", onedirectionnode.red, onedirectionnode.green, onedirectionnode.blue);
            bool skip = false;
            // skip if already visited
            if (inversesearchvisitlist[onedirectionnode.red][onedirectionnode.green][onedirectionnode.blue]){
                //printf("\t\tskipping already visited\n");
                skip = true;
            }
            // skip if already in the frontier queue
            for (unsigned int i=0; i<frontier.size(); i++){
                if ((onedirectionnode.red == frontier[i].red) && (onedirectionnode.green == frontier[i].green) && (onedirectionnode.blue == frontier[i].blue)){
                    //printf("\t\tskipping already queued\n");
                    skip = true;
                    break;
                }
            }
            if (!skip){
                //printf("\t\tpushing\n");
                if (isbest){
                    frontier.push_front(onedirectionnode);
                }
                else {
                    frontier.push_back(onedirectionnode);
                }
            }
        }
        // full neighbor search (not onedirection)
        else {

            std::deque<frontiernode> fourpointers;
            std::deque<frontiernode> threepointers;
            std::deque<frontiernode> twopointers;
            std::deque<frontiernode> onepointers;
            std::deque<frontiernode> zeropointers;
            int redscore = 0;
            int greenscore = 0;
            int bluescore = 0;
            int totalscore = 0;

            for (int offsetred = -1;  offsetred <= 1; offsetred++){
                int nextred = examnode.red + offsetred;
                // skip if out of bounds
                if ((nextred < 0) || (nextred > 255)) {
                    //printf("\t\tskipping red outof bounds.\n");
                    continue;
                }
                // skip if wrong direction relative to largest error
                if (redworst){
                    if (redplus && (offsetred > 0)){
                        //printf("\t\tskipping red wrong direction\n");
                        continue;
                    }
                    else if (redminus && (offsetred < 0)){
                        //printf("\t\tskipping red wrong direction\n");
                        continue;
                    }
                }
                // score this axis so we can roughly sort the possible moves before queuing them
                if ((redplus && (offsetred < 0)) || (redminus && (offsetred > 0)) || (redzero && (offsetred == 0))){
                    redscore = 1;
                    if (redworst){
                        redscore = 2;
                    }
                }
                else {
                    redscore = 0;
                }
                for (int offsetgreen = -1;  offsetgreen <= 1; offsetgreen++){
                    int nextgreen = examnode.green + offsetgreen;
                    // skip if out of bounds
                    if ((nextgreen < 0) || (nextgreen > 255)) {
                        //printf("\t\tskipping green outof bounds.\n");
                        continue;
                    }
                    // skip if wrong direction relative to largest error
                    if (greenworst){
                        if (greenplus && (offsetgreen > 0)){
                            //printf("\t\tskipping green wrong direction\n");
                            continue;
                        }
                        else if (greenminus && (offsetgreen < 0)){
                            //printf("\t\tskipping green wrong direction\n");
                            continue;
                        }
                    }
                    // score this axis so we can roughly sort the possible moves before queuing them
                    if ((greenplus && (offsetgreen < 0)) || (greenminus && (offsetgreen > 0)) || (greenzero && (offsetgreen == 0))){
                        greenscore = 1;
                        if (greenworst){
                            greenscore = 2;
                        }
                    }
                    else {
                        greenscore = 0;
                    }
                    for (int offsetblue = -1;  offsetblue <= 1; offsetblue++){
                        int nextblue = examnode.blue + offsetblue;
                        // skip if out of bounds
                        if ((nextblue < 0) || (nextblue > 255)) {
                            //printf("\t\tskipping blue outof bounds.\n");
                            continue;
                        }
                        // skip if wrong direction relative to largest error
                        if (blueworst){
                            if (blueplus && (offsetblue > 0)){
                                //printf("\t\tskipping blue wrong direction\n");
                                continue;
                            }
                        else if (blueminus && (offsetblue < 0)){
                                //printf("\t\tskipping blue wrong direction\n");
                                continue;
                            }
                        }

                        // score this axis so we can roughly sort the possible moves before queuing them
                        if ((blueplus && (offsetblue < 0)) || (blueminus && (offsetblue > 0)) || (bluezero && (offsetblue == 0))){
                            bluescore = 1;
                            if (blueworst){
                                bluescore = 2;
                            }
                        }
                        else {
                            bluescore = 0;
                        }

                        // skip if all offsets are 0
                        if ((offsetred == 0) && (offsetgreen == 0) && (offsetblue == 0)){
                            //printf("\t\tskipping zero offsets\n");
                            continue;
                        }
                        // skip if already visited
                        if (inversesearchvisitlist[nextred][nextgreen][nextblue]){
                            //printf("\t\tskipping already visited\n");
                            continue;
                        }
                        // skip if already in the frontier queue
                        bool skipflag = false;
                        for (unsigned int i=0; i<frontier.size(); i++){
                            if (((unsigned int)nextred == frontier[i].red) && ((unsigned int)nextgreen == frontier[i].green) && ((unsigned int)nextblue == frontier[i].blue)){
                                skipflag = true;
                                break;
                            }
                        }
                        if (skipflag){
                            //printf("\t\tskipping already in queue\n");
                            continue;
                        }

                        // if we haven't skipped by this point, queue this neighbor
                        frontiernode nextnode;
                        nextnode.red = nextred;
                        nextnode.green = nextgreen;
                        nextnode.blue = nextblue;
                        /*
                        if (isbest){
                            frontier.push_front(nextnode);
                        }
                        else {
                            frontier.push_back(nextnode);
                        }
                        */
                        // push into temporary queues for binning
                        totalscore = redscore + greenscore + bluescore;
                        //printf("\t\tpushing %i, %i, %i score %i\n", nextred, nextgreen, nextblue, totalscore);
                        switch (totalscore){
                            case 4:
                                fourpointers.push_back(nextnode);
                                break;
                            case 3:
                                threepointers.push_back(nextnode);
                                break;
                            case 2:
                                twopointers.push_back(nextnode);
                                break;
                            case 1:
                                onepointers.push_back(nextnode);
                                break;
                            default:
                                zeropointers.push_back(nextnode);
                                break;
                        };


                    } // end for offsetblue
                } // end for offsetgreen
            } // end for offsetred

            // push our binned neighbors into the frontier queue
            // push to front if this is the best node so far
            if (isbest){
                // reverse order since we're pushing to front
                for (unsigned int i=0; i<zeropointers.size(); i++){
                    frontier.push_front(zeropointers[i]);
                }
                for (unsigned int i=0; i<onepointers.size(); i++){
                    frontier.push_front(onepointers[i]);
                }
                for (unsigned int i=0; i<twopointers.size(); i++){
                    frontier.push_front(twopointers[i]);
                }
                for (unsigned int i=0; i<threepointers.size(); i++){
                    frontier.push_front(threepointers[i]);
                }
                for (unsigned int i=0; i<fourpointers.size(); i++){
                    frontier.push_front(fourpointers[i]);
                }
            }
            else {
                for (unsigned int i=0; i<fourpointers.size(); i++){
                    frontier.push_back(fourpointers[i]);
                }
                for (unsigned int i=0; i<threepointers.size(); i++){
                    frontier.push_back(threepointers[i]);
                }
                for (unsigned int i=0; i<twopointers.size(); i++){
                    frontier.push_back(twopointers[i]);
                }
                for (unsigned int i=0; i<onepointers.size(); i++){
                    frontier.push_back(onepointers[i]);
                }
                for (unsigned int i=0; i<zeropointers.size(); i++){
                    frontier.push_back(zeropointers[i]);
                }
            }

        } //end else clause -- full neighbor search (not onedirection)

    } //end while !frontier.empty()


    // bestnode should now contain the input that yields the result closest to the goal output
    // convert to double so we can have same return type as processcolor()
    vec3 output = vec3(bestnode.red / 255.0, bestnode.green / 255.0, bestnode.blue / 255.0);

    return output;
}

vec3 processcolorwrapper(vec3 inputcolor, int gammamodein, double gammapowin, int gammamodeout, double gammapowout, int mapmode, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, int cccfunctiontype, double cccfloor, double cccceiling, double cccexp, double remapfactor, double remaplimit, bool softkneemode, double kneefactor, int mapdirection, int safezonetype, bool spiralcarisma, int lutmode, bool nesmode, double hdrsdrmaxnits, bool backwardsmode){
    vec3 output;
    if (backwardsmode){
        output = inverseprocesscolor(inputcolor, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, nesmode, hdrsdrmaxnits);
    }
    else {
        output = processcolor(inputcolor, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, nesmode, hdrsdrmaxnits);
    }
    return output;
}

// This is the guts of the main per-pixel processing loop for images and LUTs.
// It's been made into a function so that it can be run multithreaded.
// This function must be kept thread safe.
void loopGuts(int x, int y, int width, int height, bool lutgen, png_bytep buffer, int lutsize, double crtclamplow, double crtclamphigh, bool crtsuperblacks, double lpguscale, bool dither, int gammamodein, double gammapowin, int gammamodeout, double gammapowout, int mapmode, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, int cccfunctiontype, double cccfloor, double cccceiling, double cccexp, double remapfactor, double remaplimit, bool softkneemode, double kneefactor, int mapdirection, int safezonetype, bool spiralcarisma, int lutmode, bool nesmode, double hdrsdrmaxnits, bool backwardsmode){

    // read bytes from buffer (unless doing LUT)

    png_byte redin = 0;
    png_byte greenin = 0;
    png_byte bluein = 0;
    if (!lutgen){
        // we don't need a mutex here because no other instance will write at these array indices
        redin = buffer[ ((y * width) + x) * 4];
        greenin = buffer[ (((y * width) + x) * 4) + 1 ];
        bluein = buffer[ (((y * width) + x) * 4) + 2 ];
    }

    vec3 outcolor;

    // if we've already processed the same input color, just recall the memo
    bool havememo = false;
    if (!lutgen){
        memomtx.lock();
        if (!lutgen && memos[redin][greenin][bluein].known){
            outcolor = memos[redin][greenin][bluein].data;
            havememo = true;
        }
        memomtx.unlock();
    }
    if (!havememo){

        double redvalue;
        double greenvalue;
        double bluevalue;

        if (lutgen){
            redvalue = (double)(x % lutsize) / ((double)(lutsize - 1));
            greenvalue = (double)y / ((double)(lutsize - 1));
            bluevalue = (double)(x / lutsize) / ((double)(lutsize - 1));

            // expanded intermediate LUT uses range specified by crt clamping parameters
            if (lutmode == LUTMODE_POSTCC){
                double scaleby = crtclamphigh - crtclamplow;
                redvalue = (redvalue * scaleby) + crtclamplow;
                greenvalue = (greenvalue * scaleby) + crtclamplow;
                bluevalue = (bluevalue * scaleby) + crtclamplow;
            }
            // LUTMODE_POSTGAMMA_UNLIMITED ranges from zero light to maximum output value
            else if (lutmode == LUTMODE_POSTGAMMA_UNLIMITED){
                redvalue *= lpguscale;
                greenvalue *= lpguscale;
                bluevalue *= lpguscale;
                if (!crtsuperblacks){
                    // crush the superblacks we added earlier so that the LUT indices include the superblack range, but they map to outputs without the super blacks
                    redvalue = sourcegamut.attachedCRT->UnSuperBlack(redvalue);
                    greenvalue = sourcegamut.attachedCRT->UnSuperBlack(greenvalue);
                    bluevalue = sourcegamut.attachedCRT->UnSuperBlack(bluevalue);
                }
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

        outcolor = processcolorwrapper(inputcolor, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, false, hdrsdrmaxnits, backwardsmode);

        // blank the out-of-bounds stuff for sanity checking extended intermediate LUTSs
        /*
        if ((redvalue < 0.0) || (greenvalue < 0.0) || (bluevalue < 0.0) || (redvalue > 1.0) || (greenvalue > 1.0) || (bluevalue > 1.0)){
            outcolor = vec3(1.0, 1.0, 1.0);
        }
        */

        // memoize the result of the conversion so we don't need to do it again for this input color
        if (!lutgen){
            memomtx.lock();
            memos[redin][greenin][bluein].known = true;
            memos[redin][greenin][bluein].data = outcolor;
            memomtx.unlock();
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
    // we don't need a mutex here because no other instance will read or write at these array indices
    buffer[ ((y * width) + x) * 4] = redout;
    buffer[ (((y * width) + x) * 4) + 1 ] = greenout;
    buffer[ (((y * width) + x) * 4) + 2 ] = blueout;
    // we need to set opacity data id generating a LUT; if reading an image, leave it unchanged
    if (lutgen){
        buffer[ (((y * width) + x) * 4) + 3 ] = 255;
    }

} // end loopGuts()

// This function chunks half a row of a image/LUT into a single task.
// Its purpose is to improve multithreading efficiency by making fewer, bigger tasks for assigning to threads.
// I'm reluctant to make chunks bigger because:
//      The performance improvement from multithreading is most desperately needed for backwards mode.
//      For a big subset of possible parameters, backwards mode is going to bog down hard on the first few rows.
//      Therefore, I want to make sure chunks are small enough that the first few rows are broken up over a majority of cores.
// This function also does progress bar stuff.
void loopGutsHalfStride(bool leftside, int y, int width, int height, bool lutgen, png_bytep buffer, int lutsize, double crtclamplow, double crtclamphigh, bool crtsuperblacks, double lpguscale, bool dither, int gammamodein, double gammapowin, int gammamodeout, double gammapowout, int mapmode, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, int cccfunctiontype, double cccfloor, double cccceiling, double cccexp, double remapfactor, double remaplimit, bool softkneemode, double kneefactor, int mapdirection, int safezonetype, bool spiralcarisma, int lutmode, bool nesmode, double hdrsdrmaxnits, bool backwardsmode, int &progressbar, int maxprogressbar, int &lastannounce, int verbosity){

    // call the loop guts for each pixel in this half stride
    double halfstride = ((double)width)/2.0;
    int lefthalf = (int)ceil(halfstride);
    int righthalf = width - lefthalf;
    int max = leftside ? lefthalf : righthalf;
    int offset = leftside ? 0 : lefthalf;
    for (int i=0; i<max; i++){
        loopGuts(i+offset, y, width, height, lutgen, buffer, lutsize, crtclamplow, crtclamphigh, crtsuperblacks, lpguscale, dither, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, nesmode, hdrsdrmaxnits, backwardsmode);
    }

    if (verbosity >= VERBOSITY_MINIMAL){
        progressbarmtx.lock();
        // increment progress bar
        progressbar++;
        // do an announcement if we've crossed the next 5% threshhold
        int nextannounce = lastannounce + 5;
        while (nextannounce % 5 != 0){
            nextannounce--;
        }
        int currentpercent = (progressbar * 100) / maxprogressbar;
        bool doannounce=false;
        if (currentpercent >= nextannounce){
            lastannounce = currentpercent;
            doannounce = true;
        }
        progressbarmtx.unlock();

        if (doannounce){
            printfmtx.lock();
            printf("%i%%", currentpercent);
            if (currentpercent < 100){
                printf("... ");
            }
            if ((currentpercent >= 50) && (currentpercent < 55)){
                printf("\n");
            }
            fflush(stdout);
            printfmtx.unlock();
        }
    }

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

typedef struct float6param{
    std::string paramstring; // parameter's text
    std::string prettyname; // name for pretty printing
    double* vartobind0; // pointer to variable whose value to set
    double* vartobind1; // pointer to variable whose value to set
    double* vartobind2; // pointer to variable whose value to set
    double* vartobind3; // pointer to variable whose value to set
    double* vartobind4; // pointer to variable whose value to set
    double* vartobind5; // pointer to variable whose value to set
} float6param;

typedef struct float2param{
    std::string paramstring; // parameter's text
    std::string prettyname; // name for pretty printing
    double* vartobind0; // pointer to variable whose value to set
    double* vartobind1; // pointer to variable whose value to set
} float2param;

int main(int argc, const char **argv){
    
    // ----------------------------------------------------------------------------------------
    // parameter processing

    // defaults
    int threads = 0;
    bool helpmode = false;
    bool filemode = true;
    int gammamodein = GAMMA_SRGB;
    int gammamodeout = GAMMA_SRGB;
    double gammapowin = 2.4;
    double gammapowout = 2.2;
    double hdrsdrmaxnits = 200.0; // max nits a hdr monitor uses to display sdr white
    bool softkneemode = true;
    int softkneemodealias = 1;
    bool dither = true;
    int mapdirection = MAP_VPRC;
    int mapmode = MAP_COMPRESS;
    int sourcegamutindex = GAMUT_P22_TRINITRON;
    double sourcecustomgamut[3][3] = {
        // deafult to GAMUT_P22_TRINITRON
        {0.621, 0.34, 0.039}, //red
        {0.281, 0.606, 0.113}, //green
        {0.152, 0.067, 0.781} //blue
    };
    int destgamutindex = GAMUT_SRGB;
    double destcustomgamut[3][3] = {
        // deafult to GAMUT_SRGB
        {0.64, 0.33, 0.03}, //red
        {0.3, 0.6, 0.1}, //green
        {0.15, 0.06, 0.79} //blue
    };
    double sourcecustomwhitex = 0.2838;
    double sourcecustomwhitey = 0.2981;
    double sourcecustomwhitetemp = 9177.98;
    double sourcecustomwhitempcd = 0.0;
    int sourcecustomwhitempcdtype = MPCD_CIE;
    int sourcecustomwhitelocus = DAYLIGHTLOCUS_OLD;
    vec3 sourcecustomwhitefromtemp;
    int sourcewhitepointindex = WHITEPOINT_9300K27MPCD;
    int destwhitepointindex = WHITEPOINT_D65;
    double destcustomwhitex = 0.312713;
    double destcustomwhitey = 0.329016;
    double destcustomwhitetemp =  6500;
    double destcustomwhitempcd = 0.0;
    int destcustomwhitempcdtype = MPCD_CIE;
    int destcustomwhitelocus = DAYLIGHTLOCUS;
    vec3 destcustomwhitefromtemp;
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
    bool forcedisablechromaticadapt = false;
    int cccfunctiontype = CCC_EXPONENTIAL;
    double cccfloor = 0.5;
    double cccceiling = 0.95;
    double cccexp = 1.0;
    bool spiralcarisma = true;
    int scfunctiontype = SC_CUBIC_HERMITE;
    double scfloor = 0.7;
    double scceiling = 1.0;
    double scexp = 1.2;
    double scmax = 1.0;
    int crtemumode = CRT_EMU_NONE;
    double crtblacklevel = 0.0001;
    double crtwhitelevel = 1.71;
    int crtyuvconstantprecision = YUV_CONSTANT_PRECISION_MID;
    int crtmodindex = CRT_MODULATOR_NONE;
    int crtdemodindex = CRT_DEMODULATOR_NONE;
    int crtdemodrenorm = RENORM_DEMOD_INSANE;
    bool crtdemodfixes = true;
    bool crtdoclamphigh = true;
    bool crtclamplowatzerolight = true;
    double crtclamphigh = 1.1;
    double crtclamplow = -0.1;
    bool crtblackpedestalcrush = false;
    double crtblackpedestalcrushamount = 0.075;
    bool crtsuperblacks = false;
    bool lutgen = false;
    int lutmode = LUTMODE_NORMAL;
    int lutsize = 128;
    double lpguscale = 1.0; // needs to be a function-wide variable so we can print it later
    double lpguscalereciprocal = 1.0; // needs to be a function-wide variable so we can print it later
    bool nesmode = false;
    bool nesispal = false;
    bool nescbc = true;
    double nesskew26A = 4.5;
    double nesboost48C = 1.0;
    double nesskewstep = 2.5;
    bool neswritehtml = false;
    char* neshtmlfilename;
    bool backwardsmode = false;
    double crthueknob = 0.0;
    double crtsaturationknob = 1.0;
    double crtgammaknob = 1.0;
    bool retroarchwritetext = false;
    char* retroarchtextfilename;
    
    const boolparam params_bool[19] = {
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
            "--crtclamphighenable",                     //std::string paramstring; // parameter's text
            "CRT Clamp High RGB Output Values",           //std::string prettyname; // name for pretty printing
            &crtdoclamphigh                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtclamplowzerolight",                     //std::string paramstring; // parameter's text
            "CRT Clamp Low RGB Output Values at Zero Light",           //std::string prettyname; // name for pretty printing
            &crtclamplowatzerolight              //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtdemodfixes",                     //std::string paramstring; // parameter's text
            "Correct CRT demodulator angles/gains near \"straight\" demodulation to those exact values",           //std::string prettyname; // name for pretty printing
            &crtdemodfixes                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--backwards",                     //std::string paramstring; // parameter's text
            "Backwards Search Mode",           //std::string prettyname; // name for pretty printing
            &backwardsmode                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "-b",                     //std::string paramstring; // parameter's text
            "Backwards Search Mode",           //std::string prettyname; // name for pretty printing
            &backwardsmode                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtblackpedestalcrush",                     //std::string paramstring; // parameter's text
            "CRT Black Pedestal Crush",           //std::string prettyname; // name for pretty printing
            &crtblackpedestalcrush                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--cbpc",                     //std::string paramstring; // parameter's text
            "CRT Black Pedestal Crush",           //std::string prettyname; // name for pretty printing
            &crtblackpedestalcrush                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtsuperblacks",                     //std::string paramstring; // parameter's text
            "CRT show \"super black\" colors",           //std::string prettyname; // name for pretty printing
            &crtsuperblacks                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--csb",                     //std::string paramstring; // parameter's text
            "CRT show \"super black\" colors",           //std::string prettyname; // name for pretty printing
            &crtsuperblacks                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--no-source-adapt",                     //std::string paramstring; // parameter's text
            "Force disable chromatic adapation for source gamut",           //std::string prettyname; // name for pretty printing
            &forcedisablechromaticadapt                //bool* vartobind; // pointer to variable whose value to set
        },
        {
            "--nsa",                     //std::string paramstring; // parameter's text
            "Force disable chromatic adapation for source gamut",           //std::string prettyname; // name for pretty printing
            &forcedisablechromaticadapt                //bool* vartobind; // pointer to variable whose value to set
        }
    };

    const stringparam params_string[8] = {
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
        },
        {
            "--retroarchtextoutputfile",             //std::string paramstring; // parameter's text
            "Retroarch CCC Shader Parameter Text Output Filename",       //std::string prettyname; // name for pretty printing
            &retroarchtextfilename,         //char** vartobind;    // pointer to variable whose value to set
            &retroarchwritetext              //bool* flagtobind; // pointer to flag to set when this variable is set
        },

    };

    const paramvalue gamutlist[21] = {
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
            "rec2020_spec",
            GAMUT_REC2020
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
            "P22_trinitron_bohnsack",
            GAMUT_P22_TRINITRON_BOHNSACK
        },
        {
            "P22_trinitron_raney1",
            GAMUT_P22_TRINITRON_RANEY1
        },
        {
            "P22_trinitron_raney2",
            GAMUT_P22_TRINITRON_RANEY2
        },
        {
            "P22_trinitron_mixandmatch",
            GAMUT_P22_TRINITRON_MIXANDMATCH
        },
        {
            "P22_nec_multisync_c400",
            GAMUT_P22_NEC_MULTISYNC_C400
        },
        {
            "P22_dell",
            GAMUT_DELL
        },
        {
            "P22_japan_specific",
            GAMUT_JAPAN_SPEC
        },
        {
            "P22_kds_vs19",
            GAMUT_P22_KDS_VS19
        },
        {
            "P22_ebuish",
            GAMUT_P22_EBUISH
        },
        {
            "P22_hitachi",
            GAMUT_P22_HITACHI
        },
        {
            "P22_apple_multiscan1705",
            GAMUT_P22_APPLE_MULTISCAN1705
        },
        {
            "P22_rca_colortrak_patchy68k",
            GAMUT_P22_COLORTRAK
        },
        {
            "P22_toshiba_blackstripe_patchy68k",
            GAMUT_P22_BLACKSTRIPE
        },
        {
            "customcoord",
            GAMUT_CUSTOM
        }

        // intentionally omitting grade's P22_90s_ph because it is definitely wrong.
        // see comment in constants.h
    };

    const paramvalue whitepointlist[15] = {
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
            "9300K8mpcd_cie",
            WHITEPOINT_9300K8MPCDCIE
        },
        {
            "9300K8mpcd_judd",
            WHITEPOINT_9300K8MPCDJUDD
        },
        {
            "illuminantC",
            WHITEPOINT_ILLUMINANTC
        },
        /*
        {
            "6900K",
            WHITEPOINT_6900K
        },
        {
            "7000K",
            WHITEPOINT_7000K
        },
        {
            "7100K",
            WHITEPOINT_7100K
        },
        {
            "7250K",
            WHITEPOINT_7250K
        },
        {
            "D75",
            WHITEPOINT_D75
        },
        {
            "8500K",
            WHITEPOINT_8500K
        },
        {
            "8800K",
            WHITEPOINT_8800K
        },
        */
        {
            "trinitron_93k_bohnsack",
            WHITEPOINT_BOHNSACK
        },
        {
            "trinitron_d65_soniera",
            WHITEPOINT_D65_DISPLAYMATE
        },
        {
            "diamondpro_d65_fairchild",
            WHITEPOINT_D65_FAIRCHILD
        },
        {
            "diamondpro_93k_fairchild",
            WHITEPOINT_93K_FAIRCHILD
        },
        {
            "nec_multisync_c400_93k",
            WHITEPOINT_NEC_MULTISYNC_C400
        },
        {
            "kds_vs19_93k",
            WHITEPOINT_KDS_VS19
        },
        {
            "rca_colortrak_D75_patchy68k",
            WHITEPOINT_D75_COLORTRAK
        },
        {
            "customcoord",
            WHITEPOINT_CUSTOM_COORD
        },
        {
            "customtemp",
            WHITEPOINT_CUSTOM_TEMP
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

    const paramvalue gammatypelist[4] = {
        {
            "linear",
            GAMMA_LINEAR
        },
        {
            "srgb",
            GAMMA_SRGB
        },
        {
            "rec2084",
            GAMMA_REC2084
        },
        {
            "power",
            GAMMA_POWER
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

    const paramvalue crtdemodulatorlist[16] = {
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
            "CXA2060BS_PAL",
            CRT_DEMODULATOR_CXA2060BS_PAL
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
        },
        {
            "rca_colortrak",
            CRT_DEMODULATOR_COLORTRAK
        },
        {
            "TA7644BP",
            CRT_DEMODULATOR_TA7644BP
        },
        {
            "TA7644BP_measured",
            CRT_DEMODULATOR_TA7644BP_MEASURED
        },
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

    const paramvalue yuvprecisionlist[3] = {
        {
            "2digit",
            YUV_CONSTANT_PRECISION_CRAP
        },
        {
            "3digit",
            YUV_CONSTANT_PRECISION_MID
        },
        {
            "exact",
            YUV_CONSTANT_PRECISION_FULL
        }
    };

    const paramvalue locustypelist[7] = {
        {
            "plankian",
            PLANKIANLOCUS
        },
        {
            "plankian-old",
            PLANKIANLOCUS_OLD
        },
        {
            "plankian-veryold",
            PLANKIANLOCUS_VERYOLD
        },
        {
            "daylight",
            DAYLIGHTLOCUS
        },
        {
            "daylight-old",
            DAYLIGHTLOCUS_OLD
        },
        {
            "daylight-dogway",
            DAYLIGHTLOCUS_DOGWAY
        },
        {
            "daylight-dogway-old",
            DAYLIGHTLOCUS_DOGWAY_OLD
        }
    };

    const paramvalue mpcdtypelist[3] = {
        {
            "cie",
            MPCD_CIE
        },
        {
            "judd-macadam",
            MPCD_JUDD_MACADAM
        },
        {
            "judd",
            MPCD_JUDD
        }
    };

    const paramvalue luttypelist[4] = {
        {
            "normal",
            LUTMODE_NORMAL
        },
        {
            "postcc",
            LUTMODE_POSTCC
        },
        {
            "postgamma",
            LUTMODE_POSTGAMMA
        },
        {
            "postgammaunlimited",
            LUTMODE_POSTGAMMA_UNLIMITED
        }
    };

    const selectparam params_select[39] = {
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
            &destwhitepointindex,          //int* vartobind; // pointer to variable whose value to set
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
            &gammamodein,          //int* vartobind; // pointer to variable whose value to set
            gammatypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gammatypelist)/sizeof(gammatypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--gin",            //std::string paramstring; // parameter's text
            "Input/Output Gamma Function",             //std::string prettyname; // name for pretty printing
            &gammamodein,          //int* vartobind; // pointer to variable whose value to set
            gammatypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gammatypelist)/sizeof(gammatypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--gamma-out",            //std::string paramstring; // parameter's text
            "Input/Output Gamma Function",             //std::string prettyname; // name for pretty printing
            &gammamodeout,          //int* vartobind; // pointer to variable whose value to set
            gammatypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(gammatypelist)/sizeof(gammatypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--gout",            //std::string paramstring; // parameter's text
            "Input/Output Gamma Function",             //std::string prettyname; // name for pretty printing
            &gammamodeout,          //int* vartobind; // pointer to variable whose value to set
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
        },
        {
            "--crtyuvconst",            //std::string paramstring; // parameter's text
            "CRT YUV White Balance Constant Precison",             //std::string prettyname; // name for pretty printing
            &crtyuvconstantprecision,          //int* vartobind; // pointer to variable whose value to set
            yuvprecisionlist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(yuvprecisionlist)/sizeof(yuvprecisionlist[0])  //int tablesize; // number of items in the table
        },
        {
            "--source-whitepoint-custom-temp-locus",            //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Temperature Locus",             //std::string prettyname; // name for pretty printing
            &sourcecustomwhitelocus,          //int* vartobind; // pointer to variable whose value to set
            locustypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(locustypelist)/sizeof(locustypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--swctl",            //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Temperature Locus",             //std::string prettyname; // name for pretty printing
            &sourcecustomwhitelocus,          //int* vartobind; // pointer to variable whose value to set
            locustypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(locustypelist)/sizeof(locustypelist[0])  //int tablesize; // number of items in the table
        },
                {
            "--dest-whitepoint-custom-temp-locus",            //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Temperature Locus",             //std::string prettyname; // name for pretty printing
            &destcustomwhitelocus,          //int* vartobind; // pointer to variable whose value to set
            locustypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(locustypelist)/sizeof(locustypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--dwctl",            //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Temperature Locus",             //std::string prettyname; // name for pretty printing
            &destcustomwhitelocus,          //int* vartobind; // pointer to variable whose value to set
            locustypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(locustypelist)/sizeof(locustypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--lutmode",            //std::string paramstring; // parameter's text
            "LUT mode",             //std::string prettyname; // name for pretty printing
            &lutmode,          //int* vartobind; // pointer to variable whose value to set
            luttypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(luttypelist)/sizeof(luttypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--lm",            //std::string paramstring; // parameter's text
            "LUT mode",             //std::string prettyname; // name for pretty printing
            &lutmode,          //int* vartobind; // pointer to variable whose value to set
            luttypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(luttypelist)/sizeof(luttypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--source-whitepoint-custom-mpcd-type",            //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Temperature MPCD Type",             //std::string prettyname; // name for pretty printing
            &sourcecustomwhitempcdtype,          //int* vartobind; // pointer to variable whose value to set
            mpcdtypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(mpcdtypelist)/sizeof(mpcdtypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--swcmpcdt",            //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Temperature MPCD Type",             //std::string prettyname; // name for pretty printing
            &sourcecustomwhitempcdtype,          //int* vartobind; // pointer to variable whose value to set
            mpcdtypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(mpcdtypelist)/sizeof(mpcdtypelist[0])  //int tablesize; // number of items in the table
        },
       {
            "--dest-whitepoint-custom-mpcd-type",            //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Temperature MPCD Type",             //std::string prettyname; // name for pretty printing
            &destcustomwhitempcdtype,          //int* vartobind; // pointer to variable whose value to set
            mpcdtypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(mpcdtypelist)/sizeof(mpcdtypelist[0])  //int tablesize; // number of items in the table
        },
        {
            "--dwcmpcdt",            //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Temperature MPCD Type",             //std::string prettyname; // name for pretty printing
            &destcustomwhitempcdtype,          //int* vartobind; // pointer to variable whose value to set
            mpcdtypelist,                  // const paramvalue* valuetable; // pointer to table of possible values
            sizeof(mpcdtypelist)/sizeof(mpcdtypelist[0])  //int tablesize; // number of items in the table
        },
    };


    const floatparam params_float[44] = {
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
        },
        {
            "--source-whitepoint-custom-temp",         //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Temperature",        //std::string prettyname; // name for pretty printing
            &sourcecustomwhitetemp           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--swct",         //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Temperature",        //std::string prettyname; // name for pretty printing
            &sourcecustomwhitetemp           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--source-whitepoint-custom-mpcd",         //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Temperature MPCD Offset",        //std::string prettyname; // name for pretty printing
            &sourcecustomwhitempcd           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--swcmpcd",         //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Temperature MPCD Offset",        //std::string prettyname; // name for pretty printing
            &sourcecustomwhitempcd           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--dest-whitepoint-custom-temp",         //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Temperature",        //std::string prettyname; // name for pretty printing
            &destcustomwhitetemp           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--dwct",         //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Temperature",        //std::string prettyname; // name for pretty printing
            &destcustomwhitetemp           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--dest-whitepoint-custom-mpcd",         //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Temperature MPCD Offset",        //std::string prettyname; // name for pretty printing
            &destcustomwhitempcd           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--dwcmpcd",         //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Temperature MPCD Offset",        //std::string prettyname; // name for pretty printing
            &destcustomwhitempcd           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--hdr-sdr-max-nits",         //std::string paramstring; // parameter's text
            "Max Nits for SDR White on HDR Monitor",        //std::string prettyname; // name for pretty printing
            &hdrsdrmaxnits           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--hsmn",         //std::string paramstring; // parameter's text
            "Max Nits for SDR White on HDR Monitor",        //std::string prettyname; // name for pretty printing
            &hdrsdrmaxnits           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--crt-hue-knob",         //std::string paramstring; // parameter's text
            "CRT Hue Knob",        //std::string prettyname; // name for pretty printing
            &crthueknob           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--chk",         //std::string paramstring; // parameter's text
            "CRT Hue Knob",        //std::string prettyname; // name for pretty printing
            &crthueknob           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--crt-saturation-knob",         //std::string paramstring; // parameter's text
            "CRT Saturation Knob",        //std::string prettyname; // name for pretty printing
            &crtsaturationknob           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--csk",         //std::string paramstring; // parameter's text
            "CRT Saturation Knob",        //std::string prettyname; // name for pretty printing
            &crtsaturationknob           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--crt-gamma-knob",         //std::string paramstring; // parameter's text
            "CRT Gamma Knob",        //std::string prettyname; // name for pretty printing
            &crtgammaknob           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--cgk",         //std::string paramstring; // parameter's text
            "CRT Gamma Knob",        //std::string prettyname; // name for pretty printing
            &crtgammaknob           //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--crtblackpedestalcrush-amount",         //std::string paramstring; // parameter's text
            "CRT Black Pedestal Crush Amount",        //std::string prettyname; // name for pretty printing
            &crtblackpedestalcrushamount         //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--cbpca",         //std::string paramstring; // parameter's text
            "CRT Black Pedestal Crush Amount",        //std::string prettyname; // name for pretty printing
            &crtblackpedestalcrushamount         //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--gamma-in-power",         //std::string paramstring; // parameter's text
            "Gamma in power",        //std::string prettyname; // name for pretty printing
            &gammapowin         //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--ginp",         //std::string paramstring; // parameter's text
            "Gamma in power",        //std::string prettyname; // name for pretty printing
            &gammapowin         //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--gamma-out-power",         //std::string paramstring; // parameter's text
            "Gamma out power",        //std::string prettyname; // name for pretty printing
            &gammapowout         //double* vartobind; // pointer to variable whose value to set
        },
        {
            "--goutp",         //std::string paramstring; // parameter's text
            "Gamma out power",        //std::string prettyname; // name for pretty printing
            &gammapowout        //double* vartobind; // pointer to variable whose value to set
        }

        // Intentionally omitting cccfloor, cccceiling, cccexp for color correction methods derived from patent filings
        // because they suck and there's no evidence they were ever used for CRTs (or anything else).
        // Leaving them on the backend in case they ever prove useful in the future.
    };

    const intparam params_int[4] = {
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
        {
            "--threads",         //std::string paramstring; // parameter's text
            "Thread count",        //std::string prettyname; // name for pretty printing
            &threads            //int* vartobind; // pointer to variable whose value to set
        },
    };

    const float6param params_float6[4] = {
        {
            "--source-primaries-custom-coords",         //std::string paramstring; // parameter's text
            "Source Primaries Custom Coordinants",        //std::string prettyname; // name for pretty printing
            &sourcecustomgamut[0][0],          //double* vartobind0; // pointer to variable whose value to set
            &sourcecustomgamut[0][1],          //double* vartobind1; // pointer to variable whose value to set
            &sourcecustomgamut[1][0],          //double* vartobind2; // pointer to variable whose value to set
            &sourcecustomgamut[1][1],          //double* vartobind3; // pointer to variable whose value to set
            &sourcecustomgamut[2][0],          //double* vartobind4; // pointer to variable whose value to set
            &sourcecustomgamut[2][1]          //double* vartobind5; // pointer to variable whose value to set
        },
        {
            "--spcc",         //std::string paramstring; // parameter's text
            "Source Primaries Custom Coordinants",        //std::string prettyname; // name for pretty printing
            &sourcecustomgamut[0][0],          //double* vartobind0; // pointer to variable whose value to set
            &sourcecustomgamut[0][1],          //double* vartobind1; // pointer to variable whose value to set
            &sourcecustomgamut[1][0],          //double* vartobind2; // pointer to variable whose value to set
            &sourcecustomgamut[1][1],          //double* vartobind3; // pointer to variable whose value to set
            &sourcecustomgamut[2][0],          //double* vartobind4; // pointer to variable whose value to set
            &sourcecustomgamut[2][1]          //double* vartobind5; // pointer to variable whose value to set
        },
        {
            "--dest-primaries-custom-coords",         //std::string paramstring; // parameter's text
            "Destination Primaries Custom Coordinants",        //std::string prettyname; // name for pretty printing
            &destcustomgamut[0][0],          //double* vartobind0; // pointer to variable whose value to set
            &destcustomgamut[0][1],          //double* vartobind1; // pointer to variable whose value to set
            &destcustomgamut[1][0],          //double* vartobind2; // pointer to variable whose value to set
            &destcustomgamut[1][1],          //double* vartobind3; // pointer to variable whose value to set
            &destcustomgamut[2][0],          //double* vartobind4; // pointer to variable whose value to set
            &destcustomgamut[2][1]          //double* vartobind5; // pointer to variable whose value to set
        },
        {
            "--dpcc",         //std::string paramstring; // parameter's text
            "Destination Primaries Custom Coordinants",        //std::string prettyname; // name for pretty printing
            &destcustomgamut[0][0],          //double* vartobind0; // pointer to variable whose value to set
            &destcustomgamut[0][1],          //double* vartobind1; // pointer to variable whose value to set
            &destcustomgamut[1][0],          //double* vartobind2; // pointer to variable whose value to set
            &destcustomgamut[1][1],          //double* vartobind3; // pointer to variable whose value to set
            &destcustomgamut[2][0],          //double* vartobind4; // pointer to variable whose value to set
            &destcustomgamut[2][1]          //double* vartobind5; // pointer to variable whose value to set
        }
    };

    const float2param params_float2[4] = {
        {
            "--source-whitepoint-custom-coords",         //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Coordinants",        //std::string prettyname; // name for pretty printing
            &sourcecustomwhitex,          //double* vartobind0; // pointer to variable whose value to set
            &sourcecustomwhitey        //double* vartobind1; // pointer to variable whose value to set
        },
        {
            "--swcc",         //std::string paramstring; // parameter's text
            "Source Whitepoint Custom Coordinants",        //std::string prettyname; // name for pretty printing
            &sourcecustomwhitex,          //double* vartobind0; // pointer to variable whose value to set
            &sourcecustomwhitey        //double* vartobind1; // pointer to variable whose value to set
        },
        {
            "--dest-whitepoint-custom-coords",         //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Coordinants",        //std::string prettyname; // name for pretty printing
            &destcustomwhitex,          //double* vartobind0; // pointer to variable whose value to set
            &destcustomwhitey        //double* vartobind1; // pointer to variable whose value to set
        },
        {
            "--dwcc",         //std::string paramstring; // parameter's text
            "Destination Whitepoint Custom Coordinants",        //std::string prettyname; // name for pretty printing
            &destcustomwhitex,          //double* vartobind0; // pointer to variable whose value to set
            &destcustomwhitey        //double* vartobind1; // pointer to variable whose value to set
        }
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
    double* nextfloatptr1 = nullptr;
    double* nextfloatptr2 = nullptr;
    double* nextfloatptr3 = nullptr;
    double* nextfloatptr4 = nullptr;
    double* nextfloatptr5 = nullptr;
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
                nextfloatptr1 = nullptr;
                nextfloatptr2 = nullptr;
                nextfloatptr3 = nullptr;
                nextfloatptr4 = nullptr;
                nextfloatptr5 = nullptr;
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

                listsize = sizeof(params_float6)/sizeof(params_float6[0]);
                for (int j=0; j<listsize; j++){
                    if (strcmp(argv[i], params_float6[j].paramstring.c_str()) == 0){
                        nextfloatptr = params_float6[j].vartobind0;
                        nextfloatptr1 = params_float6[j].vartobind1;
                        nextfloatptr2 = params_float6[j].vartobind2;
                        nextfloatptr3 = params_float6[j].vartobind3;
                        nextfloatptr4 = params_float6[j].vartobind4;
                        nextfloatptr5 = params_float6[j].vartobind5;
                        nextparamtype = 6;
                        lastj = j;
                        breakout = true;
                        break;
                    }
                }
                if (breakout){break;}

                listsize = sizeof(params_float2)/sizeof(params_float2[0]);
                for (int j=0; j<listsize; j++){
                    if (strcmp(argv[i], params_float2[j].paramstring.c_str()) == 0){
                        nextfloatptr = params_float2[j].vartobind0;
                        nextfloatptr1 = params_float2[j].vartobind1;
                        nextparamtype = 7;
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
                        printf("Invalid value for parameter %s (%s). Expecting integer numerical value.\n", params_int[lastj].paramstring.c_str(), params_int[lastj].prettyname.c_str());
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
                        printf("Invalid value for parameter %s (%s). Expecting floating-point numerical value.\n", params_float[lastj].paramstring.c_str(), params_float[lastj].prettyname.c_str());
                        return ERROR_BAD_PARAM_FLOAT;
                    }
                    nextparamtype = 0;
                    break;
                }
            case 6:
                {
                    int inputok = sscanf(argv[i], "%lf,%lf,%lf,%lf,%lf,%lf", nextfloatptr, nextfloatptr1, nextfloatptr2, nextfloatptr3, nextfloatptr4, nextfloatptr5);
                    if (inputok != 6){
                        printf("Invalid value for parameter %s (%s). Expecting 6 comma-separated floating-point numerical values. Got %i.\n", params_float6[lastj].paramstring.c_str(), params_float6[lastj].prettyname.c_str(), inputok);
                        return ERROR_BAD_PARAM_SSCANF;
                    }
                    nextparamtype = 0;
                    break;
                }
            case 7:
                {
                    int inputok = sscanf(argv[i], "%lf,%lf", nextfloatptr, nextfloatptr1);
                    if (inputok != 2){
                        printf("Invalid value for parameter %s (%s). Expecting 2 comma-separated floating-point numerical values. Got %i.\n", params_float2[lastj].paramstring.c_str(), params_float2[lastj].prettyname.c_str(), inputok);
                        return ERROR_BAD_PARAM_SSCANF;
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
        if ((crtemumode != CRT_EMU_FRONT) && (lutmode != LUTMODE_NORMAL)){
            lutmode = LUTMODE_NORMAL;
            printf("Forcing lutmode to normal because no CRT simulation.\n");
        }
        if (retroarchwritetext && (crtemumode != CRT_EMU_FRONT)){
            retroarchwritetext = false;
            printf("\nNot writing retroarch shader parameters text file because no CRT simulation.\n");
        }
    }
    else {
        lutmode = LUTMODE_NONE; // make sure we pass mode none if not lutgen
        if (retroarchwritetext){
            retroarchwritetext = false;
            printf("\nNot writing retroarch shader parameters text file because not generating a LUT.\n");
        }
    }

    if (lutmode == LUTMODE_POSTGAMMA){
        if (crtclamplow != 0.0){
            printf("Forcing crtclamplow to 0.0 because lutmode is postgamma.\n");
            crtclamplow = 0.0;
        }
        if (!crtdoclamphigh || (crtclamphigh != 1.0)){
            printf("Forcing crtclamphigh to 1.0 because lutmode is postgamma.\n");
            crtclamphigh = 1.0;
            crtdoclamphigh = true;
        }
    }
    else if (LUTMODE_POSTGAMMA_UNLIMITED){
        if (!crtclamplowatzerolight){
            crtclamplowatzerolight = true;
            printf("Forcing crtclamplowatzerolight to true because lutmode is postgamma unlimited.\n");
        }
    }
    else if ((lutmode == LUTMODE_POSTCC) && backwardsmode){
        if (crtclamplow != 0.0){
            printf("Forcing crtclamplow to 0.0 because lutmode is postcc and backwardsmode is enabled.\n");
            crtclamplow = 0.0;
        }
        if (!crtdoclamphigh || (crtclamphigh != 1.0)){
            printf("Forcing crtclamphigh to 1.0 because lutmode is postc and backwardsmode is enabled.\n");
            crtclamphigh = 1.0;
            crtdoclamphigh = true;
        }
    }

    if (nesmode && backwardsmode){
        printf("\nWARNING: You've specified NES palette generation in combination with backwards search. The output will likely be useless.\n");
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
            if (gammamodeout == GAMMA_SRGB){
                printf("\nNOTE: You are saving a LUT using sRGB gamma. The program using this LUT will have to linearize values before interpolating. Are you sure this is what you want?\n");
            }
            else if (gammamodeout == GAMMA_REC2084){
                printf("\nNOTE: You are saving a LUT using Rec2084 gamma. The program using this LUT will have to linearize values before interpolating. Additionally, a large amount of bandwidth will be wasted. This is almost definitely not what you want.\n");
            }
            else if (gammamodeout == GAMMA_POWER){
                printf("\nNOTE: You are saving a LUT using power gamma. The program using this LUT will have to linearize values before interpolating. Are you sure this is what you want?\n");
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

    if ((crthueknob != 0.0) && ((crtemumode == CRT_EMU_NONE) || (crtdemodindex == CRT_DEMODULATOR_NONE))){
        printf("Ignoring CRT hue knob because not simulating demodulation.\n");
        crthueknob = 0.0;
    }
    else {
        while (crthueknob > 360.0){
            crthueknob -= 360.0;
        }
        while (crthueknob < -360){
            crthueknob += 360.0;
        }
    }

    if ((crtsaturationknob != 1.0) && (crtemumode == CRT_EMU_NONE)){
        printf("Ignoring CRT saturation knob because not simulating CRT.\n");
    }
    else if (crtsaturationknob < 0.0){
        printf("CRT saturation cannot be less than zero. Forcing to zero.\n");
        crtsaturationknob = 0.0;
    }

    if ((crtgammaknob != 1.0) && (crtemumode == CRT_EMU_NONE)){
        printf("Ignoring CRT gamma knob because not simulating CRT.\n");
    }
    else if (crtgammaknob <= 0.0){
        printf("CRT gamma cannot be zero or less. Forcing to 0.1. \n");
        crtgammaknob = 0.1;
    }

    if (    (   (sourcecustomwhitempcd != 0.0) &&
                (
                    (sourcecustomwhitelocus == DAYLIGHTLOCUS) ||
                    (sourcecustomwhitelocus == DAYLIGHTLOCUS_OLD) ||
                    (sourcecustomwhitelocus == DAYLIGHTLOCUS_DOGWAY) ||
                    (sourcecustomwhitelocus == DAYLIGHTLOCUS_DOGWAY_OLD)
                )
            ) ||
            (   (destcustomwhitempcd != 0.0) &&
                (
                    (destcustomwhitelocus == DAYLIGHTLOCUS) ||
                    (destcustomwhitelocus == DAYLIGHTLOCUS_OLD) ||
                    (destcustomwhitelocus == DAYLIGHTLOCUS_DOGWAY) ||
                    (destcustomwhitelocus == DAYLIGHTLOCUS_DOGWAY_OLD)
                )
            )
    ){
        printf("WARNING: You've specified a MPCD value with a non-Plankian locus. This is computable, but makes little sense.\n");
    }

    if ((crtemumode != CRT_EMU_NONE) && crtblackpedestalcrush){
        if (crtblackpedestalcrushamount < 0.0){
            crtblackpedestalcrushamount = 0.0;
            printf("CRT black pedestal cannot be less than zero. Forcing to zero.\n");
        }
        if (crtblackpedestalcrushamount > 1.0){
            crtblackpedestalcrushamount = 1.0;
            printf("CRT black pedestal cannot be greater than one. Forcing to one. (Everything will be black.)\n");
        }
    }

    if ((crtemumode != CRT_EMU_NONE) && crtsuperblacks && !crtclamplowatzerolight){
        crtclamplowatzerolight = true;
        printf("Force enabling --crtclamplowzerolight because CRT super blacks are enabled.\n");
    }

    if ((crtemumode != CRT_EMU_FRONT) && (gammamodein == GAMMA_POWER) && (gammapowin < 1.0)){
        gammapowin = 1.0;
        printf("Gamma in power cannot be less than one. Forcing to one.\n");
    }
    if ((gammamodeout == GAMMA_POWER) && (gammapowout < 1.0)){
        gammapowout = 1.0;
        printf("Gamma out power cannot be less than one. Forcing to one.\n");
    }

    if (forcedisablechromaticadapt && (destwhitepointindex != WHITEPOINT_D65)){
        forcedisablechromaticadapt = false;
        printf("Chromatic adapation cannot be disabled when destination whitepoint is not D65.\n");
    }

    // ---------------------------------------------------------------------
    // process custom constants

    if (sourcewhitepointindex == WHITEPOINT_CUSTOM_TEMP){
        sourcecustomwhitefromtemp = xycoordfromfromCCT(sourcecustomwhitetemp, sourcecustomwhitelocus, sourcecustomwhitempcd, sourcecustomwhitempcdtype);
    }

    if (destwhitepointindex == WHITEPOINT_CUSTOM_TEMP){
        destcustomwhitefromtemp = xycoordfromfromCCT(destcustomwhitetemp, destcustomwhitelocus, destcustomwhitempcd, destcustomwhitempcdtype);
    }

    sourcecustomgamut[0][2] = 1.0 - sourcecustomgamut[0][0] - sourcecustomgamut[0][1];
    sourcecustomgamut[1][2] = 1.0 - sourcecustomgamut[1][0] - sourcecustomgamut[1][1];
    sourcecustomgamut[2][2] = 1.0 - sourcecustomgamut[2][0] - sourcecustomgamut[2][1];
    destcustomgamut[0][2] = 1.0 - destcustomgamut[0][0] - destcustomgamut[0][1];
    destcustomgamut[1][2] = 1.0 - destcustomgamut[1][0] - destcustomgamut[1][1];
    destcustomgamut[2][2] = 1.0 - destcustomgamut[2][0] - destcustomgamut[2][1];
    
    // ---------------------------------------------------------------------
    // Screen barf the params in verbose mode
    // TODO: refactor this

    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n\n----------\nParameters are:\n");
        if (filemode){
            if (lutgen){
                printf("LUT generation.\nLUT size: %i\nOutput file: %s\n", lutsize, outputfilename);
                printf("LUT type: ");
                if (lutmode == LUTMODE_NORMAL){
                    printf("LUT type: Normal LUT\n");
                }
                else if (lutmode == LUTMODE_POSTCC){
                    printf("LUT type: Post-Color-Correction LUT\n");
                }
                else if (lutmode == LUTMODE_POSTGAMMA){
                    printf("LUT type: Post-Gamma LUT\n");
                }
                else {
                    printf("Error!\n");
                }
                if (retroarchwritetext){
                    printf("Retroarch CCC shader parameter text output file: %s\n", retroarchtextfilename);
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

        printf("Backwards search mode: ");
        if (backwardsmode){
            printf("ENABLED. (Treats user-supplied input as the desired output and searches for an input that yields that output, or nearest match.)\n");
        }
        else {
            printf("disabled\n");
        }

        switch (gammamodein){
            case GAMMA_LINEAR:
                printf("Input gamma function: linear\n");
                break;
            case GAMMA_SRGB:
                printf("Input gamma function: srgb\n");
                break;
            case GAMMA_REC2084:
                printf("Input gamma function: Rec2084\n");
                printf("Max nits for displaying SDR white on HDR monitor: %f\n", hdrsdrmaxnits);
                break;
            case GAMMA_POWER:
                printf("Input gamma function: power %f\n", gammapowin);
                break;
            default:
                break;
        };
        switch (gammamodeout){
            case GAMMA_LINEAR:
                printf("Output gamma function: linear\n");
                break;
            case GAMMA_SRGB:
                printf("Output gamma function: srgb\n");
                break;
            case GAMMA_REC2084:
                printf("Output gamma function: Rec2084\n");
                printf("Max nits for displaying SDR white on HDR monitor: %f\n", hdrsdrmaxnits);
                break;
            case GAMMA_POWER:
                printf("Output gamma function: power %f\n", gammapowout);
                break;
            default:
                break;
        };
        if (sourcegamutindex == GAMUT_CUSTOM){
            printf("Source primaries custom coordinates: red %f, %f; green %f, %f; blue %f, %f\n", sourcecustomgamut[0][0], sourcecustomgamut[0][1], sourcecustomgamut[1][0], sourcecustomgamut[1][1], sourcecustomgamut[2][0], sourcecustomgamut[2][1]);
        }
        else {
            printf("Source primaries: %s\n", gamutnames[sourcegamutindex].c_str());
        }

        if (sourcewhitepointindex == WHITEPOINT_CUSTOM_TEMP){
            printf("Source whitepoint: custom temperature %fK ", sourcecustomwhitetemp);
            if (sourcecustomwhitempcd != 0.0){
                printf("+ %f MPCD ", sourcecustomwhitempcd);
            }
            printf("(x=%f, y=%f)", sourcecustomwhitefromtemp.x, sourcecustomwhitefromtemp.y);
            switch (sourcecustomwhitelocus){
                case DAYLIGHTLOCUS:
                    printf(" (daylight locus (post-1968))");
                    break;
                case DAYLIGHTLOCUS_OLD:
                    printf(" (daylight locus (pre-1968))");
                    break;
                case DAYLIGHTLOCUS_DOGWAY:
                    printf(" (daylight locus (post-1968) (Dogway's approximation function))");
                    break;
                case DAYLIGHTLOCUS_DOGWAY_OLD:
                    printf(" (daylight locus (pre-1968) (Dogway's approximation function))");
                    break;
                case PLANKIANLOCUS:
                    printf(" (Plankian locus (modern))");
                    break;
                case PLANKIANLOCUS_OLD:
                    printf(" (Plankian locus (pre-1968))");
                    break;
                case PLANKIANLOCUS_VERYOLD:
                    printf(" (Plankian locus (1931))");
                    break;
                default:
                    break;
            };
            if (sourcecustomwhitempcd != 0.0){
                switch (sourcecustomwhitempcdtype){
                    case MPCD_CIE:
                        printf(" (CIE1960 MPCD units)");
                        break;
                    case MPCD_JUDD_MACADAM:
                        printf(" (Judd1935 MPCD units (MacAdam transformation))");
                        break;
                    case MPCD_JUDD:
                        printf(" (Judd1935 MPCD units (appendix transformation))");
                        break;
                    default:
                        break;
                }
            }
            printf("\n");
        }
        else if (sourcewhitepointindex == WHITEPOINT_CUSTOM_COORD){
            printf("Source whitepoint: custom coordinates x=%f, y=%f\n", sourcecustomwhitex, sourcecustomwhitey);
        }
        else{
            printf("Source whitepoint: %s\n", whitepointnames[sourcewhitepointindex].c_str());
        }

        if (destgamutindex == GAMUT_CUSTOM){
            printf("Destination primaries custom coordinates: red %f, %f; green %f, %f; blue %f, %f\n", destcustomgamut[0][0], destcustomgamut[0][1], destcustomgamut[1][0], destcustomgamut[1][1], destcustomgamut[2][0], destcustomgamut[2][1]);
        }
        else {
            printf("Destination primaries: %s\n", gamutnames[destgamutindex].c_str());
        }

        if (destwhitepointindex == WHITEPOINT_CUSTOM_TEMP){
            printf("Destination whitepoint: custom temperature %fK ", destcustomwhitetemp);
            if (destcustomwhitempcd != 0.0){
                printf("+ %f MPCD ", destcustomwhitempcd);
            }
            printf("(x=%f, y=%f)", destcustomwhitefromtemp.x, destcustomwhitefromtemp.y);
            switch (destcustomwhitelocus){
                case DAYLIGHTLOCUS:
                    printf(" (daylight locus (post-1968))");
                    break;
                case DAYLIGHTLOCUS_OLD:
                    printf(" (daylight locus (pre-1968))");
                    break;
                case DAYLIGHTLOCUS_DOGWAY:
                    printf(" (daylight locus (post-1968) (Dogway's approximation function))");
                    break;
                case DAYLIGHTLOCUS_DOGWAY_OLD:
                    printf(" (daylight locus (pre-1968) (Dogway's approximation function))");
                    break;
                case PLANKIANLOCUS:
                    printf(" (Plankian locus (modern))");
                    break;
                case PLANKIANLOCUS_OLD:
                    printf(" (Plankian locus (pre-1968))");
                    break;
                case PLANKIANLOCUS_VERYOLD:
                    printf(" (Plankian locus (1931))");
                    break;
                default:
                    break;
            };
            if (destcustomwhitempcd != 0.0){
                switch (destcustomwhitempcdtype){
                    case MPCD_CIE:
                        printf(" (CIE1960 MPCD units)");
                        break;
                    case MPCD_JUDD_MACADAM:
                        printf(" (Judd1935 MPCD units (MacAdam transformation))");
                        break;
                    case MPCD_JUDD:
                        printf(" (Judd1935 MPCD units (appendix transformation))");
                        break;
                    default:
                        break;
                }
            }
            printf("\n");
        }
        else if (destwhitepointindex == WHITEPOINT_CUSTOM_COORD){
            printf("Destination whitepoint: custom coordinates x=%f, y=%f\n", destcustomwhitex, destcustomwhitey);
        }
        else {
            printf("Destination whitepoint: %s\n", whitepointnames[destwhitepointindex].c_str());
        }

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
        if (forcedisablechromaticadapt){
            printf("Chromatic adapation force disabled for source gamut.\n");
        }
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
            if (crtblackpedestalcrush){
                printf("CRT black pedestal %f IRE crushed to 0 IRE.\n", 100.0 * crtblackpedestalcrushamount);
            }
            else {
                printf("CRT black pedestal crush disabled.\n");
            }
            if (crtsuperblacks){
                printf("CRT \"super black\" colors shown.\n");
            }
            else {
                printf("CRT \"super black\" colors clipped.\n");
            }
            printf("CRT black level %f x100 cd/m^2\n", crtblacklevel);
            printf("CRT white level %f x100 cd/m^2\n", crtwhitelevel);
            printf("CRT YUV white balance constant precision: ");
            if (crtyuvconstantprecision == YUV_CONSTANT_PRECISION_CRAP){
                printf("truncated to 2 digits (1953 standard).\n");
            }
            else if (crtyuvconstantprecision == YUV_CONSTANT_PRECISION_MID){
                printf("truncated to 3 digits (1994 standard).\n");
            }
            else {
                printf("full precision.\n");
            }
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
                 printf("CRT hue knob at %f degrees.\n", crthueknob);
            }
            printf("CRT saturation knob at %f times normal.\n", crtsaturationknob);
            printf("CRT R'G'B' low values clamped to %f", crtclamplow);
            if (crtclamplowatzerolight){
                printf(" (or zero light emission, whichever is higher)");
            }
            printf(".\n");
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
        emulatedcrt.Initialize(crtblacklevel, crtwhitelevel, crtyuvconstantprecision, crtmodindex, crtdemodindex, crtdemodrenorm, crtdoclamphigh, crtclamplowatzerolight, crtclamplow, crtclamphigh, verbosity, crtdemodfixes, crthueknob, crtsaturationknob, crtgammaknob, crtblackpedestalcrush, crtblackpedestalcrushamount, crtsuperblacks);
        if (crtemumode == CRT_EMU_FRONT){
            sourcegamutcrtsetting = CRT_EMU_FRONT;
        }
        else if (crtemumode == CRT_EMU_BACK){
            destgamutcrtsetting = CRT_EMU_BACK;
        }
    }
    
    vec3 sourcewhite;
    if (sourcewhitepointindex == WHITEPOINT_CUSTOM_TEMP){
        sourcewhite = sourcecustomwhitefromtemp;
    }
    else if (sourcewhitepointindex == WHITEPOINT_CUSTOM_COORD){
        sourcewhite = vec3(sourcecustomwhitex, sourcecustomwhitey, 1.0 - sourcecustomwhitex - sourcecustomwhitey);
    }
    else {
        sourcewhite = vec3(whitepoints[sourcewhitepointindex][0], whitepoints[sourcewhitepointindex][1], whitepoints[sourcewhitepointindex][2]);
    }

    vec3 sourcered, sourcegreen, sourceblue;
    if (sourcegamutindex == GAMUT_CUSTOM){
        sourcered = vec3(sourcecustomgamut[0][0], sourcecustomgamut[0][1], sourcecustomgamut[0][2]);
        sourcegreen = vec3(sourcecustomgamut[1][0], sourcecustomgamut[1][1], sourcecustomgamut[1][2]);
        sourceblue = vec3(sourcecustomgamut[2][0], sourcecustomgamut[2][1], sourcecustomgamut[2][2]);
    }
    else {
        sourcered = vec3(gamutpoints[sourcegamutindex][0][0], gamutpoints[sourcegamutindex][0][1], gamutpoints[sourcegamutindex][0][2]);
        sourcegreen = vec3(gamutpoints[sourcegamutindex][1][0], gamutpoints[sourcegamutindex][1][1], gamutpoints[sourcegamutindex][1][2]);
        sourceblue = vec3(gamutpoints[sourcegamutindex][2][0], gamutpoints[sourcegamutindex][2][1], gamutpoints[sourcegamutindex][2][2]);
    }
    
    vec3 destwhite;
    if (destwhitepointindex == WHITEPOINT_CUSTOM_TEMP){
        destwhite = destcustomwhitefromtemp;
    }
    else if (destwhitepointindex == WHITEPOINT_CUSTOM_COORD){
        destwhite = vec3(destcustomwhitex, destcustomwhitey, 1.0 - destcustomwhitex - destcustomwhitey);
    }
    else {
        destwhite = vec3(whitepoints[destwhitepointindex][0], whitepoints[destwhitepointindex][1], whitepoints[destwhitepointindex][2]);
    }

    vec3 destred, destgreen, destblue;
    if (destgamutindex == GAMUT_CUSTOM){
        destred = vec3(destcustomgamut[0][0], destcustomgamut[0][1], destcustomgamut[0][2]);
        destgreen = vec3(destcustomgamut[1][0], destcustomgamut[1][1], destcustomgamut[1][2]);
        destblue = vec3(destcustomgamut[2][0], destcustomgamut[2][1], destcustomgamut[2][2]);
    }
    else {
        destred = vec3(gamutpoints[destgamutindex][0][0], gamutpoints[destgamutindex][0][1], gamutpoints[destgamutindex][0][2]);
        destgreen = vec3(gamutpoints[destgamutindex][1][0], gamutpoints[destgamutindex][1][1], gamutpoints[destgamutindex][1][2]);
        destblue = vec3(gamutpoints[destgamutindex][2][0], gamutpoints[destgamutindex][2][1], gamutpoints[destgamutindex][2][2]);
    }
    
    bool compressenabled = (mapmode >= MAP_FIRST_COMPRESS);
    
    gamutdescriptor sourcegamut;
    bool srcOK = sourcegamut.initialize(sourcegamutindex != GAMUT_CUSTOM ? gamutnames[sourcegamutindex] : "Custom Source Gamut", sourcewhite, sourcered, sourcegreen, sourceblue, destwhite, true, verbosity, adapttype, forcedisablechromaticadapt, compressenabled, sourcegamutcrtsetting, &emulatedcrt);
    
    gamutdescriptor destgamut;
    bool destOK = destgamut.initialize(destgamutindex != GAMUT_CUSTOM ? gamutnames[destgamutindex] : "Custom Destination Gamut", destwhite, destred, destgreen, destblue, sourcewhite, false, verbosity, adapttype, false, compressenabled, destgamutcrtsetting, &emulatedcrt);
    
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

    // screen barf an overall matrix (useful for copy/pasting into other code)
    double matrixpreviewsource[3][3];
    double matrixpreviewdest[3][3];
    double matrixpreview[3][3];
    if (sourcegamut.needschromaticadapt){
        memcpy(matrixpreviewsource, sourcegamut.matrixNPMadaptToD65, 3 * 3 * sizeof(double));
    }
    else {
        memcpy(matrixpreviewsource, sourcegamut.matrixNPM, 3 * 3 * sizeof(double));
    }
    if (destgamut.needschromaticadapt){
        memcpy(matrixpreviewdest, destgamut.inverseMatrixNPMadaptToD65, 3 * 3 * sizeof(double));
    }
    else {
        memcpy(matrixpreviewdest, destgamut.inverseMatrixNPM, 3 * 3 * sizeof(double));
    }
    mult3x3Matrices(matrixpreviewdest, matrixpreviewsource, matrixpreview);
    printf("\nOverall linear RGB to linear RGB transformation matrix:\n");
    print3x3matrix(matrixpreview);
    printf("----------\n");

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
        
        vec3 outcolor = processcolorwrapper(inputcolor, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, false, false, hdrsdrmaxnits, backwardsmode);

        redout = toRGB8nodither(outcolor.x);
        greenout = toRGB8nodither(outcolor.y);
        blueout = toRGB8nodither(outcolor.z);
        if (verbosity >= VERBOSITY_MINIMAL){
            if (backwardsmode){
                printf("To reach the desired output color of %s (red %f, green %f, blue %f; red %i, green %i, blue %i), use an input color of 0x%02X%02X%02X (red %f, green %f, blue %f; red: %i, green: %i, blue: %i)\n", inputcolorstring, inputcolor.x, inputcolor.y, inputcolor.z,  toRGB8nodither(inputcolor.x), toRGB8nodither(inputcolor.y), toRGB8nodither(inputcolor.z), redout, greenout, blueout, outcolor.x, outcolor.y, outcolor.z, redout, greenout, blueout);
            }
            else {
                printf("Input color %s (red %f, green %f, blue %f; red %i, green %i, blue %i) converts to 0x%02X%02X%02X (red %f, green %f, blue %f; red: %i, green: %i, blue: %i)\n", inputcolorstring, inputcolor.x, inputcolor.y, inputcolor.z,  toRGB8nodither(inputcolor.x), toRGB8nodither(inputcolor.y), toRGB8nodither(inputcolor.z), redout, greenout, blueout, outcolor.x, outcolor.y, outcolor.z, redout, greenout, blueout);
            }
        }
        else {
            printf("%02X%02X%02X", redout, greenout, blueout);
        }
        return RETURN_SUCCESS;
    }
    // this mode generates a NES palette
    else if (nesmode){

        nesppusimulation nessim;
        nessim.Initialize(verbosity, nesispal, nescbc, nesskew26A, nesboost48C, nesskewstep, crtyuvconstantprecision);
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

            htmlfile << "\t\t\tCommand: gamutthingy";
            for (int i=1; i<argc; i++){
                htmlfile << " " << argv[i];
            }
            htmlfile << "<BR>\n";

            htmlfile << "\t\t\tParameters:<BR>\n";

            if (backwardsmode){
                htmlfile << "\t\t\tBackwards search mode enabled. (Output is likely useless.)<BR>\n";
            }

            htmlfile << "\t\t\tNES simulate PAL phase alternation: " << (nesispal ? "true" : "false") << "<BR>\n";
            htmlfile << "\t\t\tNES normalize chroma to colorburst: " << (nescbc ? "true" : "false") << "<BR>\n";
            htmlfile << "\t\t\tNES phase skew for hues 0x2, 0x6, and 0xA: " << nesskew26A << " degrees<BR>\n";
            htmlfile << "\t\t\tNES luma boost for hues 0x4, 0x8, and 0xC: " << nesboost48C << " IRE<BR>\n";
            htmlfile << "\t\t\tNES phase skew per luma step: " << nesskewstep << " degrees<BR>\n";

            htmlfile << "\t\t\tCRT YUV white balance constant precision: ";
            if (crtyuvconstantprecision == YUV_CONSTANT_PRECISION_CRAP){
                htmlfile << "truncated to 2 digits (1953 standard).<BR>\n";
            }
            else if (crtyuvconstantprecision == YUV_CONSTANT_PRECISION_MID){
                htmlfile << "truncated to 3 digits (1994 standard).<BR>\n";
            }
            else {
                htmlfile << "full precision.<BR>\n";
            }

            htmlfile << "\t\t\tCRT YIQ to R'G'B' demodulator chip: ";
            if (crtdemodindex == CRT_DEMODULATOR_NONE){
                htmlfile << "none<BR>\n";
            }
            else {
                 htmlfile << demodulatornames[crtdemodindex] << "<BR>\n";
                 htmlfile << "\t\t\tCRT hue knob at " << crthueknob << " degrees.<BR>\n";
            }
            htmlfile << "\t\t\tCRT saturation knob at " << crtsaturationknob << " times normal.<BR>\n";

            htmlfile << "\t\t\tCRT R'G'B' high low output values clamped to " << crtclamplow;
            if (crtclamplowatzerolight){
                htmlfile << " (or zero light emission, whichever is higher)";
            }
            htmlfile << ".<BR>\n";
            if (crtdoclamphigh){
                htmlfile << "\t\t\tCRT R'G'B' high output values clamped to " << crtclamphigh << ".<BR>\n";
            }
            else {
                htmlfile << "\t\t\tCRT R'G'B' high output values not clamped. (Out-of-bounds values resolved by gamut compression algorithm.)<BR>\n";
            }

            if (crtblackpedestalcrush){
                htmlfile << "\t\t\tCRT black pedestal " << (100.0 * crtblackpedestalcrushamount) << " IRE crushed to 0 IRE.<BR>\n";
            }
            else {
                htmlfile << "\t\t\tCRT black pedestal crush disabled.<BR>\n";
            }

            if (crtsuperblacks){
                htmlfile << "CRT \"super black\" colors shown.<BR>\n";
            }
            else {
                htmlfile << "CRT \"super black\" colors clipped.<BR>\n";
            }

            htmlfile << "\t\t\tCRT bt1886 Appendix1 EOTF function calibrated to:<BR>\n";
            htmlfile << "\t\t\tCRT black level: " << crtblacklevel << " x100 cd/m^2<BR>\n";
            htmlfile << "\t\t\tCRT white level: " << crtwhitelevel << " x100 cd/m^2<BR>\n";

            if (sourcegamutindex == GAMUT_CUSTOM){
                htmlfile << "\t\t\tSource primaries custom coordinates: red " << sourcecustomgamut[0][0] << ", " << sourcecustomgamut[0][1] << "; green " << sourcecustomgamut[1][0] << ", " << sourcecustomgamut[1][1] << "; blue " << sourcecustomgamut[2][0] << ", " << sourcecustomgamut[2][1] << "<BR>\n";
            }
            else {
                htmlfile << "\t\t\tSource primaries: " << gamutnames[sourcegamutindex] << "<BR>\n";
            }

            if (sourcewhitepointindex == WHITEPOINT_CUSTOM_TEMP){
                htmlfile << "\t\t\tSource whitepoint: custom temperature " << sourcecustomwhitetemp << "K ";
                if (sourcecustomwhitempcd != 0.0){
                    htmlfile << "+ " << sourcecustomwhitempcd << " MPCD ";
                }
                htmlfile << "(x=" << sourcecustomwhitefromtemp.x << ", y=" << sourcecustomwhitefromtemp.y << ")";
                switch (sourcecustomwhitelocus){
                    case DAYLIGHTLOCUS:
                        htmlfile << " (daylight locus (post-1968))";
                        break;
                    case DAYLIGHTLOCUS_OLD:
                        htmlfile << " (daylight locus (pre-1968))";
                        break;
                    case DAYLIGHTLOCUS_DOGWAY:
                        htmlfile << " (daylight locus (post-1968) (Dogway's approximation function))";
                        break;
                    case DAYLIGHTLOCUS_DOGWAY_OLD:
                        htmlfile << " (daylight locus (pre-1968) (Dogway's approximation function))";
                        break;
                    case PLANKIANLOCUS:
                        htmlfile << " (Plankian locus (modern))";
                        break;
                    case PLANKIANLOCUS_OLD:
                        htmlfile << " (Plankian locus (pre-1968))";
                        break;
                    case PLANKIANLOCUS_VERYOLD:
                        htmlfile << " (Plankian locus (1931))";
                        break;
                    default:
                        break;
                };
                if (sourcecustomwhitempcd != 0.0){
                    switch (sourcecustomwhitempcdtype){
                        case MPCD_CIE:
                            htmlfile << " (CIE1960 MPCD units)";
                            break;
                        case MPCD_JUDD_MACADAM:
                            htmlfile << " (Judd1935 MPCD units (MacAdam transformation))";
                            break;
                        case MPCD_JUDD:
                            htmlfile << " (Judd1935 MPCD units (appendix transformation))";
                            break;
                        default:
                            break;
                    }
                }
                htmlfile << "<BR>\n";
            }
            else if (sourcewhitepointindex == WHITEPOINT_CUSTOM_COORD){
                htmlfile << "\t\t\tSource whitepoint: custom coordinates x=" << sourcecustomwhitex << ", y=" << sourcecustomwhitey << "<BR>\n";
            }
            else {
                htmlfile << "\t\t\tSource whitepoint: " << whitepointnames[sourcewhitepointindex] << "<BR>\n";
            }

            if (destgamutindex == GAMUT_CUSTOM){
                htmlfile << "\t\t\tDestination primaries custom coordinates: red " << destcustomgamut[0][0] << ", " << destcustomgamut[0][1] << "; green " << destcustomgamut[1][0] << ", " << destcustomgamut[1][1] << "; blue " << destcustomgamut[2][0] << ", " << destcustomgamut[2][1] << "<BR>\n";
            }
            else {
                htmlfile << "\t\t\tDestination primaries: " << gamutnames[destgamutindex] << "<BR>\n";
            }

            if (destwhitepointindex == WHITEPOINT_CUSTOM_TEMP){
                htmlfile << "\t\t\tDestination whitepoint: custom temperature " << destcustomwhitetemp << "K ";
                if (destcustomwhitempcd != 0.0){
                    htmlfile << "+ " << destcustomwhitempcd << " MPCD ";
                }
                htmlfile << "(x=" << destcustomwhitefromtemp.x << ", y=" << destcustomwhitefromtemp.y << ")";
                switch (destcustomwhitelocus){
                    case DAYLIGHTLOCUS:
                        htmlfile << " (daylight locus (post-1968))";
                        break;
                    case DAYLIGHTLOCUS_OLD:
                        htmlfile << " (daylight locus (pre-1968))";
                        break;
                    case DAYLIGHTLOCUS_DOGWAY:
                        htmlfile << " (daylight locus (post-1968) (Dogway's approximation function))";
                        break;
                    case DAYLIGHTLOCUS_DOGWAY_OLD:
                        htmlfile << " (daylight locus (pre-1968) (Dogway's approximation function))";
                        break;
                    case PLANKIANLOCUS:
                        htmlfile << " (Plankian locus (modern))";
                        break;
                    case PLANKIANLOCUS_OLD:
                        htmlfile << " (Plankian locus (pre-1968))";
                        break;
                    case PLANKIANLOCUS_VERYOLD:
                        htmlfile << " (Plankian locus (1931))";
                        break;
                    default:
                        break;
                };
                if (destcustomwhitempcd != 0.0){
                    switch (destcustomwhitempcdtype){
                        case MPCD_CIE:
                            htmlfile << " (CIE1960 MPCD units)";
                            break;
                        case MPCD_JUDD_MACADAM:
                            htmlfile << " (Judd1935 MPCD units (MacAdam transformation))";
                            break;
                        case MPCD_JUDD:
                            htmlfile << " (Judd1935 MPCD units (appendix transformation))";
                            break;
                        default:
                            break;
                    }
                }
                htmlfile << "<BR>\n";
            }
            else if (destwhitepointindex == WHITEPOINT_CUSTOM_COORD){
                htmlfile << "\t\t\tDestination whitepoint: custom coordinates x=" << destcustomwhitex << ", y=" << destcustomwhitey << "<BR>\n";
            }
            else {
                htmlfile << "\t\t\tDestination whitepoint: " << whitepointnames[destwhitepointindex] << "<BR>\n";
            }

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

            if (forcedisablechromaticadapt){
               htmlfile << "\t\t\tChromatic adapation force disabled for source gamut.<BR>\n";
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

            switch (gammamodeout){
                case GAMMA_LINEAR:
                    htmlfile << "\t\t\tOutput gamma function: linear RGB<BR>\n";
                    break;
                case GAMMA_SRGB:
                    htmlfile << "\t\t\tOutput gamma function: sRGB<BR>\n";
                    break;
                case GAMMA_REC2084:
                    htmlfile << "\t\t\tOutput gamma function: Rec2084<BR>\n";
                    htmlfile << "\t\t\tMax Nits for Displaying SDR white on HDR monitor: " << hdrsdrmaxnits << "<BR>\n";
                    break;
                case GAMMA_POWER:
                    htmlfile << "\t\t\tOutput gamma function: power " << gammapowout << "<BR>\n";
                    break;
                default:
                    break;
            };

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
                    vec3 outcolor = processcolorwrapper(nesrgb, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, true, hdrsdrmaxnits, backwardsmode);
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
                
                // we need to know our ceiling for LUTMODE_POSTGAMMA_UNLIMITED
                lpguscale = 1.0;
                lpguscalereciprocal = 1.0;
                if (lutgen && (lutmode == LUTMODE_POSTGAMMA_UNLIMITED) && sourcegamut.attachedCRT){
                    // we need to temporarily set the CRT to super black mode
                    bool oldsuperblackmode = sourcegamut.attachedCRT->superblacks;
                    sourcegamut.attachedCRT->superblacks = true;
                    // if we're clamping, then we can compute the ceiling
                    if (crtdoclamphigh){
                        if (crtclamphigh > 1.0){
                            lpguscale = crtclamphigh;
                            if (crtgammaknob != 1.0){
                                lpguscale = pow(lpguscale, crtgammaknob);
                            }
                            lpguscale = sourcegamut.attachedCRT->tolinear1886appx1(lpguscale);
                        }
                    }
                    // otherwise we must guess and check to find the max output value.
                    else {
                        double lpgumax = 0.0;
                        // iterate over the primary and secondary colors
                        for (int guessr=0; guessr<=1; guessr++){
                            for (int guessg=0; guessg<=1; guessg++){
                                for (int guessb=0; guessb<=1; guessb++){
                                    if (guessr + guessg + guessb == 0){
                                        continue; // skip black
                                    }
                                    vec3 guess = vec3(double(guessr), double(guessg), double(guessb));
                                    vec3 guessresult = sourcegamut.attachedCRT->CRTEmulateGammaSpaceRGBtoLinearRGB(guess);
                                    if (guessresult.x > lpgumax){
                                        lpgumax = guessresult.x;
                                    }
                                    if (guessresult.y > lpgumax){
                                        lpgumax = guessresult.y;
                                    }
                                    if (guessresult.z > lpgumax){
                                        lpgumax = guessresult.z;
                                    }
                                }
                            }
                        }
                        lpguscale = lpgumax;
                    }
                    //printf("lpguscale is %f!\n", lpguscale);
                    // since there's no way to interpolate white from its neighbors,
                    // we need to make sure that white gets its own entry in the LUT
                    // so we'll increase the scale to hit a good factor to make that happen
                    // we want the reciprocal to be a concise rational number
                    if (lpguscale > 1.0){
                        bool happy = false;
                        for (int divisor = lutsize - 1; divisor > 0; divisor--){
                            // keep trying until we hit a bigger factor than what we have
                            double scalefactor = ((double)lutsize) / ((double)divisor);
                            if (scalefactor < lpguscale){
                                continue;
                            }
                            // compute greatest common factor
                            int gcf = std::gcd(lutsize, divisor);
                            // at least 2?
                            if (gcf < 2){
                                continue;
                            }
                            happy = true;
                            lpguscale = scalefactor;
                            lpguscalereciprocal = ((double)divisor) / ((double)lutsize);
                            //printf("lpguscale increased to %f!\n", lpguscale);
                            break;
                        }
                        if (!happy){
                            printf("WARNING: Could not find a good scaling factor to ensure white has its own entry in the LUT.\n");
                        }
                    }
                    // set the CRT superblack setting back to what it was
                    sourcegamut.attachedCRT->superblacks = oldsuperblackmode;
                }

                // start up a thread pool so we can process in parallel
                BS::thread_pool pool;
                // if the user supplied a thread count, honor it
                if (threads > 0){
                    pool.reset(threads);
                }
                if (verbosity >= VERBOSITY_MINIMAL){
                    printf("Using %li threads...\n", pool.get_thread_count());
                }

                int width = image.width;
                int height = image.height;

                // set up progess bar variables
                int progressbar = 0;
                int maxprogressbar = height * 2;
                int lastannounce = 0;
                if (verbosity >= VERBOSITY_MINIMAL){
                    printf("0%%... ");
                    fflush(stdout);
                }

                // iterate over every row
                for (int y=0; y<height; y++){

                    // add the left half of the row to the thread pool's task queue
                    pool.detach_task(
                        [y, width, height, lutgen, buffer, lutsize, crtclamplow, crtclamphigh, crtsuperblacks, lpguscale, dither, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, &sourcegamut, &destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, nesmode, hdrsdrmaxnits, backwardsmode, &progressbar, maxprogressbar, &lastannounce, verbosity]
                        {
                            loopGutsHalfStride(true, y, width, height, lutgen, buffer, lutsize, crtclamplow, crtclamphigh, crtsuperblacks, lpguscale, dither, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, nesmode, hdrsdrmaxnits, backwardsmode, progressbar, maxprogressbar, lastannounce, verbosity);
                        }
                    );

                    // add the right half of the row to the thread pool's task queue
                    pool.detach_task(
                        [y, width, height, lutgen, buffer, lutsize, crtclamplow, crtclamphigh, crtsuperblacks, lpguscale, dither, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, &sourcegamut, &destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, nesmode, hdrsdrmaxnits, backwardsmode, &progressbar, maxprogressbar, &lastannounce, verbosity]
                        {
                            loopGutsHalfStride(false, y, width, height, lutgen, buffer, lutsize, crtclamplow, crtclamphigh, crtsuperblacks, lpguscale, dither, gammamodein, gammapowin, gammamodeout, gammapowout, mapmode, sourcegamut, destgamut, cccfunctiontype, cccfloor, cccceiling, cccexp, remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype, spiralcarisma, lutmode, nesmode, hdrsdrmaxnits, backwardsmode, progressbar, maxprogressbar, lastannounce, verbosity);
                        }
                    );

                } // end for (int y=0; y<height; y++)
                // wait for all tasks sent to the thread pool to be completed.
                pool.wait();
                if ((verbosity >= VERBOSITY_MINIMAL) && (verbosity < VERBOSITY_HIGH)){
                    printf("\n");
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

    // if we're still here, maybe write the parameters file for retroarch CCC shaders
    if (lutgen && retroarchwritetext && (result == RETURN_SUCCESS) && sourcegamut.attachedCRT){
        printf("Writing text file for retroarch CC shaders parameters to %s...", retroarchtextfilename);
        std::ofstream ratxtfile;
        ratxtfile.open(retroarchtextfilename, std::ios::out);
        if (!ratxtfile.is_open()){
            printf("Unable to open %s for writing.\n", retroarchtextfilename);
            return ERROR_PNG_OPEN_FAIL;
        }
        ratxtfile << "# Paste this at the bottom of a template file of Chthon's Color Correction shaders.\n";
        switch (lutmode){
            case LUTMODE_NORMAL:
                ratxtfile << "# Use a LUTtype1 or LUTtype1fast template.\n";
                break;
            case LUTMODE_POSTCC:
                ratxtfile << "# Use a LUTtype2 template.\n";
                break;
            case LUTMODE_POSTGAMMA:
                ratxtfile << "# Use a LUTtype3 template.\n";
                break;
            case LUTMODE_POSTGAMMA_UNLIMITED:
                ratxtfile << "# Use a LUTtype4 template.\n";
                break;
            default:
                ratxtfile << "# Somthing is very wrong.\n";
                break;
        };
        ratxtfile << "# Paste " << outputfilename << " into the \"luts\" subdirectory of Chthon's Color Correction shaders.\n";
        ratxtfile << "# LUT generation command: gamutthingy";
        for (int i=1; i<argc; i++){
            ratxtfile << " " << argv[i];
        }
        ratxtfile << "\n\n\n";
        ratxtfile << "SamplerLUT = \"luts/" << outputfilename << "\"\n\n";

        ratxtfile << std::setprecision(16);

        ratxtfile << "crtBlackLevel = \"" << crtblacklevel << "\"\n";
        ratxtfile << "crtWhiteLevel = \"" << crtwhitelevel << "\"\n";
        ratxtfile << "crtConstantB = \"" << sourcegamut.attachedCRT->CRT_EOTF_b << "\"\n";
        ratxtfile << "crtConstantK = \"" << sourcegamut.attachedCRT->CRT_EOTF_k << "\"\n";
        ratxtfile << "crtConstantS = \"" << sourcegamut.attachedCRT->CRT_EOTF_s << "\"\n";
        ratxtfile << "crtGammaKnob = \"" << crtgammaknob << "\"\n\n";

        if (lutmode == LUTMODE_NORMAL){
            ratxtfile << "# For a LUTtype1fast template, you may omit everything below this point.\n\n";
        }

        ratxtfile << "crtLUT4scale = \"" << lpguscalereciprocal << "\"\n";
        ratxtfile << "crtLUT4renorm = \"" << (((lutmode == LUTMODE_POSTGAMMA_UNLIMITED) && !crtsuperblacks) ? "1.0" : "0.0") << "\"\n\n";

        ratxtfile << "crtMatrixRR = \"" << sourcegamut.attachedCRT->overallMatrix[0][0] << "\"\n";
        ratxtfile << "crtMatrixRG = \"" << sourcegamut.attachedCRT->overallMatrix[0][1] << "\"\n";
        ratxtfile << "crtMatrixRB = \"" << sourcegamut.attachedCRT->overallMatrix[0][2] << "\"\n";
        ratxtfile << "crtMatrixGR = \"" << sourcegamut.attachedCRT->overallMatrix[1][0] << "\"\n";
        ratxtfile << "crtMatrixGG = \"" << sourcegamut.attachedCRT->overallMatrix[1][1] << "\"\n";
        ratxtfile << "crtMatrixGB = \"" << sourcegamut.attachedCRT->overallMatrix[1][2] << "\"\n";
        ratxtfile << "crtMatrixBR = \"" << sourcegamut.attachedCRT->overallMatrix[2][0] << "\"\n";
        ratxtfile << "crtMatrixBG = \"" << sourcegamut.attachedCRT->overallMatrix[2][1] << "\"\n";
        ratxtfile << "crtMatrixBB = \"" << sourcegamut.attachedCRT->overallMatrix[2][2] << "\"\n\n";

        ratxtfile << "crtBlackCrush = \"" << (crtblackpedestalcrush ? "1.0" : "0.0") << "\"\n";
        ratxtfile << "crtBlackCrushAmount = \"" << crtblackpedestalcrushamount << "\"\n";
        if ((lutmode == LUTMODE_POSTGAMMA_UNLIMITED) && !crtsuperblacks){
            ratxtfile << "# crtSuperBlackEnable is forced to true for LUTtype4.\n";
        }
        ratxtfile << "crtSuperBlackEnable = \"" << ((crtsuperblacks || (lutmode == LUTMODE_POSTGAMMA_UNLIMITED)) ? "1.0" : "0.0") << "\"\n\n";

        if(sourcegamut.attachedCRT->zerolightclampenable){
            ratxtfile << "# crtLowClamp is set at zero light emission.\n";
        }
        ratxtfile << "crtLowClamp = \"" << sourcegamut.attachedCRT->rgbclamplowlevel << "\"\n";
        ratxtfile << "crtHighClampEnable = \"" << (crtdoclamphigh ? "1.0" : "0.0") << "\"\n";
        ratxtfile << "crtHighClamp = \"" << crtclamphigh << "\"\n";

        ratxtfile.flush();
        ratxtfile.close();
        printf(" done.\n");
    }
   

   return result;
}
