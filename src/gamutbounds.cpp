#include "gamutbounds.h"

#include "constants.h"
#include "plane.h"
#include "matrix.h"
#include "cielab.h"
#include "jzazbz.h"
#include "colormisc.h"

#include <math.h>
#include <cfloat>
#include <numbers>
#include <cstring> //for memcpy
#include <algorithm> //for reverse

// make this global so we only need to compute it once
const double HuePerStep = ((2.0 *  std::numbers::pi_v<long double>) / HUE_STEPS);
const double HalfHuePerStep = HuePerStep / 2.0;

bool gamutdescriptor::initialize(std::string name, vec3 wp, vec3 rp, vec3 gp, vec3 bp, vec3 other_wp, bool issource, int verbose, int cattype, bool compressenabled){
    verbosemode = verbose;
    gamutname = name;
    whitepoint = wp;
    redpoint = rp;
    greenpoint = gp;
    bluepoint = bp;
    issourcegamut = issource;
    CATtype = cattype;
    // working in JzCzhz colorspace requires everything be converted to D65 whitepoint.
    // don't convert to D65 if both white points are equal and we're not doing compression.
    // we're doing extra math (and maybe accruing some floating point errors) in the case where no compression and unequal whitepoints and destination is not D65 -- but I don't care enough about that case to deal with it.
    needschromaticadapt = ((compressenabled && !whitepoint.isequal(D65)) || (!whitepoint.isequal(other_wp)));
    if (verbose >= VERBOSITY_SLIGHT){
    printf("\n----------\nInitializing %s as ", gamutname.c_str());
        if (issourcegamut){
            printf("source gamut");
            if (needschromaticadapt){
                printf(" with chromatic adaptation");
            }
            printf("...\n");
        }
        else {
            printf("destination gamut");
            if (needschromaticadapt){
                printf(" with chromatic adaptation");
            }
            printf("...\n");
        }
    }
    
    initializeMatrixP();
    if (verbose >= VERBOSITY_HIGH){
        printf("\nMatrix P is:\n");
        print3x3matrix(matrixP);
    }
    
    if (!initializeInverseMatrixP()){
        printf("Initialization aborted!\n");
        return false;
    }
    if (verbose >= VERBOSITY_HIGH){
        printf("\nInverse Matrix P is:\n");
        print3x3matrix(inverseMatrixP);
    }
    
    initializeMatrixW();
    if (verbose >= VERBOSITY_HIGH){
        printf("\nMatrix W is: ");
        matrixW.printout();
    }
    
    initializeNormalizationFactors();
    if (verbose >= VERBOSITY_HIGH){
        printf("\nNormalization Factors are: ");
        normalizationFactors.printout();
    }
    
    initializeMatrixC();
    // not worth printing
    
    initializeMatrixNPM();
    if (verbose >= VERBOSITY_SLIGHT){
        printf("\nMatrix NPM (linear RGB to XYZ) is:\n");
        print3x3matrix(matrixNPM);
    }
    
    if (!initializeInverseMatrixNPM()){
        printf("Initialization aborted!\n");
        return false;
    }
    if (verbose >= VERBOSITY_SLIGHT){
        printf("\nInverse matrix NPM (XYZ to linear RGB) is:\n");
        print3x3matrix(inverseMatrixNPM);
    }
    
    if (needschromaticadapt){
        if (!initializeChromaticAdaptationToD65()){
            printf("Initialization aborted!\n");
            return false;
        }
        if (verbose >= VERBOSITY_SLIGHT){
            printf("\nMatrix M (XYZ to XYZ chromatic adaptation of whitepoint to D65) is:\n");
            print3x3matrix(matrixMtoD65);
            printf("\nMatrix NPM-adapt (linear RGB to XYZ-D65) is:\n");
            print3x3matrix(matrixNPMadaptToD65);
            printf("\nInverse Matrix NPM-adapt (XYZ-D65 to linear RGB) is:\n");
            print3x3matrix(inverseMatrixNPMadaptToD65);
        }
    }
    
    reservespace();
    if (verbose >= VERBOSITY_SLIGHT) printf("\nSampling gamut boundaries...");
    FindBoundaries();
    if (verbose >= VERBOSITY_SLIGHT) printf(" done.\n");
    
    if (verbose >= VERBOSITY_SLIGHT) printf("\nDone initializing gamut descriptor for %s.\n----------\n", gamutname.c_str());
    return true;
}

// resizes vectors ahead of time
void gamutdescriptor::reservespace(){
    for (int i=0; i<HUE_STEPS; i++){
        data[i].reserve(LUMA_STEPS + 6); // We need LUMA_STEPS + 2 for the fully convex case. The rest is padding in case of concavity.
    }
    return;
}

void gamutdescriptor::initializeMatrixP(){
    matrixP[0][0] = redpoint.x;
    matrixP[0][1] = greenpoint.x;
    matrixP[0][2] = bluepoint.x;
    matrixP[1][0] = redpoint.y;
    matrixP[1][1] = greenpoint.y;
    matrixP[1][2] = bluepoint.y;
    matrixP[2][0] = redpoint.z;
    matrixP[2][1] = greenpoint.z;
    matrixP[2][2] = bluepoint.z;
    return;
}

void gamutdescriptor::initializeMatrixC(){
    matrixC[0][0] = normalizationFactors.x;
    matrixC[0][1] = 0.0;
    matrixC[0][2] = 0.0;
    matrixC[1][0] = 0.0;
    matrixC[1][1] = normalizationFactors.y;
    matrixC[1][2] = 0.0;
    matrixC[2][0] = 0.0;
    matrixC[2][1] = 0.0;
    matrixC[2][2] = normalizationFactors.z;
    return;
}

bool gamutdescriptor::initializeInverseMatrixP(){
    bool output = Invert3x3Matrix(matrixP, inverseMatrixP);
    if (!output){
        printf("Disaster! Matrix P is not invertible! Bad stuff will happen!\n");       
    }
    return output;
}

bool gamutdescriptor::initializeInverseMatrixNPM(){
    bool output = Invert3x3Matrix(matrixNPM, inverseMatrixNPM);
    if (!output){
        printf("Disaster! Matrix NPM is not invertible! Bad stuff will happen!\n");       
    }
    return output;
}

void gamutdescriptor::initializeMatrixW(){
    matrixW.x = whitepoint.x / whitepoint.y;
    matrixW.y = 1.0;
    matrixW.z = whitepoint.z / whitepoint.y;
    return;
}

void gamutdescriptor::initializeNormalizationFactors(){
    normalizationFactors = multMatrixByColor(inverseMatrixP, matrixW);
    return;
}

void gamutdescriptor::initializeMatrixNPM(){
    mult3x3Matrices(matrixP, matrixC, matrixNPM);
    return;
}

bool gamutdescriptor::initializeChromaticAdaptationToD65(){
    double CATMatrix[3][3];
    if (CATtype == ADAPT_BRADFORD){
        memcpy(CATMatrix, BradfordMatrix, 9 * sizeof(double));
    }
    else if (CATtype == ADAPT_CAT16){
        memcpy(CATMatrix, CAT16Matrix, 9 * sizeof(double));
    }
    else {
        printf("Invalid chromatic adapation matrix selection index (%i).\n", CATtype);   
        return false;
    }
    double inverseCATMatrix[3][3];
    if (!Invert3x3Matrix(CATMatrix, inverseCATMatrix)){
        printf("What the flying fuck?! Chromatic adaptation matrix was not invertible.\n");   
        return false;
    }
    destMatrixW.x = D65.x / D65.y;
    destMatrixW.y = 1.0;
    destMatrixW.z = D65.z / D65.y;
    
    vec3 sourceRhoGammaBeta = multMatrixByColor(CATMatrix, matrixW);
    vec3 destRhoGammaBeta = multMatrixByColor(CATMatrix, destMatrixW);
    
    double coneResponseScaleMatrix[3][3] = {
        {destRhoGammaBeta.x / sourceRhoGammaBeta.x, 0.0, 0.0},
        {0.0, destRhoGammaBeta.y / sourceRhoGammaBeta.y, 0.0},
        {0.0, 0.0, destRhoGammaBeta.z / sourceRhoGammaBeta.z}
    };
    
    double tempMatrix[3][3];
    mult3x3Matrices(inverseCATMatrix, coneResponseScaleMatrix, tempMatrix);
    mult3x3Matrices(tempMatrix, CATMatrix, matrixMtoD65);
    
    mult3x3Matrices(matrixMtoD65, matrixNPM, matrixNPMadaptToD65);
    if (!Invert3x3Matrix(matrixNPMadaptToD65, inverseMatrixNPMadaptToD65)){
        printf("Disaster! Chromatic Adapation Matrix NPM is not invertible! Bad stuff will happen!\n");  
        return false;
    }
    
    return true;
}

// store the primaries and secondaries in JzCzhz for later reference
bool gamutdescriptor::initializePolarPrimaries(bool dosc, double scfloor, double scceil, double scexp, int scmode, int verbose){
        
    if (verbose >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing polar JzCzhz primary/seconday coordinates for %s...\n", gamutname.c_str());
    }
    
    polarredpoint = linearRGBtoJzCzhz(vec3(1.0, 0.0, 0.0));
    polargreenpoint = linearRGBtoJzCzhz(vec3(0.0, 1.0, 0.0));
    polarbluepoint = linearRGBtoJzCzhz(vec3(0.0, 0.0, 1.0));
        
    polarcyanpoint = linearRGBtoJzCzhz(vec3(0.0, 1.0, 1.0));
    polarmagentapoint = linearRGBtoJzCzhz(vec3(1.0, 0.0, 1.0));
    polaryellowpoint = linearRGBtoJzCzhz(vec3(1.0, 1.0, 0.0));
        
    // TODO: When implementing 2-step mode, use points that correspond to outputs of step 1 given primaries as input rather than the gamut primaries
    
    adjpolarredpoint = polarredpoint;
    adjpolargreenpoint = polargreenpoint;
    adjpolarbluepoint = polarbluepoint;

    adjpolarcyanpoint = polarcyanpoint;
    adjpolarmagentapoint = polarmagentapoint;
    adjpolaryellowpoint = polaryellowpoint;
    
    if (verbose >= VERBOSITY_SLIGHT){
        printf("Red: ");
        adjpolarredpoint.printout();
        printf("Green: ");
        adjpolargreenpoint.printout();
        printf("Blue: ");
        adjpolarbluepoint.printout();
        printf("Yellow: ");
        adjpolaryellowpoint.printout();
        printf("Magenta: ");
        adjpolarmagentapoint.printout();
        printf("Cyan: ");
        adjpolarcyanpoint.printout();
    }
    
    // intialize parameters for scaling
    if (dosc){
        spiralcarismafloor = scfloor;
        spiralcarismaceiling = scceil;
        spiralcharismaexponent = scexp;
        spiralcarismascalemode = scmode;
    }
    
    return true;
}

void gamutdescriptor::initializeMatrixChunghwa(gamutdescriptor &othergamut){
    
    double dummy; // this color correction circuit doesn't care what it does to luma
    
    // figure out the correction matrix
    vec3 redweights = xyYhillclimb(othergamut.redpoint.x, othergamut.redpoint.y, LOCKRED, dummy);
    vec3 greenweights = xyYhillclimb(othergamut.greenpoint.x, othergamut.greenpoint.y, LOCKGREEN, dummy);
    vec3 blueweights = xyYhillclimb(othergamut.bluepoint.x, othergamut.bluepoint.y, LOCKBLUE, dummy);
    
    matrixChunghwa[0][0] = redweights.x;
    matrixChunghwa[0][1] = greenweights.x;
    matrixChunghwa[0][2] = blueweights.x;
    
    matrixChunghwa[1][0] = redweights.y;
    matrixChunghwa[1][1] = greenweights.y;
    matrixChunghwa[1][2] = blueweights.y;
    
    matrixChunghwa[2][0] = redweights.z;
    matrixChunghwa[2][1] = greenweights.z;
    matrixChunghwa[2][2] = blueweights.z;
    
    return;
}

vec3 gamutdescriptor::linearRGBtoXYZ(vec3 input){
    if (needschromaticadapt){
        return multMatrixByColor(matrixNPMadaptToD65, input);
    }
    return multMatrixByColor(matrixNPM, input);
}

vec3 gamutdescriptor::XYZtoLinearRGB(vec3 input){
    if (needschromaticadapt){
        return multMatrixByColor(inverseMatrixNPMadaptToD65, input);
    }
    return multMatrixByColor(inverseMatrixNPM, input);
}

vec3 gamutdescriptor::linearRGBtoJzCzhz(vec3 input){
    if (verbosemode >= VERBOSITY_EXTREME){
        printf("Linear RGB input is: ");
        input.printout();
    }
    vec3 output = linearRGBtoXYZ(input);
    if (verbosemode >= VERBOSITY_EXTREME){
        printf("XYZ is: ");
        output.printout();
    }
    output = XYZtoJzazbz(output);
    if (verbosemode >= VERBOSITY_EXTREME){
        printf("Jzazbz is: ");
        output.printout();
    }
    output = Polarize(output);
    return output;
}

vec3 gamutdescriptor::JzCzhzToLinearRGB(vec3 input){
    vec3 output = Depolarize(input);
    output = JzazbzToXYZ(output);
    output = XYZtoLinearRGB(output);
    return output;
}

/*
vec3 gamutdescriptor::linearRGBtoLCh(vec3 input){
    if (verbosemode >= VERBOSITY_EXTREME){
        printf("Linear RGB input is: ");
        input.printout();
    }
    vec3 output = linearRGBtoXYZ(input);
    if (verbosemode >= VERBOSITY_EXTREME){
        printf("XYZ is: ");
        output.printout();
    }
    output = XYZtoLAB(output, D65);
    if (verbosemode >= VERBOSITY_EXTREME){
        printf("LAB is: ");
        output.printout();
    }
    output = Polarize(output);
    return output;
}
*/

// Populates the gamut boundary descriptor using the algorithm from
// Lihao, Xu, Chunzhi, Xu, & Luo, Ming Ronnier. "Accurate gamut boundary descriptor for displays." *Optics Express*, Vol. 30, No. 2, pp. 1615-1626. January 2022. (https://opg.optica.org/fulltext.cfm?rwjcode=oe&uri=oe-30-2-1615&id=466694)
void gamutdescriptor::FindBoundaries(){
    
    // first we need to know what scale we'll be working at so we can adjust the step size accordingly
    
    // find max luminosity by checking the white point
    vec3 tempcolor = linearRGBtoJzCzhz(vec3(1.0, 1.0, 1.0));
    double maxluma = tempcolor.x;
    
    // find max chroma by checking the red/green/blue points
    tempcolor = linearRGBtoJzCzhz(vec3(1.0, 0.0, 0.0));
    double maxchroma = tempcolor.y;
    tempcolor = linearRGBtoJzCzhz(vec3(0.0, 1.0, 0.0));
    if (tempcolor.y > maxchroma){
        maxchroma = tempcolor.y;
    }
    tempcolor = linearRGBtoJzCzhz(vec3(0.0, 0.0, 1.0));
    if (tempcolor.y > maxchroma){
        maxchroma = tempcolor.y;
    }
    maxchroma *= 1.1; // pad it to make sure we don't accidentally clip anything
    
    // process every hue slice
    for (int huestep = 0; huestep < HUE_STEPS; huestep++){
        ProcessSlice(huestep, maxluma, maxchroma);
        
        // intitialize the hue rotation stuff
        rotationneeded[huestep] = false; // make sure this is initialized for later
        impingingslicecount[huestep] = 0;
        impingingslices[huestep].clear();
        selfwarp[huestep].index = huestep;
        selfwarp[huestep].floor = 0.0;
        selfwarp[huestep].ceiling = std::numeric_limits<double>::max();
    }
    
    return;
}

// Precomputes which slices will rotate into which other slices over which chroma ranges under spiral carisma,
// effectively creating a new "warped" gamut boundary.
void gamutdescriptor::WarpBoundaries(){

    // process every hue slice
    for (int huestep = 0; huestep < HUE_STEPS; huestep++){
        const double hue = ((double)huestep) * ((2.0 *  std::numbers::pi_v<long double>) / HUE_STEPS);
        // find the max rotation
        double maxrotation = FindHueMaxRotation(hue);
        double fmaxrotation = fabs(maxrotation);
        bool negrotate = (maxrotation < 0.0);
        // how many slices are we going to impinge?
        int impingedslices = (int)(fmaxrotation / HuePerStep); // use float to int truncation to round down
        //printf("huestep %i has hue %f, max chroma %f, and maxrotation %f, which impinges %i slices:\n", huestep, hue, cuspchromalist[huestep], maxrotation, impingedslices);
        if (impingedslices == 0){
            rotationneeded[huestep] = false;
        }
        else {
            rotationneeded[huestep] = true;
            double floorchroma = 0.0;
            double ceilchroma = 0.0;
            for (int i=0 ; i<=impingedslices; i++){
                // new floor is old ceiling
                floorchroma = ceilchroma;
                // now find the new ceiling
                // if this is the slice that contains the max rotation, use a gigantic dummy value
                if (i == impingedslices){
                    ceilchroma = std::numeric_limits<double>::max();
                }
                // otherwise we must invert the mapping function
                else {
                        double newceilrotation = (HuePerStep * (double)(i + 1));
                        double newceilrotationpercent = newceilrotation / fmaxrotation;
                        double scalefactor;
                        if (spiralcarismascalemode == SC_EXPONENTIAL){
                            inversepowermap(spiralcarismafloor, spiralcarismaceiling,newceilrotationpercent, spiralcharismaexponent);
                        }
                        else if (spiralcarismascalemode == SC_CUBIC_HERMITE){
                            scalefactor = inversecubichermitemap(spiralcarismafloor, spiralcarismaceiling, newceilrotationpercent);
                        }
                        else {
                            // if we somehow have an invalid mapping mode passed in, just do linear
                            scalefactor = newceilrotationpercent;
                        }
                        // catch floating point errors
                        if (scalefactor < 0.0){
                            scalefactor = 0.0;
                        }
                        if (scalefactor > 1.0){
                            scalefactor = 1.0;
                        }
                        ceilchroma = scalefactor * cuspchromalist[huestep];
                }
                // now we need to save the floor and ceiling somewhere
                // offset 0 is this slice itself
                if (i == 0){
                    selfwarp[huestep].floor = floorchroma;
                    selfwarp[huestep].ceiling = ceilchroma;
                    //printf("\thue %i impinges on itself from chroma floor %10f to chroma ceiling %10f\n", huestep, floorchroma, ceilchroma);
                }
                // if the mapping is so steep that it skips over a slice, skip over it here too
                // we don't want to inflate the maximum possible post-rotation chroma for this slice
                // and I *think* whatever does rotate in here should keep the maxes on contiguous slices contiguous
                else if (ceilchroma != floorchroma){
                    int targetindex = negrotate ? huestep - i : huestep + i;
                    if (targetindex < 0){
                        targetindex += HUE_STEPS;
                    }
                    else if (targetindex >= HUE_STEPS){
                        targetindex -= HUE_STEPS;
                    }
                    warprange somewarpinfo;
                    somewarpinfo.index = huestep;
                    somewarpinfo.floor = floorchroma;
                    somewarpinfo.ceiling = ceilchroma;
                    impingingslices[targetindex].push_back(somewarpinfo);
                    impingingslicecount[targetindex]++;
                    /*
                    if (ceilchroma == std::numeric_limits<double>::max()){
                        printf("\thue %i impinges on hue %i from chroma floor %10f to chroma ceiling max\n", huestep, targetindex, floorchroma);
                    }
                    else{
                        printf("\thue %i impinges on hue %i from chroma floor %10f to chroma ceiling %10f\n", huestep, targetindex, floorchroma, ceilchroma);
                    }
                    */
                }
                
                // for now just print results
                /*
                if (ceilchroma == std::numeric_limits<double>::max()){
                    printf("\toffset %i has chroma floor %10f and chroma ceiling max\n", i, floorchroma);
                }
                else{
                    printf("\toffset %i has chroma floor %.10f and chroma ceiling %.10f (range %.10f, eq? %i)\n", i, floorchroma, ceilchroma, ceilchroma  - floorchroma, (ceilchroma == floorchroma) );
                }
                */
            }
        }
    }
    
    return;
}

// checks if the supplied JzCzhz color is within this gamut.
// if not, also sets errorsize to the sum of linear rgb over/underruns.
// (or sets errorsize to 10k if JzCzhzToLinearRGB() encounters a NaN error)
bool gamutdescriptor::IsJzCzhzInBounds(vec3 color, double &errorsize){
    
    bool isinbounds = true;
    errorsize = 0.0;
    
    vec3 rgbcolor = JzCzhzToLinearRGB(color);
    
    // inverse PQ function can generate NaN :( Let's assume all NaNs are waaay out of bounds
    if (isnan(rgbcolor.x) ||  isnan(rgbcolor.y) || isnan(rgbcolor.z)){
        isinbounds = false;
        errorsize = 10000.0; // arbitrary huge number
    }
    else {
        if (rgbcolor.x > 1.0){
            isinbounds = false;
            errorsize += rgbcolor.x - 1.0;
        }
        else if (rgbcolor.x < 0.0){
            isinbounds = false;
            errorsize += (-1.0 * rgbcolor.x);
        }
        if (rgbcolor.y > 1.0){
            isinbounds = false;
            errorsize += rgbcolor.y - 1.0;
        }
        else if (rgbcolor.y < 0.0){
            isinbounds = false;
            errorsize += (-1.0 * rgbcolor.y);
        }
        if (rgbcolor.z > 1.0){
            isinbounds = false;
            errorsize += rgbcolor.z - 1.0;
        }
        else if (rgbcolor.z < 0.0){
            isinbounds = false;
            errorsize += (-1.0 * rgbcolor.z);
        }
    }

    return isinbounds;
}


// Samples the gamut boundaries for one hue slice 
void gamutdescriptor::ProcessSlice(int huestep, double maxluma, double maxchroma){
    
    const double lumastep = maxluma / LUMA_STEPS;
    const double chromastep = maxchroma / CHROMA_STEPS;
    const double finechromastep = chromastep / FINE_CHROMA_STEPS;
    const double finelumastep = lumastep / FINE_LUMA_STEPS;
    const double hue = ((double)huestep) * ((2.0 *  std::numbers::pi_v<long double>) / HUE_STEPS);
    //printf("step is %i, hue is %f\n", huestep, hue);
    
    gridpoint grid[LUMA_STEPS][CHROMA_STEPS];
    int maxrow = LUMA_STEPS - 1;
    
    
    // step 1 -- coarse sampling 
    
    // the zero chroma column is in bounds by definition
    for (int i=0; i<LUMA_STEPS; i++){
        grid[i][0].inbounds = true;
    }
    
    // skip the first and last rows and the first column because we already know that:
    // first and last rows contain only 1 point in bounds (at chroma = 0)
    // first column is all in bounds
    for (int row = 1; row < maxrow; row++){
        double rowluma = row * lumastep;
        for (int col = 1; col < CHROMA_STEPS; col++){
            vec3 color = vec3(rowluma, col * chromastep, hue);
            /*
            vec3 rgbcolor = JzCzhzToLinearRGB(color);
            bool isinbounds = true;
            // inverse PQ function can generate NaN :( Let's assume all NaNs are out of bounds
            if (isnan(rgbcolor.x) ||  isnan(rgbcolor.y) || isnan(rgbcolor.z) || (rgbcolor.x > 1.0) || (rgbcolor.x < 0.0) || (rgbcolor.y > 1.0) || (rgbcolor.y < 0.0) || (rgbcolor.z > 1.0) || (rgbcolor.z < 0.0)){
                isinbounds = false;
            }
            grid[row][col].inbounds = isinbounds;
            */
            double dummy;
            grid[row][col].inbounds = IsJzCzhzInBounds(color, dummy);
            
            //printf("row %i, col %i, color: %f, %f, %f, rgbcolor %f, %f, %f, inbounds %i\n", row, col, color.x, color.y, color.z, rgbcolor.z, rgbcolor.y, rgbcolor.z, isinbounds);
        }
    }
    
    // step 2 -- fine sampling
    // again skip the top and bottom rows
    // skip the last column because this is two-columns-at-once operation
    int maxcol = CHROMA_STEPS - 1;
    for (int row = 1; row < maxrow; row++){
        double rowluma = row * lumastep;
        for (int col =0; col < maxcol; col++){
            // do fine sampling on pairs of horizontal neighbors where one is in bounds and the other out
            // (The "in-bounds after out-of-bounds" case is possible because the boundary might be slightly concave in places.)
            if ((grid[row][col].inbounds && !grid[row][col+1].inbounds) || (!grid[row][col].inbounds && grid[row][col+1].inbounds)){
                bool waitingforout = grid[row][col].inbounds;
                bool foundit = false;
                for (int finestep = 1; finestep<FINE_CHROMA_STEPS; finestep++){
                    double finex = (col * chromastep) + (finestep * finechromastep);
                    vec3 color = vec3(rowluma, finex, hue);
                    /*
                    vec3 rgbcolor = JzCzhzToLinearRGB(color);
                    bool isinbounds = true;
                    // inverse PQ function can generate NaN :( Let's assume all NaNs are out of bounds
                    if (isnan(rgbcolor.x) ||  isnan(rgbcolor.y) || isnan(rgbcolor.z) || (rgbcolor.x > 1.0) || (rgbcolor.x < 0.0) || (rgbcolor.y > 1.0) || (rgbcolor.y < 0.0) || (rgbcolor.z > 1.0) || (rgbcolor.z < 0.0)){
                        isinbounds = false;
                    }
                    */
                    double dummy;
                    bool isinbounds = IsJzCzhzInBounds(color, dummy);
                    // we found the boundary point
                    if ((waitingforout && !isinbounds) || (!waitingforout && isinbounds)){
                        boundarypoint newbpoint;
                        newbpoint.x = finex - (0.5 * finechromastep); // assume boundary is halfway between samples;
                        newbpoint.y = rowluma;
                        newbpoint.iscusp = false;
                        data[huestep].push_back(newbpoint);
                        foundit = true;
                        break; // stop fine sampling
                    }
                }
                // boundary is beyond the last fine sampling point
                if (!foundit){
                    boundarypoint newbpoint;
                    newbpoint.x = ((col + 1) * chromastep) - (0.5 * finechromastep); // assume boundary is halfway between samples;
                    newbpoint.y = rowluma;
                    newbpoint.iscusp = false;
                    data[huestep].push_back(newbpoint);
                }
            }
        }
    }
    
    // step 3 -- fine sampling to locate the cusp
    // find the highest chroma we've sampled so far
    int pointcount = data[huestep].size();
    double biggestchroma = 0.0;
    double lumaforbiggestchroma;
    for (int i=0; i<pointcount; i++){
        if (data[huestep][i].x > biggestchroma){
            biggestchroma = data[huestep][i].x;
            lumaforbiggestchroma = data[huestep][i].y;
        }
    }
    // we need to take the half sample back off so we don't miss when that's an over-estimate
    // actually take off a full step in case there's floating point errors
    biggestchroma -= finechromastep; 
    // scan for the cusp
    double scanminluma = lumaforbiggestchroma - lumastep;
    double scanmaxluma = lumaforbiggestchroma + lumastep;
    double scanluma = scanminluma;
    double maptoluma = lumaforbiggestchroma;
    double cuspchroma = biggestchroma;
    while (scanluma <= scanmaxluma){
        vec3 color = vec3(scanluma, biggestchroma, hue);
        double dummy;
        //vec3 rgbcolor = JzCzhzToLinearRGB(color);
        // only process this row if it's in bounds at the biggest chroma found so far
        //if (!(isnan(rgbcolor.x) ||  isnan(rgbcolor.y) || isnan(rgbcolor.z) || (rgbcolor.x > 1.0) || (rgbcolor.x < 0.0) || (rgbcolor.y > 1.0) || (rgbcolor.y < 0.0) || (rgbcolor.z > 1.0) || (rgbcolor.z < 0.0))){
        if (IsJzCzhzInBounds(color, dummy)){
            double scanchroma = cuspchroma;
            while (scanchroma <= maxchroma){
                color = vec3(scanluma, scanchroma, hue);
                //rgbcolor = JzCzhzToLinearRGB(color);
                // we've gone out of bounds, so we can stop now
                //if (isnan(rgbcolor.x) ||  isnan(rgbcolor.y) || isnan(rgbcolor.z) || (rgbcolor.x > 1.0) || (rgbcolor.x < 0.0) || (rgbcolor.y > 1.0) || (rgbcolor.y < 0.0) || (rgbcolor.z > 1.0) || (rgbcolor.z < 0.0)){
                if (!IsJzCzhzInBounds(color, dummy)){
                    double boundary = scanchroma - (0.5 * finechromastep); // assume boundary is halfway between samples;
                    if (boundary > cuspchroma){
                        cuspchroma = boundary;
                        maptoluma = scanluma;
                    }
                    break;
                }
                scanchroma += finechromastep;
            }
        }
        scanluma += finelumastep;
    }
    boundarypoint newbpoint;
    newbpoint.x = cuspchroma;
    newbpoint.y = maptoluma;
    newbpoint.iscusp = true;
    data[huestep].push_back(newbpoint);
    cusplumalist[huestep] = maptoluma;
    cuspchromalist[huestep] = cuspchroma;
    
    // step 4 -- insert the black and white points that we skipped because they're known
    // also shuffle stuff around to make the later sorting step faster
    newbpoint.x = 0;
    newbpoint.y = maxluma;
    newbpoint.iscusp = false;
    data[huestep].push_back(newbpoint);
    std::reverse( data[huestep].begin(),  data[huestep].end());
    newbpoint.x = 0;
    newbpoint.y = 0;
    newbpoint.iscusp = false;
    data[huestep].push_back(newbpoint);

    // step 5 -- order the points by their "pitching angle" from neutral gray
    // (this is necessary because concavities in the boundary might cause more than one point at a given luma value)
    // thought: the paper uses neutral gray, but the cusp might be a better reference point if there are large concavities. hmm... 
    vec2 neutralgray = vec2(0, maxluma * 0.5);
    vec2 white = vec2(0, maxluma);
    vec2 neutralgraytowhite = white - neutralgray;
    neutralgraytowhite.normalize();
    pointcount = data[huestep].size();
    for (int i=0; i<pointcount; i++){
        vec2 thispoint = vec2(data[huestep][i].x, data[huestep][i].y);
        vec2 neutralgraytothispoint = thispoint - neutralgray;
        neutralgraytothispoint.normalize();
        data[huestep][i].angle = clockwiseAngle(neutralgraytowhite, neutralgraytothispoint);
    }
    // bubble sort by angle
    for (int i = 0; i< pointcount - 1; i++){
        for (int j = 0; j< pointcount - i - 1; j++){
            if (data[huestep][j].angle > data[huestep][j+1].angle){
                boundarypoint temppoint = data[huestep][j];
                data[huestep][j] = data[huestep][j+1];
                data[huestep][j+1] = temppoint;
            }
        }
    }
    
    // check for duplicate points
    bool cleancheck = false;
    while (!cleancheck){
        cleancheck = true;
        pointcount = data[huestep].size();
        for (int i = 0; i< pointcount - 1; i++){
            if ((fabs(data[huestep][i].x - data[huestep][i+1].x) < EPSILON) && (fabs(data[huestep][i].y - data[huestep][i+1].y) < EPSILON)){
                //printf("deduplicating!\n");
                if (data[huestep][i].iscusp){
                    data[huestep][i+1].iscusp = true;
                }
                data[huestep].erase(data[huestep].begin() + i);
                cleancheck = false;
                break;
            }
        }
    }
    
    
    
    /*
    if (huestep == 0){
        printf("hue: %f size of vector: %i\n", hue, (int)data[huestep].size());
        printf("chroma\t\tluma\t\tangle\t\tcusp\n");
        for (int i = 0; i<(int)data[huestep].size(); i++){
            //printf("point %i: x = %f, y = %f\n", i, data[huestep][i].x, data[huestep][i].y);
            printf("%f\t%f\t%f\t%i\n", data[huestep][i].x, data[huestep][i].y, (data[huestep][i].angle * 180.0) / (double)std::numbers::pi_v<long double>, data[huestep][i].iscusp);
        }
    }
    */
    
    // set the fake point used for VP's extrapolation
    bool foundpoint = false;
    int i;
    for (i=0; i<pointcount; i++){
        if (data[huestep][i].iscusp){
            bool breakout = false;
            for (int j = i-1; j >= 0; j--){
                // 3x max chroma should be far enough out to catch everything, but not as problematically far out as zero-luma intersection can sometimes be
                // Go back until we hit a point that's at least a lumastep above. Otherwise slope might be flat b/c basically the same point twice.
                if ((data[huestep][j].y - data[huestep][i].y >= lumastep) && lineIntersection2D(vec2(data[huestep][j].x, data[huestep][j].y), vec2(data[huestep][i].x, data[huestep][i].y), vec2(3.0 * maxchroma, 0.0), vec2(3.0 * maxchroma, maxluma), fakepoints[huestep])){
                    foundpoint = true;
                    breakout = true;
                    break;
                }
                if (breakout){
                    break;
                }
            }
        }
    }
    if (!foundpoint){
        printf("Something went wrong in ProcessSlice(). No intercept for VP's fake point! Point is %f, %f and index is %i\n", data[huestep][i].x, data[huestep][i].y, i);
    }
    
    foundpoint = false;
    for (i=0; i<pointcount; i++){
        if (data[huestep][i].iscusp){
            /*
            bool breakout = false;
            for (int j = i+1; j < pointcount; j++){
                if ((data[huestep][i].y - data[huestep][j].y >= lumastep) && lineIntersection2D(vec2(data[huestep][j].x, data[huestep][j].y), vec2(data[huestep][i].x, data[huestep][i].y), vec2(0.0, 1.5 * maxluma), vec2(1.0, 1.5 * maxluma), ufakepoints[huestep])){
                    foundpoint = true;
                    breakout = true;
                    break;
                }
                if (breakout){
                    break;
                }
            }
            */
            // just use the line from 0,0 to cusp, because "in bounds" will be defined by that later
            if (lineIntersection2D(vec2(0.0, 0.0), vec2(data[huestep][i].x, data[huestep][i].y), vec2(0.0, 1.5 * maxluma), vec2(1.0, 1.5 * maxluma), ufakepoints[huestep])){
                foundpoint = true;
                break;
            }
        }
    }
    if (!foundpoint){
        printf("Something went wrong in ProcessSlice(). No intercept for VP's upper fake point! Point is %f, %f and index is %i\n", data[huestep][i].x, data[huestep][i].y, i);
    }
    
    
    return;
}

// Finds the point where the line from the focal point (chroma 0, luma = focalpointluma) to color intercepts the gamut boundary in the 2D hue splice specified by hueindex.
// boundtype is used for the VP gamut mapping algorithm
// BOUND_ABOVE extends the just-above-the-cusp segment indefinitely to the right and ignores the below-the-cusp segments
vec2 gamutdescriptor::getBoundary2D(vec2 color, double focalpointluma, int hueindex, int boundtype){

    vec2 focalpoint = vec2(0.0, focalpointluma);
    int linecount = data[hueindex].size() - 1;
    vec2 intersections[linecount];

    // Note: We'll get false positives if mapping towards white.
    // If we ever want to do that, we'll need to selectively flip the loop order.
    
    bool foundcusp = false;
    for (int i = 0; i<linecount; i++){
        vec2 bound1 = vec2(data[hueindex][i].x, data[hueindex][i].y);
        vec2 bound2 = vec2(data[hueindex][i+1].x, data[hueindex][i+1].y);
        bool breaktime = false;
        if ((boundtype == BOUND_ABOVE) && data[hueindex][i].iscusp){
            breaktime = true;
            bound2 = fakepoints[hueindex];
        }
        if (!foundcusp && (boundtype == BOUND_BELOW)){
            if (data[hueindex][i+1].iscusp){
                foundcusp = true;
                bound1 = ufakepoints[hueindex];
            }
            else {
                continue;
            }
        }
        vec2 intersection;
        bool intersects = lineIntersection2D(focalpoint, color, bound1, bound2, intersection);
        //printf("i is %i, focalpoint %f, %f, color, %f, %f, bound1 %f, %f, bound2 %f, %f, intersects %i, at %f, %f\n", i, focalpoint.x, focalpoint.y, color.x, color.y, bound1.x, bound1.y, bound2.x, bound2.y, intersects, intersection.x, intersection.y);
        if (intersects && isBetween2D(bound1, intersection, bound2)){
            return intersection;
        }
        intersections[i] = intersection; // save the intersection in case we need to do step 2
        if (breaktime){
            break;
        }
    }
    // if we made it this far, we've probably had a floating point error that caused isBetween2D() to give a wrong answer
    // which means we are probably very near one of the boundary nodes
    // so just take the node closest to an intersection
    // (this should be rare, so we're doing a second loop rather than slow down the first.)
    float bestdist = DBL_MAX;
    vec2 bestpoint = vec2(0,0);
    vec2 bestnode = vec2(0,0);
    foundcusp = false;
    for (int i = 0; i<linecount; i++){
        vec2 bound1 = vec2(data[hueindex][i].x, data[hueindex][i].y);
        vec2 bound2 = vec2(data[hueindex][i+1].x, data[hueindex][i+1].y);
        bool breaktime = false;
        if ((boundtype == BOUND_ABOVE) && data[hueindex][i].iscusp){
            breaktime = true;
            bound2 = fakepoints[hueindex];
        }
        if (!foundcusp && (boundtype == BOUND_BELOW)){
            if (data[hueindex][i+1].iscusp){
                foundcusp = true;
                bound1 = ufakepoints[hueindex];
            }
            else {
                continue;
            }
        }
        vec2 intersection = intersections[i];
        vec2 diff = intersection - bound1;
        double diffdist = diff.magnitude();
        if (diffdist < bestdist){
            bestdist = diffdist;
            bestnode = bound1;
            bestpoint = intersection;
        }
        diff = bound2 - intersection;
        diffdist = diff.magnitude();
        if (diffdist < bestdist){
            bestdist = diffdist;
            bestnode = bound2;
            bestpoint = intersection;
        }
        if (breaktime){
            break;
        }
    }
    if (bestdist > EPSILONDONTCARE){
        // Adjusted epsilon to keep this quieter...
        printf("Something went really wrong in gamutdescriptor::getBoundary(). bestdist is %f, boundtype is %i, color: %f, %f; focal point %f, %f; best point: %f, %f; bestnode: %f, %f.\nboundary nodes:\n", bestdist, boundtype, color.x, color.y, focalpoint.x, focalpoint.y, bestpoint.x, bestpoint.y, bestnode.x, bestnode.y);
        for (int i=0; i<(int)data[hueindex].size(); i++){
            printf("\t\tnode %i: %f, %f, cusp=%i\n", i, data[hueindex][i].x, data[hueindex][i].y, data[hueindex][i].iscusp);
        }
        printf("\t\tfakepoint: %f, %f\n", fakepoints[hueindex].x, fakepoints[hueindex].y);
    }
    return bestpoint;
}

// Finds the point where the line from the focal point (chroma 0, luma = focalpointluma, hue = color's hue) to color intercepts the gamut boundary.
// hueindex is the index of the adjacent sampled hue splice below color's hue. (This was computed before, so it's passed for efficiency's sake) 
// boundtype is used for the VP gamut mapping algorithm
vec3 gamutdescriptor::getBoundary3D(vec3 color, double focalpointluma, int hueindex, int boundtype, bool dospiralcarisma){
    
    // Bascially we're going to call getBoundary2D() for the two adjacent sampled hue slices,
    // then do a line/plane intersection to get the final answer.
    
    vec2 color2D = vec2(color.y, color.x); // chroma is x; luma is y
    
    vec2 focalpoint = vec2(0.0, focalpointluma);
    
    // find the boundary at the floor hue angle.
    vec2 floorbound2D = getBoundary2D(color2D, focalpointluma, hueindex, boundtype);    

    // now we have a miserable time if spiralcarisma is enabled
    if (dospiralcarisma){
        vec2 farthestbound = floorbound2D; // shouldn't need to initialize this since we should always have a result, but let's have something to fall back to just in case
        double farthestdist = 0.0;
        // first check if the portion of the slice itself containing the inteception with the boundary hasn't warped somewhere else 
        if ((!rotationneeded[hueindex]) || ((floorbound2D.x > selfwarp[hueindex].floor) && (floorbound2D.x <= selfwarp[hueindex].ceiling))){
            vec2 thisvec = floorbound2D - focalpoint;
            farthestdist = thisvec.magnitude();
            //printf("initial boundary for floor slice  %i is ok\n", hueindex);
            // don't need to set the boundary point b/c already did
        }
        for (int i=0; i<(int)impingingslices[hueindex].size(); i++){
            vec2 somebound = getBoundary2D(color2D, focalpointluma, impingingslices[hueindex][i].index, boundtype);
            if ((somebound.x > impingingslices[hueindex][i].floor) && (somebound.x <= impingingslices[hueindex][i].ceiling)){
                vec2 thisvec = somebound - focalpoint;
                double thisdist = thisvec.magnitude();
                if (thisdist > farthestdist){
                    farthestdist = thisdist;
                    farthestbound = somebound;
                    //printf("expanding boundary for floor slice %i with boundary from impinging slince %i\n", hueindex, impingingslices[hueindex][i].index);
                }
            }
        }
        floorbound2D = farthestbound;
    }

    double floorhue = hueindex * HuePerStep;
    vec3 floorbound3D = vec3(floorbound2D.y, floorbound2D.x, floorhue); // again need to transpose x and y
    
    vec3 output = floorbound3D;
    
    // assuming the hue isn't exactly at the floor, find the boundary at the ceiling hue angle too

    if (color.z != floorhue){
        int ceilhueindex = hueindex +1;
        if (ceilhueindex == HUE_STEPS){
            ceilhueindex = 0;
        }
        vec2 ceilbound2D = getBoundary2D(color2D, focalpointluma, ceilhueindex, boundtype);
        
        // now we have a miserable time if spiralcarisma is enabled
        if (dospiralcarisma){
            vec2 farthestbound = ceilbound2D; // shouldn't need to initialize this since we should always have a result, but let's have something to fall back to just in case
            double farthestdist = 0.0;
            // first check if the portion of the slice itself containing the inteception with the boundary hasn't warped somewhere else 
            if ((!rotationneeded[ceilhueindex]) || ((ceilbound2D.x > selfwarp[ceilhueindex].floor) && (ceilbound2D.x <= selfwarp[ceilhueindex].ceiling))){
                vec2 thisvec = ceilbound2D - focalpoint;
                farthestdist = thisvec.magnitude();
                // don't need to set the boundary point b/c already did
                //printf("initial boundary for ceiling slice  %i is ok\n", ceilhueindex);
            }
            for (int i=0; i<(int)impingingslices[ceilhueindex].size(); i++){
                vec2 somebound = getBoundary2D(color2D, focalpointluma, impingingslices[ceilhueindex][i].index, boundtype);
                if ((somebound.x > impingingslices[ceilhueindex][i].floor) && (somebound.x <= impingingslices[ceilhueindex][i].ceiling)){
                    vec2 thisvec = somebound - focalpoint;
                    double thisdist = thisvec.magnitude();
                    if (thisdist > farthestdist){
                        farthestdist = thisdist;
                        farthestbound = somebound;
                        //printf("expanding boundary for ceiling slice %i with boundary from impinging slince %i\n", ceilhueindex, impingingslices[ceilhueindex][i].index);
                    }
                }
            }
            ceilbound2D = farthestbound;
        }
        
        double ceilhue = ceilhueindex * HuePerStep;
        vec3 ceilbound3D = vec3(ceilbound2D.y, ceilbound2D.x, ceilhue); // again need to transpose x and y
        
        // now find where the line between the two boundary points intersects the plane containing the real hue
        // need to convert to cartesian coordinates to do that
        vec3 cartfloorbound = Depolarize(floorbound3D);
        vec3 cartceilbound = Depolarize(ceilbound3D);
        vec3 floortoceil = cartceilbound - cartfloorbound;
        floortoceil.normalize();
        // also need some points for the plane
        vec3 cartcolor = Depolarize(color);
        vec3 cartblack = Depolarize(vec3(0.0, 0.0, color.z));
        vec3 cartgray = Depolarize(vec3(color.x, 0.0, color.z));
        plane hueplane;
        hueplane.initialize(cartblack, cartcolor, cartgray);
        vec3 tweenbound;
        // if the boundary is black or white, we'll get  a NAN error we need to bypass
        // (although this in itself is probably bad input...)
        if ((floorbound3D.y < EPSILONZERO) && (ceilbound3D.y < EPSILONZERO)){
            // x and y are correct from above
            output.z = color.z;
        }
        else{
            if (!linePlaneIntersection(tweenbound, cartfloorbound, floortoceil, hueplane.normal, hueplane.point)){
                printf("Something went very wrong in getBoundary3D(), boundtype is %i\n", boundtype);
                printf("\tinput was %f, %f, %f\n", color.x, color.y, color.z);
                printf("\tfloorbound %f, %f; ceilbound %f, %f\n", floorbound2D.x, floorbound2D.y, ceilbound2D.x, ceilbound2D.y);
                printf("\t3D: floor %f, %f, %f; mid %f, %f, %f; ceil %f, %f, %f\n", floorbound3D.x, floorbound3D.y, floorbound3D.z, output.x, output.y, output.z, ceilbound3D.x, ceilbound3D.y, ceilbound3D.z);
            }
            output = Polarize(tweenbound);
        }
        //printf("input was %f, %f, %f\n", color.x, color.y, color.z);
        //printf("floorbound %f, %f; ceilbound %f, %f\n", floorbound2D.x, floorbound2D.y, ceilbound2D.x, ceilbound2D.y);
        //printf("3D: floor %f, %f, %f; mid %f, %f, %f; ceil %f, %f, %f\n", floorbound3D.x, floorbound3D.y, floorbound3D.z, output.x, output.y, output.z, ceilbound3D.x, ceilbound3D.y, ceilbound3D.z);
        
    }
    return output;
}

// given the x and y coordinates of a point in xyY space,
// find a Y such that the corresponding linear RGB is 1.0 for lockcolor (LOCKRED, LOCKGREEN, or LOCKBLUE).
// output the linear RGB triplet and set Y to the Y value
// (this function is used by color correction circuits)
vec3 gamutdescriptor::xyYhillclimb(double x, double y, int lockcolor, double &Y){
    double high = 1.0;
    double low = 0.0;
    double Yguess = 0.0;
    vec3 RGBguess;
    int stepcount = 0;
    // binary search
    // note: There's a more efficient way to binary search floats by linear searching the exponent then binary searching the mantissa. But I'm lazy, and this is readable, and it will only run a handful of times so I don't care about the performance cost.
    while (true){
        Yguess = (low + high) * 0.5;
        vec3 xyYguess = vec3(x, y, Yguess);
        vec3 XYZguess = xyYtoXYZ(xyYguess);
        RGBguess = multMatrixByColor(inverseMatrixNPM, XYZguess);
        double checkcolor = 0.0;
        switch(lockcolor){
            case LOCKRED:
                checkcolor = RGBguess.x;
                break;
            case LOCKGREEN:
                checkcolor = RGBguess.y;
                break;
            case LOCKBLUE:
                checkcolor = RGBguess.z;
                break;
            default:
                printf("Something went very wrong in xyYhillclimb(), lockcolor is %i\n", lockcolor);
                break;
        }
        double offby = fabs(1.0 - checkcolor);
        if (offby < EPSILONZERO){
            break;
        }
        // binary search should take O(log2(n)+1) at worst
        // since EPSILONZERO is 1e-10 in constants.h, we should finish in 31 steps.
        // so 50 means something is definitely wrong
        else if (stepcount > 50){
            printf("Something went very wrong in xyYhillclimb(), stepcount is %i\n", stepcount);
            break;
        }
        else {
            if (checkcolor > 1.0){
                high = Yguess;
            }
            else {
                low = Yguess;
            }
            stepcount++;
        }
    }
    // set outputs
    Y = Yguess;
    switch(lockcolor){
        case LOCKRED:
            RGBguess.x = 1.0;
            break;
        case LOCKGREEN:
            RGBguess.y = 1.0;
            break;
        case LOCKBLUE:
            RGBguess.z = 1.0;
            break;
        default:
            printf("Something went very wrong in xyYhillclimb(), lockcolor is %i\n", lockcolor);
            break;
    }
    return RGBguess;
}

// Finds the rotation (in radians) to be applied to each primary/secondary color.
// If the primary/secondary color is representable in the destination gamut, then 0.
// If the primary/secondary color is not representable in the destination gamut...
// ...and if its luma and chroma at the destination gamut's primary's/secondary's hue angle would be in bounds,
//          or would be out of bounds, but less far,
//          then the primary's/secondary's hue angle minus 
// ...but if the rotated color would be at least equally far out of bounds, then 0.
void gamutdescriptor::FindPrimaryRotations(gamutdescriptor &othergamut, double maxscale, int verbose){
    
    if (verbose >= VERBOSITY_SLIGHT){
        printf("\n----------\nFinding primary/secondary rotations for %s towards %s...\n", gamutname.c_str(), othergamut.gamutname.c_str());
    }
    
    if (verbose >= VERBOSITY_SLIGHT){
        printf("Red: ");
    }
    redrotation = 0.0;
    double error;
    // if source primary is representable in dest gamut, no rotation needed
    if (!othergamut.IsJzCzhzInBounds(adjpolarredpoint, error)){
        if (verbose >= VERBOSITY_SLIGHT){
            printf("not representable in destination gamut, ");
        }
        // make sure rotation would be representable, or at least smaller error
        vec3 rotatedcolor = vec3(adjpolarredpoint.x, adjpolarredpoint.y, othergamut.polarredpoint.z);
        double rotatederror;
        bool rotategood = othergamut.IsJzCzhzInBounds(rotatedcolor, rotatederror);
        if (rotategood || (rotatederror < error)){
            redrotation = AngleDiff(othergamut.polarredpoint.z, adjpolarredpoint.z);
            if (verbose >= VERBOSITY_SLIGHT){
                if (rotategood){
                    printf("and rotation is representable, ");
                }
                else {
                    printf("and rotation is less far out of bounds, ");
                }
            }
        }
        else if (verbose >= VERBOSITY_SLIGHT){
            printf("but rotation is at least as far out of bounds, ");
        }
    }
    else if (verbose >= VERBOSITY_SLIGHT){
        printf("representable in destination gamut, ");
    }
    if (verbose >= VERBOSITY_SLIGHT){
        printf("rotation is %f\n", redrotation);
    }
    
    if (verbose >= VERBOSITY_SLIGHT){
        printf("Green: ");
    }
    greenrotation = 0.0;
    // if source primary is representable in dest gamut, no rotation needed
    if (!othergamut.IsJzCzhzInBounds(adjpolargreenpoint, error)){
        if (verbose >= VERBOSITY_SLIGHT){
            printf("not representable in destination gamut, ");
        }
        // make sure rotation would be representable, or at least smaller error
        vec3 rotatedcolor = vec3(adjpolargreenpoint.x, adjpolargreenpoint.y, othergamut.polargreenpoint.z);
        double rotatederror;
        bool rotategood = othergamut.IsJzCzhzInBounds(rotatedcolor, rotatederror);
        if (rotategood || (rotatederror < error)){
            greenrotation = AngleDiff(othergamut.polargreenpoint.z, adjpolargreenpoint.z);
            if (verbose >= VERBOSITY_SLIGHT){
                if (rotategood){
                    printf("and rotation is representable, ");
                }
                else {
                    printf("and rotation is less far out of bounds, ");
                }
            }
        }
        else if (verbose >= VERBOSITY_SLIGHT){
            printf("but rotation is at least as far out of bounds, ");
        }
    }
    else if (verbose >= VERBOSITY_SLIGHT){
        printf("representable in destination gamut, ");
    }
    if (verbose >= VERBOSITY_SLIGHT){
        printf("rotation is %f\n", greenrotation);
    }
    
    if (verbose >= VERBOSITY_SLIGHT){
        printf("Blue: ");
    }
    bluerotation = 0.0;
    // if source primary is representable in dest gamut, no rotation needed
    if (!othergamut.IsJzCzhzInBounds(adjpolarbluepoint, error)){
        if (verbose >= VERBOSITY_SLIGHT){
            printf("not representable in destination gamut, ");
        }
        // make sure rotation would be representable, or at least smaller error
        vec3 rotatedcolor = vec3(adjpolarbluepoint.x, adjpolarbluepoint.y, othergamut.polarbluepoint.z);
        double rotatederror;
        bool rotategood = othergamut.IsJzCzhzInBounds(rotatedcolor, rotatederror);
        if (rotategood || (rotatederror < error)){
            bluerotation = AngleDiff(othergamut.polarbluepoint.z, adjpolarbluepoint.z);
            if (verbose >= VERBOSITY_SLIGHT){
                if (rotategood){
                    printf("and rotation is representable, ");
                }
                else {
                    printf("and rotation is less far out of bounds, ");
                }
            }
        }
        else if (verbose >= VERBOSITY_SLIGHT){
            printf("but rotation is at least as far out of bounds, ");
        }
    }
        else if (verbose >= VERBOSITY_SLIGHT){
        printf("representable in destination gamut, ");
    }
    if (verbose >= VERBOSITY_SLIGHT){
        printf("rotation is %f\n", bluerotation);
    }
    
    if (verbose >= VERBOSITY_SLIGHT){
        printf("Yellow: ");
    }
    yellowrotation = 0.0;
    // if source primary is representable in dest gamut, no rotation needed
    if (!othergamut.IsJzCzhzInBounds(adjpolaryellowpoint, error)){
        if (verbose >= VERBOSITY_SLIGHT){
            printf("not representable in destination gamut, ");
        }
        // make sure rotation would be representable, or at least smaller error
        vec3 rotatedcolor = vec3(adjpolaryellowpoint.x, adjpolaryellowpoint.y, othergamut.polaryellowpoint.z);
        double rotatederror;
        bool rotategood = othergamut.IsJzCzhzInBounds(rotatedcolor, rotatederror);
        if (rotategood || (rotatederror < error)){
            yellowrotation = AngleDiff(othergamut.polaryellowpoint.z, adjpolaryellowpoint.z);
            if (verbose >= VERBOSITY_SLIGHT){
                if (rotategood){
                    printf("and rotation is representable, ");
                }
                else {
                    printf("and rotation is less far out of bounds, ");
                }
            }
        }
        else if (verbose >= VERBOSITY_SLIGHT){
            printf("but rotation is at least as far out of bounds, ");
        }
    }
    else if (verbose >= VERBOSITY_SLIGHT){
        printf("representable in destination gamut, ");
    }
    if (verbose >= VERBOSITY_SLIGHT){
        printf("rotation is %f\n", yellowrotation);
    }
    
    if (verbose >= VERBOSITY_SLIGHT){
        printf("Magenta: ");
    }
    magentarotation = 0.0;
    // if source primary is representable in dest gamut, no rotation needed
    if (!othergamut.IsJzCzhzInBounds(adjpolarmagentapoint, error)){
        if (verbose >= VERBOSITY_SLIGHT){
            printf("not representable in destination gamut, ");
        }
        // make sure rotation would be representable, or at least smaller error
        vec3 rotatedcolor = vec3(adjpolarmagentapoint.x, adjpolarmagentapoint.y, othergamut.polarmagentapoint.z);
        double rotatederror;
        bool rotategood = othergamut.IsJzCzhzInBounds(rotatedcolor, rotatederror);
        if (rotategood || (rotatederror < error)){
            magentarotation = AngleDiff(othergamut.polarmagentapoint.z, adjpolarmagentapoint.z);
            if (verbose >= VERBOSITY_SLIGHT){
                if (rotategood){
                    printf("and rotation is representable, ");
                }
                else {
                    printf("and rotation is less far out of bounds, ");
                }
            }
        }
        else if (verbose >= VERBOSITY_SLIGHT){
            printf("but rotation is at least as far out of bounds, ");
        }
    }
    else if (verbose >= VERBOSITY_SLIGHT){
        printf("representable in destination gamut, ");
    }
    if (verbose >= VERBOSITY_SLIGHT){
        printf("rotation is %f\n", magentarotation);
    }
    
    if (verbose >= VERBOSITY_SLIGHT){
        printf("Cyan: ");
    }
    cyanrotation = 0.0;
    // if source primary is representable in dest gamut, no rotation needed
    if (!othergamut.IsJzCzhzInBounds(adjpolarcyanpoint, error)){
        if (verbose >= VERBOSITY_SLIGHT){
            printf("not representable in destination gamut, ");
        }
        // make sure rotation would be representable, or at least smaller error
        vec3 rotatedcolor = vec3(adjpolarcyanpoint.x, adjpolarcyanpoint.y, othergamut.polarcyanpoint.z);
        double rotatederror;
        bool rotategood = othergamut.IsJzCzhzInBounds(rotatedcolor, rotatederror);
        if (rotategood || (rotatederror < error)){
            cyanrotation = AngleDiff(othergamut.polarcyanpoint.z, adjpolarcyanpoint.z);
            if (verbose >= VERBOSITY_SLIGHT){
                if (rotategood){
                    printf("and rotation is representable, ");
                }
                else {
                    printf("and rotation is less far out of bounds, ");
                }
            }
        }
        else if (verbose >= VERBOSITY_SLIGHT){
            printf("but rotation is at least as far out of bounds, ");
        }
    }
    else if (verbose >= VERBOSITY_SLIGHT){
        printf("representable in destination gamut, ");
    }
    if (verbose >= VERBOSITY_SLIGHT){
        printf("rotation is %f\n", cyanrotation);
    }
    
    if (maxscale < 1.0){
        if (verbose >= VERBOSITY_SLIGHT){
            printf("\nscaling all rotations by %f\n", maxscale);
        }
        redrotation *= maxscale;
        yellowrotation *= maxscale;
        greenrotation *= maxscale;
        cyanrotation *= maxscale;
        bluerotation *= maxscale;
        magentarotation *= maxscale;
    }
    
    redtoyellowpolardist = AngleDiff(adjpolaryellowpoint.z, adjpolarredpoint.z);
    yellowtogreenpolardist = AngleDiff(adjpolargreenpoint.z, adjpolaryellowpoint.z);
    greentocyanpolardist = AngleDiff(adjpolarcyanpoint.z, adjpolargreenpoint.z);
    cyantobluepolardist = AngleDiff(adjpolarbluepoint.z, adjpolarcyanpoint.z);
    bluetomagentapolardist = AngleDiff(adjpolarmagentapoint.z, adjpolarbluepoint.z);
    magentatoredpolardist = AngleDiff(adjpolarredpoint.z, adjpolarmagentapoint.z);
    if (verbose >= VERBOSITY_SLIGHT){
        printf("\nAngular distances:\nRed to yellow: %f\nYellow to green: %f\nGreen to cyan: %f\nCyan to blue: %f\nBlue to magenta: %f\nMagenta to red: %f\n", redtoyellowpolardist, yellowtogreenpolardist, greentocyanpolardist, cyantobluepolardist, bluetomagentapolardist, magentatoredpolardist);
    }
    
    /* no... these should just be raw subtraction since we will have negative inputs
    redtoyellowrotatediff = AngleDiff(yellowrotation, redrotation);
    yellowtogreenrotatediff = AngleDiff(greenrotation, yellowrotation);
    greentocyanrotatediff = AngleDiff(cyanrotation, greenrotation);
    cyantobluerotatediff  = AngleDiff(bluerotation, cyanrotation);
    bluetomagentarotatediff = AngleDiff(magentarotation, bluerotation);
    magentatoredrotatediff = AngleDiff(redrotation, magentarotation);
    */
    redtoyellowrotatediff = yellowrotation - redrotation;
    yellowtogreenrotatediff = greenrotation - yellowrotation;
    greentocyanrotatediff = cyanrotation - greenrotation;
    cyantobluerotatediff  = bluerotation - cyanrotation;
    bluetomagentarotatediff = magentarotation - bluerotation;
    magentatoredrotatediff = redrotation - magentarotation;
    if (verbose >= VERBOSITY_SLIGHT){
        printf("\nRotation deltas:\nRed to yellow: %f\nYellow to green: %f\nGreen to cyan: %f\nCyan to blue: %f\nBlue to magenta: %f\nMagenta to red: %f\n", redtoyellowrotatediff, yellowtogreenrotatediff, greentocyanrotatediff, cyantobluerotatediff, bluetomagentarotatediff, magentatoredrotatediff);
    }
    
    
    // test code
    /*
    printf("testing FindHueMaxRotation()...\n");
    vec3 tempcolor;
    double temprotate;
    tempcolor = linearRGBtoJzCzhz(vec3(1.0, 0.0, 0.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Red: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(1.0, 0.5, 0.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Orange: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(1.0, 1.0, 0.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Yellow: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(0.5, 1.0, 0.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Slime: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(0.0, 1.0, 0.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Green: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(0.0, 1.0, 0.5));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Seagreen: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(0.0, 1.0, 1.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Cyan: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(0.0, 0.5, 1.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Greenish Blue: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(0.0, 0.0, 1.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Blue: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(5.0, 0.0, 1.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Purplish Blue: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(1.0, 0.0, 1.0));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Magenta: %f\n", temprotate);
    tempcolor = linearRGBtoJzCzhz(vec3(1.0, 0.0, 0.5));
    temprotate = FindHueMaxRotation(tempcolor.z);
    printf("Purplish Red: %f\n", temprotate);
    */
    
    return;
    
}

// finds the max rotation in radians for a given hue
double gamutdescriptor::FindHueMaxRotation(double hue){
    enum color_is {
        IS_MAGENTA_TO_RED,
        IS_RED_TO_YELLOW,
        IS_YELLOW_TO_GREEN,
        IS_GREEN_TO_CYAN,
        IS_CYAN_TO_BLUE,
        IS_BLUE_TO_MAGENTA
    };
    color_is whichcolor;
    if (hue < adjpolarredpoint.z){
        whichcolor = IS_MAGENTA_TO_RED;
    }
    else if (hue < adjpolaryellowpoint.z){
        whichcolor = IS_RED_TO_YELLOW;
    }
    else if (hue < adjpolargreenpoint.z){
        whichcolor = IS_YELLOW_TO_GREEN;
    }
    else if (hue < adjpolarcyanpoint.z){
        whichcolor = IS_GREEN_TO_CYAN;
    }
    else if (hue < adjpolarbluepoint.z){
        whichcolor = IS_CYAN_TO_BLUE;
    }
    else if (hue < adjpolarmagentapoint.z){
        whichcolor = IS_BLUE_TO_MAGENTA;
    }
    else {
        // we wrapped
        whichcolor = IS_MAGENTA_TO_RED;
    }
    
    double thisdist;
    double fulldist;
    double baseangle;
    double fullangledelta;
    
    switch (whichcolor){
        case IS_MAGENTA_TO_RED:
            thisdist = AngleDiff(hue, adjpolarmagentapoint.z);
            fulldist = magentatoredpolardist;
            baseangle = magentarotation;
            fullangledelta = magentatoredrotatediff;
            break;
        case IS_RED_TO_YELLOW:
            thisdist = AngleDiff(hue, adjpolarredpoint.z);
            fulldist = redtoyellowpolardist;
            baseangle = redrotation;
            fullangledelta = redtoyellowrotatediff;
            break;
        case IS_YELLOW_TO_GREEN:
            thisdist = AngleDiff(hue, adjpolaryellowpoint.z);
            fulldist = yellowtogreenpolardist;
            baseangle = yellowrotation;
            fullangledelta = yellowtogreenrotatediff;
            break;
        case IS_GREEN_TO_CYAN:
            thisdist = AngleDiff(hue, adjpolargreenpoint.z);
            fulldist = greentocyanpolardist;
            baseangle = greenrotation;
            fullangledelta = greentocyanrotatediff;
            break;
        case IS_CYAN_TO_BLUE:
            thisdist = AngleDiff(hue, adjpolarcyanpoint.z);
            fulldist = cyantobluepolardist;
            baseangle = cyanrotation;
            fullangledelta = cyantobluerotatediff;
            break;
        case IS_BLUE_TO_MAGENTA:
            thisdist = AngleDiff(hue, adjpolarbluepoint.z);
            fulldist = bluetomagentapolardist;
            baseangle = bluerotation;
            fullangledelta = bluetomagentarotatediff;
            break;
        default:
            printf("oh dear, unreachable state got reached...");
            break;
    }

    double distanceshare = thisdist / fulldist;
    double output = baseangle + (distanceshare * fullangledelta);
    
    return output;
}

// finds spiral carisma rotation in radians for a given JzCzhz color
double gamutdescriptor::FindHueRotation(vec3 input){

    // find the index for the hue angle
    double ceilweight;
    int floorhueindex = hueToFloorIndex(input.z, ceilweight);
    int ceilhueindex = floorhueindex + 1;
    if (ceilhueindex == HUE_STEPS){
        ceilhueindex = 0;
    }
    
    // take weighted average of cusp chroma in adjacent slices
    double floorcuspchroma = cuspchromalist[floorhueindex];
    double ceilcuspchroma = cuspchromalist[ceilhueindex];
    double cuspchroma = ((1.0 - ceilweight) * floorcuspchroma) + (ceilweight * ceilcuspchroma);
    
    // what percent of cusp chroma is this color?
    double chromapercent = input.y / cuspchroma;
    if (chromapercent > 1.0){
        chromapercent = 1.0;
    }
    
    // if we are below floor, return 0 (no rotation)
    if (chromapercent <= spiralcarismafloor){
        return 0.0;
    }
    
    // find max rotation
    double maxrotation = FindHueMaxRotation(input.z);
    
    // if we are above ceiling, return max rotation
    if (chromapercent >= spiralcarismaceiling){
        return maxrotation;
    }
    
    // otherwise use our mapping function
    double scalefactor;
    if (spiralcarismascalemode == SC_EXPONENTIAL){
        scalefactor = powermap(spiralcarismafloor, spiralcarismaceiling, chromapercent, spiralcharismaexponent);
    }
    else if (spiralcarismascalemode == SC_CUBIC_HERMITE){
        scalefactor = cubichermitemap(spiralcarismafloor, spiralcarismaceiling, chromapercent);
    }
    else {
        // if we somehow have an invalid mapping mode passed in, just do linear
        scalefactor = chromapercent;
    }
    
    return maxrotation * scalefactor;
    
}

// This function is dead. It belonged to an attempted fix for VP's step3 issues that didn't work out well
// Returns a vector representing the direction from the cusp to the just-above-the-cusp boundary node at the given hue.
// hueindexA and hueindexB are the indices for the adjacent sampled hue slices
/*
vec2 gamutdescriptor::getLACSlope(int hueindexA, int hueindexB, double hue){
    
    double hueA = hueindexA * HuePerStep;
    double hueB = hueindexB * HuePerStep;
    
    vec2 cuspA;
    vec2 cuspB;
    // todo: just store the whole cusp and refactor the cusp luma stuff 
    int nodecount = data[hueindexA].size();
    for (int i = 1; i< nodecount; i++){
        if (data[hueindexA][i].iscusp){
            cuspA = vec2(data[hueindexA][i].x, data[hueindexA][i].y);
            break;
        }
    }
    nodecount = data[hueindexB].size();
    for (int i = 1; i< nodecount; i++){
        if (data[hueindexB][i].iscusp){
            cuspB = vec2(data[hueindexB][i].x, data[hueindexB][i].y);
            break;
        }
    }
    vec2 otherA = fakepoints[hueindexA];
    vec2 otherB = fakepoints[hueindexB];
    vec3 cuspA3D = vec3(cuspA.y, cuspA.x, hueA); // y is luma; x is chroma; J is luma, C is chroma
    vec3 otherA3D = vec3(otherA.y, otherA.x, hueA);  // y is luma; x is chroma; J is luma, C is chroma
    vec3 cuspB3D = vec3(cuspB.y, cuspB.x, hueB);  // y is luma; x is chroma; J is luma, C is chroma
    vec3 otherB3D = vec3(otherB.y, otherB.x, hueB); // y is luma; x is chroma; J is luma, C is chroma
    
    vec3 cuspA3Dcarto = Depolarize(cuspA3D);
    vec3 otherA3Dcarto = Depolarize(otherA3D);
    vec3 cuspB3Dcarto = Depolarize(cuspB3D);
    vec3 otherB3Dcarto = Depolarize(otherB3D);
    vec3 cuspAtoB = cuspB3Dcarto - cuspA3Dcarto;
    vec3 otherAtoB = otherB3Dcarto - otherA3Dcarto;
    
    vec3 cartcolor = Depolarize(vec3(0.0, 1.0, hue));
    vec3 cartblack = Depolarize(vec3(0.0, 0.0, hue));
    vec3 cartgray = Depolarize(vec3(1.0, 0.0, hue));
    plane hueplane;
    hueplane.initialize(cartblack, cartcolor, cartgray);
    
    vec3 tweenbound;
    
    if (!linePlaneIntersection(tweenbound, cuspA3Dcarto, cuspAtoB, hueplane.normal, hueplane.point)){
        printf("Something went very wrong in getLACSlope() spot1 - %f, %f, %f and %f, %f, %f and hue = %f; carto are %f, %f, %f and %f, %f, %f; and diff is %f, %f, %f\n", cuspA3D.x, cuspA3D.y, cuspA3D.z, cuspB3D.x, cuspB3D.y, cuspB3D.z, hue, cuspA3Dcarto.x, cuspA3Dcarto.y, cuspA3Dcarto.z, cuspB3Dcarto.x, cuspB3Dcarto.y, cuspB3Dcarto.z, cuspAtoB.x, cuspAtoB.y, cuspAtoB.z);
    }
    vec3 tweencusp = Polarize(tweenbound);
    
    vec3 tweenother;
    // diff might be zero if this node is black or white; need to bypass that case because NAN
    if ((otherA3D.x == otherB3D.x) && (otherA3D.y == otherB3D.y)){
        tweenother = otherA3D;
    }
    else{
        if (!linePlaneIntersection(tweenbound, otherA3Dcarto, otherAtoB, hueplane.normal, hueplane.point)){
            printf("Something went very wrong in getLACSlope() spot2 - %f, %f, %f and %f, %f, %f and hue = %f; carto are %f, %f, %f and %f, %f, %f; and diff is %f, %f, %f\n", otherA3D.x, otherA3D.y, otherA3D.z, otherB3D.x, otherB3D.y, otherB3D.z, hue, otherA3Dcarto.x, otherA3Dcarto.y, otherA3Dcarto.z, otherB3Dcarto.x, otherB3Dcarto.y, otherB3Dcarto.z, otherAtoB.x, otherAtoB.y, otherAtoB.z);
        }
        tweenother = Polarize(tweenbound);
    }
    vec2 tweencusp2D = vec2(tweencusp.y, tweencusp.x);  // y is luma; x is chroma; J is luma, C is chroma
    vec2 tweenother2D = vec2(tweenother.y, tweenother.x);  // y is luma; x is chroma; J is luma, C is chroma
    
    //vec2 output = tweenother2D - tweencusp2D;
    vec2 output =  tweencusp2D - tweenother2D;
    
    output.normalize();
    
    return output;
}
*/

// returns the index of the adjacent sampled hue slice "below" hue,
// and stores how far hue is towards the next slice (on a 0 to 1 scale) to excess
int hueToFloorIndex(double hue, double &excess){
    int index = (int)(hue / HuePerStep);
    excess = (hue - (index * HuePerStep)) / HuePerStep;
    return index;
}

// The core function! Takes a linear RGB color, two gamut descriptors, and some gamut-mapping parameters, and outputs a remapped linear RGB color
// color: linear RGB input color
// sourcegamut: the source gamut
// destgamut: the destination gmaut
// expand: whether to apply inverse compression function when the destination gamut exceeds the source gamut
// remapfactor: size of remap zone relative to difference between gamuts
// remaplimit: size of safe zone relative to destination gamut; overrides results of remapfactor
// softknee: use soft knee compression rather than hard knee
// kneefactor: size of soft knee relative to remap zone (half of soft knee on either side of knee point)
// mapdirection: which gamut mapping algorithm to use MAP_GCUSP (actually just CUSP), MAP_HLPCM, or MAP_VP
// safezonetype: whether to use the traditional relative-to-destination-gamut approach (RMZONE_DEST_BASED) or Su, Tao, & Kim's relative-to-difference-between-gamuts approach (RMZONE_DELTA_BASED)
//  if RMZONE_DEST_BASED, then remapfactor does nothing
vec3 mapColor(vec3 color, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, bool expand, double remapfactor, double remaplimit, bool softknee, double kneefactor, int mapdirection, int safezonetype, bool dospiralcarisma){
    
    // skip the easy black and white cases with no computation
    if (color.isequal(vec3(0.0, 0.0, 0.0)) || color.isequal(vec3(1.0, 1.0, 1.0))){
        return color;
    }
    
    // convert to JzCzhz
    vec3 Jcolor = sourcegamut.linearRGBtoJzCzhz(color);
    vec2 colorCJ = vec2(Jcolor.y, Jcolor.x); // chroma is x; luma is y
    vec3 Joutput = Jcolor;
    
    // if spiralcarisma is enabled, rotate the input
    if (dospiralcarisma){
        printf("rotating hue from %f", Jcolor.z);
        double huerotation = sourcegamut.FindHueRotation(Jcolor);
        Jcolor.z = AngleAdd(Jcolor.z, huerotation);
        Joutput.z = Jcolor.z;
        printf(" to %f\n", Jcolor.z);
    }
    
    // find the index for the hue angle
    double ceilweight;
    int floorhueindex = hueToFloorIndex(Jcolor.z, ceilweight);
    int ceilhueindex = floorhueindex + 1;
    if (ceilhueindex == HUE_STEPS){
        ceilhueindex = 0;
    }
    
    double floorcuspluma = destgamut.cusplumalist[floorhueindex];
    double ceilcuspluma = destgamut.cusplumalist[ceilhueindex];
    double cuspluma = ((1.0 - ceilweight) * floorcuspluma) + (ceilweight * ceilcuspluma);
    
    // find the luma to use for the focal point
    // for CUSP, take a weighted average from the two nearest hues that were sampled
    // for HLPCM, take the input's luma
    // for VP, it depends on the step
    //      the first step is black
    //      the second step is the input's luma (as modified by step 1)
    double maptoluma;
    int boundtype = BOUND_NORMAL;
    bool skip = false;
    if (mapdirection == MAP_GCUSP){
        maptoluma = cuspluma;
    }
    else if (mapdirection == MAP_HLPCM){
        maptoluma = Jcolor.x;
    }
    else if (mapdirection == MAP_VP){
        // inverse first step, map horizontally
        if (expand){
            maptoluma = Jcolor.x;
            boundtype = BOUND_NORMAL;
            // assume paper means that step2 is only applied below the cusp
            // use source gamut's cusp in the expand case
            double sfloorcuspluma = sourcegamut.cusplumalist[floorhueindex];
            double sceilcuspluma = sourcegamut.cusplumalist[ceilhueindex];
            double scuspluma = ((1.0 - ceilweight) * sfloorcuspluma) + (ceilweight * sceilcuspluma);
            if (Jcolor.x > scuspluma){
                skip = true;
            }
        }
        // normal first step, map towards black
        else{
            maptoluma = 0.0;
            boundtype = BOUND_ABOVE;
        }
    }
    else if (mapdirection == MAP_VPR){
        // inverse first step, map away from black
        if (expand){
            maptoluma = 0.0;
            boundtype = BOUND_ABOVE;
        }
        // normal first step, map horizontally, using extrapolated bound above the cusp
        else{
            maptoluma = Jcolor.x;
            boundtype = BOUND_BELOW;
        }
    }
    else {
        printf("WTF ERROR!\n");
    }
    
    if (!skip){
        vec2 maptopoint = vec2(0.0, maptoluma);
        
        // find the boundaries
        vec3 sourceboundary3D = sourcegamut.getBoundary3D(Jcolor, maptoluma, floorhueindex, boundtype, dospiralcarisma);
        vec2 sourceboundary = vec2(sourceboundary3D.y, sourceboundary3D.x); // chroma is x; luma is y
        vec3 destboundary3D = destgamut.getBoundary3D(Jcolor, maptoluma, floorhueindex, boundtype , false);
        vec2 destboundary = vec2(destboundary3D.y, destboundary3D.x); // chroma is x; luma is y
        
        // figure out relative distances
        vec2 cuspprojtocolor = colorCJ - maptopoint;
        vec2 cuspprojtosrcbound = sourceboundary - maptopoint;
        vec2 cuspprojtodestbound = destboundary - maptopoint;
        double distcolor = cuspprojtocolor.magnitude();
        double distsource = cuspprojtosrcbound.magnitude();
        double distdest = cuspprojtodestbound.magnitude();
        // if the color is outside the source gamut, assume that's a sampling error and use the color's position as the source gamut boundary
        // (hopefully this doesn't happen much)
        if (distcolor > distsource){
            //printf("mapColor() discovered souce gamut sampling error for linear RGB input %f, %f, %f. Distance is %f, but sampled boundary distance is only %f.\n", color.x, color.y, color.z, distcolor, distsource);
            distsource = distcolor;
        }
        //printf("dist to color %f, dist to source bound %f, dist to dest bound %f\n", distcolor, distsource, distdest);
        
        bool scaleneeded = false;
        double newdist = scaledistance(scaleneeded, distcolor, distsource, distdest, expand, remapfactor, remaplimit, softknee, kneefactor, safezonetype);
        
        // remap if needed
        if (scaleneeded){
            vec2 colordir = cuspprojtocolor.normalizedcopy();
            vec2 newcolor = maptopoint + (colordir * newdist);
            Joutput.x = newcolor.y; // luma is J
            Joutput.y = newcolor.x; // chroma is C
        }
    } // end if !skip
    
    // VP has a second step
    if ((mapdirection == MAP_VP) || (mapdirection == MAP_VPR)){
        
        skip = false;
        
        vec2 icolor = vec2(Joutput.y, Joutput.x); // get back newcolor

        if (mapdirection == MAP_VP){
            // inverse second step, map away from black
            if (expand){
                maptoluma = 0.0;
                boundtype = BOUND_ABOVE;
            }
            // normal second step, map horizontally
            else{
                maptoluma = Joutput.x;
                boundtype = BOUND_NORMAL;
                // assume paper means that step2 is only applied below the cusp
                if (Joutput.x > cuspluma){
                    skip = true;
                }
            }
        }
        else if (mapdirection == MAP_VPR){
             // inverse second step, horizontally
            if (expand){
                maptoluma = Joutput.x;
                boundtype = BOUND_BELOW;
            }
            // normal second step, map to black
            else{
                maptoluma = 0.0;
                boundtype = BOUND_ABOVE;
            }
        }
        
        if (!skip){
            vec2 maptopoint = vec2(0.0, maptoluma);
            
            // find the boundaries (again)
            vec3 sourceboundary3D = sourcegamut.getBoundary3D(Joutput, maptoluma, floorhueindex, boundtype, dospiralcarisma);
            vec2 sourceboundary = vec2(sourceboundary3D.y, sourceboundary3D.x); // chroma is x; luma is y
            vec3 destboundary3D = destgamut.getBoundary3D(Joutput, maptoluma, floorhueindex, boundtype, false);
            vec2 destboundary = vec2(destboundary3D.y, destboundary3D.x); // chroma is x; luma is y
            
            // figure out relative distances
            vec2 cuspprojtocolor = icolor - maptopoint;
            vec2 cuspprojtosrcbound = sourceboundary - maptopoint;
            vec2 cuspprojtodestbound = destboundary - maptopoint;
            double distcolor = cuspprojtocolor.magnitude();
            double distsource = cuspprojtosrcbound.magnitude();
            double distdest = cuspprojtodestbound.magnitude();
            // if the color is outside the source gamut, assume that's a sampling error and use the color's position as the source gamut boundary
            // (hopefully this doesn't happen much)
            if (distcolor > distsource){
                //printf("mapColor() discovered souce gamut sampling error for linear RGB input %f, %f, %f. Distance is %f, but sampled boundary distance is only %f.\n", color.x, color.y, color.z, distcolor, distsource);
                distsource = distcolor;
            }
            //printf("dist to color %f, dist to source bound %f, dist to dest bound %f\n", distcolor, distsource, distdest);
            
            bool scaleneeded = false;
            double newdist = scaledistance(scaleneeded, distcolor, distsource, distdest, expand, remapfactor, remaplimit, softknee, kneefactor, safezonetype);
            
            // remap if needed
            if (scaleneeded){
                vec2 colordir = cuspprojtocolor.normalizedcopy();
                vec2 newcolor = maptopoint + (colordir * newdist);
                Joutput.x = newcolor.y; // luma is J
                Joutput.y = newcolor.x; // chroma is C
            }
        } //end if !skip
    } // end of VP second step
    
    
    // if no compression was needed, then skip JzCzhz and just use XYZ (possibly with Bradford)
    // should reduce floating point errors
    // on the other hand, this might introduce a discontinuity at the edge where scaling starts... ugg
    /*
    if (!scaleneeded){
        vec3 tempcolor = sourcegamut.linearRGBtoXYZ(color);
        return destgamut.XYZtoLinearRGB(tempcolor);
    }
    */
    
    return destgamut.JzCzhzToLinearRGB(Joutput);

}

// Scales the distance to a color according to parameters
// distcolor: distance from the focal point to the color
// distsource: distance from the focal point to the source gamut boundary
// distdest: distance from the focal point to the destination gamut boundary
// expand, rempafactor, rempalimit, softknee, kneefactor, safezonetype: same as for mapColor()
// changed: Whether a change was made is saved here. (No change if the color is in the safe zone.)
//      If no change, then the result should be discarded to reduce floating point errors
double scaledistance(bool &changed, double distcolor, double distsource, double distdest, bool expand, double remapfactor, double remaplimit, bool softknee, double kneefactor, int safezonetype){
    
    changed = false;
    
    double outer;
    double inner;
    if (distsource > distdest){
        outer = distsource;
        inner = distdest;
    }
    else {
        outer = distdest;
        inner = distsource;
    }
    
    double outofboundszone = outer - inner;
    double remapzone = outofboundszone * remapfactor;
    double kneepoint = inner - remapzone;
    double altknee = inner * remaplimit;
    if ((altknee > kneepoint) || (safezonetype == RMZONE_DEST_BASED)){
        kneepoint = altknee;
        remapzone = inner - kneepoint;
    }
    double kneewidth = remapzone * kneefactor;
    double halfkneewidth = kneewidth * 0.5;
    double safezonebound = (softknee) ? kneepoint - halfkneewidth : kneepoint;
    
    // sanity check
    if (safezonebound < 0.0){
        double oops = 0.0 - safezonebound;
        safezonebound += oops;
        kneepoint += oops;
        remapzone -= oops;
    }
    
    double kneetop = (softknee) ? kneepoint + halfkneewidth : kneepoint;
    double slope = remapzone / (remapzone + outofboundszone);
    
    double newdist = distcolor;
    
    // only do compression/expansion outside the safe zone
    if (distcolor > safezonebound){
        // the destination gamut is smaller than the source gamut in the color's direction, so compression might be needed
        if (distdest < distsource){

                /*
                soft knee formula: https://dsp.stackexchange.com/questions/28548/differences-between-soft-knee-and-hard-knee-in-dynamic-range-compression-drc
                T = threshhold
                W = knee width (half above and half below T)
                S = slope of compressed zone
                y = x IF x < T – W/2
                y = x + ((S-1)*((x – T + W/2)^2))/2W IF T – W/2 <= x <= T + W/2
                y = T + (x-T)*S IF x > T + W/2
                */
                
                // hard knee or above the soft knee zone
                if ((distcolor > kneetop) || !softknee){
                    newdist = kneepoint + ((distcolor - kneepoint) * slope);
                    
                }
                // inside the soft knee zone
                else {
                    newdist = distcolor + ((( slope - 1.0) * pow(distcolor - kneepoint - halfkneewidth, 2.0)) / (2.0 * kneewidth));
                }
                changed= true;
        }
        // the destination gamut is larger, so only do something if expansion is asked for
        else if ((distdest > distsource) && expand){
            double kneetopex = kneepoint;
            if (softknee){
                // the breakpoint for the inverse function is the y value of the original function evaluated at the original breakpoint
                kneetopex = kneepoint + ((kneetop - kneepoint) * slope);
            }
                        
            // hard knee or above the soft knee zone
            if ((distcolor > kneetopex) || !softknee){
                newdist = ((distcolor - kneepoint) / slope) + kneepoint;
            }
            // inside the soft knee zone
            else {
                // wow is this inverse is fugly...
                double term1 = ((2.0 * slope * kneepoint * kneewidth) + (slope * pow(kneewidth, 2.0)) - (2.0 * kneepoint * kneewidth) - pow(kneewidth, 2.0) - 2.0) / (2.0 * (slope - 1.0) * kneewidth);
                double term2 = sqrt((-2.0 * slope * kneepoint * kneewidth) - (slope * pow(kneewidth, 2.0)) + (2.0 * slope * kneewidth * distcolor) + (2.0 * kneepoint * kneewidth) + pow(kneewidth, 2.0) - (2.0 * kneewidth * distcolor) + 1.0);
                double term3 = ((slope - 1.0) * kneewidth);
                double pluscandidate = term1 + (term2 / term3);
                double minuscandidate = term1 + ((-1.0 * term2) / term3);
                // term2 is a +- sqrt. one of these should be wildly wrong; use the one that's closer to the input
                double plusdist = fabs(pluscandidate - distcolor);
                double minusdist = fabs(minuscandidate - distcolor);
                newdist = (plusdist < minusdist) ? pluscandidate : minuscandidate;
            }
            changed= true;
        }
    }
    
    return newdist;
}


// Given (linear) RGB input, returns Pr, Pg, or Pb with the largest absolute value.
// Effectively "how close are we to a primary/secondary color?"
// This is a terrible way to do this that is only being implemented to maybe sorta kinda mimic color correction circuits in old TVs.
// Some of which used (sometimes gamma corrected) Pb Pr to differentiate areas to apply different corrections.
double gamutdescriptor::linearRGBfindmaxP(vec3 input){
    double redfactor = matrixNPM[1][0];
    double greenfactor = matrixNPM[1][1];
    double bluefactor = matrixNPM[1][2];
    
    double luminosity = (redfactor * input.x) + (greenfactor * input.y) + (bluefactor * input.z);
    
    double Pr = (input.x - luminosity) / (1.0 - redfactor);
    Pr = fabs(Pr);
    Pr = clampdouble(Pr);
    
    double Pg = (input.y - luminosity) / (1.0 - greenfactor);
    Pg = fabs(Pg);
    Pg = clampdouble(Pg);
    
    double Pb = (input.z - luminosity) / (1.0 - bluefactor);
    Pb = fabs(Pb);
    Pb = clampdouble(Pb);
    
    double output = Pr;
    if (Pg > output){
        output = Pg;
    }
    if (Pb > output){
        output = Pb;
    }
    return output;
}
