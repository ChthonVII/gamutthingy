#include "gamutbounds.h"

#include "constants.h"
#include "plane.h"
#include "matrix.h"
#include "cielab.h"
#include "jzazbz.h"

//#include <stddef.h>
//#include <stdio.h>
//#include <stdbool.h>
#include <math.h>
//#include <limits>
#include <cfloat>
#include <numbers>


bool gamutdescriptor::initialize(std::string name, vec3 wp, vec3 rp, vec3 gp, vec3 bp, bool issource, vec3 dwp, int verbose){
    verbosemode = verbose;
    gamutname = name;
    whitepoint = wp;
    redpoint = rp;
    greenpoint = gp;
    bluepoint = bp;
    issourcegamut = issource;
    otherwhitepoint = dwp;
    // working in JzCzhz colorspace requires everything be converted to D65 whitepoint
    needschromaticadapt = (!whitepoint.isequal(D65));
    if (verbose >= VERBOSITY_MINIMAL){
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
    if (verbose >= VERBOSITY_MINIMAL){
        printf("\nMatrix NPM (linear RGB to XYZ) is:\n");
        print3x3matrix(matrixNPM);
    }
    
    if (!initializeInverseMatrixNPM()){
        printf("Initialization aborted!\n");
        return false;
    }
    if (verbose >= VERBOSITY_MINIMAL){
        printf("\nInverse matrix NPM (XYZ to linear RGB) is:\n");
        print3x3matrix(inverseMatrixNPM);
    }
    
    if (needschromaticadapt){
        if (!initializeChromaticAdaptationToD65()){
            printf("Initialization aborted!\n");
            return false;
        }
        if (verbose >= VERBOSITY_MINIMAL){
            printf("\nMatrix M (XYZ to XYZ chromatic adaptation of whitepoint to D65) is:\n");
            print3x3matrix(matrixMtoD65);
            printf("\nMatrix NPM-adapt (linear RGB to XYZ-D65) is:\n");
            print3x3matrix(matrixNPMadaptToD65);
            printf("\nInverse Matrix NPM-adapt (XYZ-D65 to linear RGB) is:\n");
            print3x3matrix(inverseMatrixNPMadaptToD65);
        }
    }
    
    reservespace();
    if (verbose >= VERBOSITY_MINIMAL) printf("\nSampling gamut boundaries...");
    FindBoundaries();
    if (verbose >= VERBOSITY_MINIMAL) printf(" done.\n");
    
    if (verbose >= VERBOSITY_MINIMAL) printf("\nDone initializing gamut descriptor for %s.\n----------\n", gamutname.c_str());
    return true;
}

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
    double inverseBradfordMatrix[3][3];
    if (!Invert3x3Matrix(BradfordMatrix, inverseBradfordMatrix)){
        printf("What the flying fuck?! Bradford matrix was not invertible.\n");   
        return false;
    }
    destMatrixW.x = D65.x / D65.y;
    destMatrixW.y = 1.0;
    destMatrixW.z = D65.z / D65.y;
    
    vec3 sourceRhoGammaBeta = multMatrixByColor(BradfordMatrix, matrixW);
    vec3 destRhoGammaBeta = multMatrixByColor(BradfordMatrix, destMatrixW);
    
    double coneResponseScaleMatrix[3][3] = {
        {destRhoGammaBeta.x / sourceRhoGammaBeta.x, 0.0, 0.0},
        {0.0, destRhoGammaBeta.y / sourceRhoGammaBeta.y, 0.0},
        {0.0, 0.0, destRhoGammaBeta.z / sourceRhoGammaBeta.z}
    };
    
    double tempMatrix[3][3];
    mult3x3Matrices(inverseBradfordMatrix, coneResponseScaleMatrix, tempMatrix);
    mult3x3Matrices(tempMatrix, BradfordMatrix, matrixMtoD65);
    
    mult3x3Matrices(matrixMtoD65, matrixNPM, matrixNPMadaptToD65);
    if (!Invert3x3Matrix(matrixNPMadaptToD65, inverseMatrixNPMadaptToD65)){
        printf("Disaster! Chromatic Adapation Matrix NPM is not invertible! Bad stuff will happen!\n");  
        return false;
    }
    
    return true;
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
    //if (isnan(output.x) || isnan(output.y) || isnan(output.z)) printf("NaN after depolarize!\n");
    output = JzazbzToXYZ(output);
    //if (isnan(output.x) || isnan(output.y) || isnan(output.z)) printf("NaN after JzazbzToXYZ!\n");
    output = XYZtoLinearRGB(output);
    //if (isnan(output.x) || isnan(output.y) || isnan(output.z)) printf("XYZtoLinearRG!\n");
    return output;
}

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

void gamutdescriptor::FindBoundaries(){
    
    vec3 tempcolor = linearRGBtoJzCzhz(vec3(1.0, 1.0, 1.0));
    double maxluma = tempcolor.x;
    
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
    // 5% lightness, 2% chrma, 0.1% chroma dense, ?? lightness dense, 0.2degree hue slice? 0.4?
    // for testing, just do one slice
    //ProcessSlice(3, maxluma, maxchroma);
    
    for (int huestep = 0; huestep < HUE_STEPS; huestep++){
        ProcessSlice(huestep, maxluma, maxchroma);
    } // end for hue angle
    
    
    
    return;
}

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
    
    // skip the first and last rows and the first column because we already know:
    // first and last rows contain only 1 point in bounds (at chroma = 0)
    // first column is all in bounds
    for (int row = 1; row < maxrow; row++){
        double rowluma = row * lumastep;
        for (int col = 1; col < CHROMA_STEPS; col++){
            vec3 color = vec3(rowluma, col * chromastep, hue);
            vec3 rgbcolor = JzCzhzToLinearRGB(color);
            bool isinbounds = true;
            // inverse PQ function can generate NaN :( Let's assume all NaNs are out of bounds
            if (isnan(rgbcolor.x) ||  isnan(rgbcolor.y) || isnan(rgbcolor.z) || (rgbcolor.x > 1.0) || (rgbcolor.x < 0.0) || (rgbcolor.y > 1.0) || (rgbcolor.y < 0.0) || (rgbcolor.z > 1.0) || (rgbcolor.z < 0.0)){
                isinbounds = false;
            }
            grid[row][col].inbounds = isinbounds;
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
                    vec3 rgbcolor = JzCzhzToLinearRGB(color);
                    bool isinbounds = true;
                    // inverse PQ function can generate NaN :( Let's assume all NaNs are out of bounds
                    if (isnan(rgbcolor.x) ||  isnan(rgbcolor.y) || isnan(rgbcolor.z) || (rgbcolor.x > 1.0) || (rgbcolor.x < 0.0) || (rgbcolor.y > 1.0) || (rgbcolor.y < 0.0) || (rgbcolor.z > 1.0) || (rgbcolor.z < 0.0)){
                        isinbounds = false;
                    }
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
    biggestchroma -= - (0.5 * finechromastep); // take the half sample back off so we don't miss when that's an over-estimate
    // scan for the cusp
    double scanminluma = lumaforbiggestchroma - lumastep;
    double scanmaxluma = lumaforbiggestchroma + lumastep;
    double scanluma = scanminluma;
    double maptoluma = lumaforbiggestchroma;
    double cuspchroma = biggestchroma;
    while (scanluma <= scanmaxluma){
        vec3 color = vec3(scanluma, biggestchroma, hue);
        vec3 rgbcolor = JzCzhzToLinearRGB(color);
        // only process this row if it's in bounds at the biggest chroma found so far
        if (!(isnan(rgbcolor.x) ||  isnan(rgbcolor.y) || isnan(rgbcolor.z) || (rgbcolor.x > 1.0) || (rgbcolor.x < 0.0) || (rgbcolor.y > 1.0) || (rgbcolor.y < 0.0) || (rgbcolor.z > 1.0) || (rgbcolor.z < 0.0))){
            double scanchroma = cuspchroma;
            while (scanchroma <= maxchroma){
                color = vec3(scanluma, scanchroma, hue);
                rgbcolor = JzCzhzToLinearRGB(color);
                // we've gone out of bounds, so we can stop now
                if (isnan(rgbcolor.x) ||  isnan(rgbcolor.y) || isnan(rgbcolor.z) || (rgbcolor.x > 1.0) || (rgbcolor.x < 0.0) || (rgbcolor.y > 1.0) || (rgbcolor.y < 0.0) || (rgbcolor.z > 1.0) || (rgbcolor.z < 0.0)){
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
    
    
    
    // we should have 18 points, I hope
    //if (data[huestep].size() != 21) printf("huestep %i has %i points!\n", huestep, data[huestep].size());
    
    /*
    printf("hue: %f size of vector: %i\n", hue, data[huestep].size());
    printf("chroma\t\tluma\t\tangle\t\tcusp\n");
    for (int i = 0; i<data[huestep].size(); i++){
        //printf("point %i: x = %f, y = %f\n", i, data[huestep][i].x, data[huestep][i].y);
        printf("%f\t%f\t%f\t%i\n", data[huestep][i].x, data[huestep][i].y, (data[huestep][i].angle * 180.0) / (double)std::numbers::pi_v<long double>, data[huestep][i].iscusp);
    }
    */
    
    
    return;
}

vec2 gamutdescriptor::getBoundary2D(vec2 color, double focalpointluma, int hueindex){
    vec2 focalpoint = vec2(0, focalpointluma);
    int linecount = data[hueindex].size() - 1;
    vec2 intersections[linecount];
    for (int i = 0; i<linecount; i++){
        vec2 bound1 = vec2(data[hueindex][i].x, data[hueindex][i].y);
        vec2 bound2 = vec2(data[hueindex][i+1].x, data[hueindex][i+1].y);
        vec2 intersection;
        bool intersects = lineIntersection2D(focalpoint, color, bound1, bound2, intersection);
        //printf("i is %i, focalpoint %f, %f, color, %f, %f, bound1 %f, %f, bound2 %f, %f, intersects %i, at %f, %f\n", i, focalpoint.x, focalpoint.y, color.x, color.y, bound1.x, bound1.y, bound2.x, bound2.y, intersects, intersection.x, intersection.y);
        if (intersects && isBetween2D(bound1, intersection, bound2)){
            return intersection;
        }
        intersections[i] = intersection; // save the intersection in case we need to do step 2
    }
    // if we made it this far, we've probably had a floating point error that caused isBetween2D() to give a wrong answer
    // which means we are probably very near one of the boundary nodes
    // so just take the node closest to an intersection
    // (this should be vanishingly rare, so we're doing a second loop rather than slow down the first.)
    float bestdist = DBL_MAX;
    vec2 bestpoint = vec2(0,0);
    for (int i = 0; i<linecount; i++){
        vec2 bound1 = vec2(data[hueindex][i].x, data[hueindex][i].y);
        vec2 bound2 = vec2(data[hueindex][i+1].x, data[hueindex][i+1].y);
        vec2 intersection = intersections[i];
        vec2 diff = intersection - bound1;
        double diffdist = diff.magnitude();
        if (diffdist < bestdist){
            bestdist = diffdist;
            bestpoint = bound1;
        }
        diff = bound2 - intersection;
        diffdist = diff.magnitude();
        if (diffdist < bestdist){
            bestdist = diffdist;
            bestpoint = bound2;
        }
    }
    if (bestdist > EPSILONZERO){
        printf("Something went really wrong in gamutdescriptor::getBoundary(). bestdist is %f.\n", bestdist);
    }
    return bestpoint;
}

vec3 gamutdescriptor::getBoundary3D(vec3 color, double focalpointluma, int hueindex){
    
    vec2 color2D = vec2(color.y, color.x); // chroma is x; luma is y
    
    // find the boundary at the floor hue angle.
    vec2 floorbound2D = getBoundary2D(color2D, focalpointluma, hueindex);
    // todo move to #def
    const double hueperstep = ((2.0 *  std::numbers::pi_v<long double>) / HUE_STEPS);
    double floorhue = hueindex * hueperstep;
    vec3 floorbound3D = vec3(floorbound2D.y, floorbound2D.x, floorhue); // again need to transpose x and y
    
    vec3 output = floorbound3D;
    
    // assuming the hue isn't exactly at the floor, find the boundary at the ceiling hue angle too
    if (color.z != floorhue){
        int ceilhueindex = hueindex +1;
        if (ceilhueindex == HUE_STEPS){
            ceilhueindex = 0;
        }
        vec2 ceilbound2D = getBoundary2D(color2D, focalpointluma, ceilhueindex);
        double ceilhue = ceilhueindex * hueperstep;
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
        if (!linePlaneIntersection(tweenbound, cartfloorbound, floortoceil, hueplane.normal, hueplane.point)){
            printf("Something went very wrong in getBoundary3D()\n");
        }
        output = Polarize(tweenbound);
        //printf("input was %f, %f, %f\n", color.x, color.y, color.z);
        //printf("floorbound %f, %f; ceilbound %f, %f\n", floorbound2D.x, floorbound2D.y, ceilbound2D.x, ceilbound2D.y);
        //printf("3D: floor %f, %f, %f; mid %f, %f, %f; ceil %f, %f, %f\n", floorbound3D.x, floorbound3D.y, floorbound3D.z, output.x, output.y, output.z, ceilbound3D.x, ceilbound3D.y, ceilbound3D.z);
        
    }
    return output;
}

int hueToFloorIndex(double hue, double &excess){
    // todo move to #def
    const double hueperstep = ((2.0 *  std::numbers::pi_v<long double>) / HUE_STEPS);
    //printf("hueperstep is %f\n", hueperstep);
    int index = (int)(hue / hueperstep);
    excess = (hue - (index * hueperstep)) / hueperstep;
    return index;
}

// linear RGB in and out
vec3 mapColor(vec3 color, gamutdescriptor sourcegamut, gamutdescriptor destgamut, bool expand, double remapfactor, double remaplimit, bool softknee, double kneefactor, int mapdirection, int safezonetype){
    
    // skip the easy black and white cases with no computation
    if (color.isequal(vec3(0.0, 0.0, 0.0)) || color.isequal(vec3(1.0, 1.0, 1.0))){
        return color;
    }
    
    // convert to JzCzhz
    vec3 Jcolor = sourcegamut.linearRGBtoJzCzhz(color);
    vec2 colorCJ = vec2(Jcolor.y, Jcolor.x); // chroma is x; luma is y
    vec3 Joutput = Jcolor;
    
    // find the index for the hue angle and projection of the destination cusp back to neutral gray
    double ceilweight;
    int floorhueindex = hueToFloorIndex(Jcolor.z, ceilweight);
    int ceilhueindex = floorhueindex + 1;
    if (ceilhueindex == HUE_STEPS){
        ceilhueindex = 0;
    }
    // for gcusp, take a weighted average from the two nearest hues that were sampled
    // for hlpcm, take the input's luma
    double maptoluma;
    if (mapdirection == MAP_GCUSP){
        double floormaptoluma = destgamut.cusplumalist[floorhueindex];
        double ceilmaptoluma = destgamut.cusplumalist[ceilhueindex];
        maptoluma = ((1.0 - ceilweight) * floormaptoluma) + (ceilweight * ceilmaptoluma);
    }
    else if (mapdirection == MAP_HLPCM){
        maptoluma = Jcolor.x;
    }
    else {
        printf("WTF ERROR!\n");
    }
    vec2 maptopoint = vec2(0.0, maptoluma);
    
    // find the boundaries
    vec3 sourceboundary3D = sourcegamut.getBoundary3D(Jcolor, maptoluma, floorhueindex);
    vec2 sourceboundary = vec2(sourceboundary3D.y, sourceboundary3D.x); // chroma is x; luma is y
    vec3 destboundary3D = destgamut.getBoundary3D(Jcolor, maptoluma, floorhueindex);
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
    
    // the destination gamut is smaller than the source gamut in the color's direction, so compression might be needed
    if (distdest < distsource){
        
        double outofboundszone = distsource - distdest;
        double remapzone = outofboundszone * remapfactor;
        double kneepoint = distdest - remapzone;
        double altknee = distdest * remaplimit;
        if ((altknee > kneepoint) || (safezonetype == RMZONE_DEST_BASED)){
            kneepoint = altknee;
            remapzone = distdest - kneepoint;
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
        
        
        
        //printf("out of bounds %f, remapzone %f, kneepoint %f, kneewidth %f, safezonebound %f\n", outofboundszone, remapzone, kneepoint, kneewidth, safezonebound);
        
        // compression is only needed if we are beyond the safety zone
        if (distcolor > safezonebound){
            //printf("hey! I'm doing a remap! Without remap we'd have:\n");
            //vec3 info = destgamut.JzCzhzToLinearRGB(Joutput);
            //info.printout();
            
            double kneetop = (softknee) ? kneepoint + halfkneewidth : kneepoint;
            double slope = remapzone / (remapzone + outofboundszone);
            
            /*
            soft knee formula: https://dsp.stackexchange.com/questions/28548/differences-between-soft-knee-and-hard-knee-in-dynamic-range-compression-drc
            T = threshhold
            W = knee width (half above and half below T)
            S = slope of compressed zone
            y = x IF x < T – W/2
            y = x + ((S-1)*((x – T + W/2)^2))/2W IF T – W/2 <= x <= T + W/2
            y = T + (x-T)*S IF x > T + W/2
            */
            
            double newdist;
            
            // above the soft knee zone
            if ((distcolor > kneetop) || !softknee){
                //printf("linear section\n");
                newdist = kneepoint + ((distcolor - kneepoint) * slope);
            }
            // inside the soft knee zone
            else {
                //printf("softknee section\n");
                newdist = distcolor + ((( slope - 1.0) * pow(distcolor - kneepoint - halfkneewidth, 2.0)) / (2.0 * kneewidth));
            }
            //printf("newdist is %f\n", newdist);
            
            // remap
            vec2 colordir = cuspprojtocolor.normalizedcopy();
            vec2 newcolor = maptopoint + (colordir * newdist);
            //printf("cusp projection %f, %f, old color CJ %f, %f, new color CJ, %f, %f\n", maptopoint.x, maptopoint.y, colorCJ.x, colorCJ.y, newcolor.x, newcolor.y);
            Joutput.x = newcolor.y; // luma is J
            Joutput.y = newcolor.x; // chroma is C
            //printf("JCh changed from %f, %f, %f to %f, %f, %f\n", Jcolor.x, Jcolor.y, Jcolor.z, Joutput.x, Joutput.y, Joutput.z);
        }

    }
    // the destination gamut is larger, so only do something if expansion is asked for
    else if ((distdest > distsource) && expand){
        // apply the inverse of the compression function
        
        // note that the set-up computations are different than above because source and dest have flip-flopped 
        double outofboundszone = distdest - distsource;
        double remapzone = outofboundszone * remapfactor;
        double kneepoint = distsource - remapzone;
        double altknee = distsource * remaplimit;
        if ((altknee > kneepoint) || (safezonetype == RMZONE_DEST_BASED)){
            kneepoint = altknee;
            remapzone = distsource - kneepoint;
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
        
        
        if (distcolor > safezonebound){
        
            double kneetop = (softknee) ? kneepoint + halfkneewidth : kneepoint;
            double slope = remapzone / (remapzone + outofboundszone);
            
            double kneetopex = kneepoint;
            if (softknee){
                // the breakpoint for the inverse function is the y value of the original function evaluated at the original breakpoint
                kneetopex = kneepoint + ((kneetop - kneepoint) * slope);
            }
            
            double newdist;
            
            // above the soft knee
            if ((distcolor > kneetopex) || !softknee){
                newdist = ((distcolor - kneepoint) / slope) + kneepoint;
            }
            // inside the soft knee zone
            else {
                // wow is this fugly...
                double term1 = ((2.0 * slope * kneepoint * kneewidth) + (slope * pow(kneewidth, 2.0)) - (2.0 * kneepoint * kneewidth) - pow(kneewidth, 2.0) - 2.0) / (2.0 * (slope - 1.0) * kneewidth);
                double term2 = sqrt((-2.0 * slope * kneepoint * kneewidth) - (slope * pow(kneewidth, 2.0)) + (2.0 * slope * kneewidth * distcolor) + (2.0 * kneepoint * kneewidth) + pow(kneewidth, 2.0) - (2.0 * kneewidth * distcolor) + 1.0);
                double term3 = ((slope - 1.0) * kneewidth);
                double pluscandidate = term1 + (term2 / term3);
                double minuscandidate = term1 + ((-1.0 * term2) / term3);
                // one of these should be wildly wrong; use the onse that's closer to the input
                double plusdist = fabs(pluscandidate - distcolor);
                double minusdist = fabs(minuscandidate - distcolor);
                newdist = (plusdist < minusdist) ? pluscandidate : minuscandidate;
            }
            // remap
            vec2 colordir = cuspprojtocolor.normalizedcopy();
            vec2 newcolor = maptopoint + (colordir * newdist);
            //printf("cusp projection %f, %f, old color CJ %f, %f, new color CJ, %f, %f\n", maptopoint.x, maptopoint.y, colorCJ.x, colorCJ.y, newcolor.x, newcolor.y);
            Joutput.x = newcolor.y; // luma is J
            Joutput.y = newcolor.x; // chroma is C
            //printf("JCh changed from %f, %f, %f to %f, %f, %f\n", Jcolor.x, Jcolor.y, Jcolor.z, Joutput.x, Joutput.y, Joutput.z);
        }
    }
    
    
    
    return destgamut.JzCzhzToLinearRGB(Joutput);
}
