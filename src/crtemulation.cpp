#include "crtemulation.h"
#include "constants.h"
#include "matrix.h"
#include "vec3.h"

#include <stdio.h>
#include <math.h>
#include <numbers>

bool crtdescriptor::Initialize(double blacklevel, double whitelevel, int demodulatorindex_in, int verbositylevel){
    bool output = true;
    verbosity = verbositylevel;
    CRT_EOTF_blacklevel = blacklevel;
    CRT_EOTF_whitelevel = whitelevel;
    Initialize1886EOTF();
    output = InitializeNTSC1953WhiteBalanceFactors();
    demodulatorindex = demodulatorindex_in;
    if (demodulatorindex != CRT_DEMODULATOR_NONE){
        InitializeDemodulator();
    }
    return output;
}

void crtdescriptor::Initialize1886EOTF(){
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing CRT EOTF emulation...\n");
    }
    
    if (CRT_EOTF_blacklevel < 0.0){
        printf("Negative luminosity is impossible. Setting CRT black level to 0.\n");
        CRT_EOTF_blacklevel = 0.0;
    }
    if (CRT_EOTF_whitelevel < 0.0){
        printf("Negative luminosity is impossible. Setting CRT white level to 0.\n");
        CRT_EOTF_whitelevel = 0.0;
    }
    if (CRT_EOTF_blacklevel >= CRT_EOTF_whitelevel){
        printf("Black brighter than white is impossible. Setting black and white levels to defaults.\n");
        CRT_EOTF_blacklevel = 0.001;
        CRT_EOTF_whitelevel = 1.0;
    }
    BruteForce1886B();
    CRT_EOTF_k = CRT_EOTF_whitelevel / pow(1.0 + CRT_EOTF_b, 2.6);
    CRT_EOTF_s = pow(0.35 + CRT_EOTF_b, -0.4);
    CRT_EOTF_i = CRT_EOTF_k * pow(0.35 + CRT_EOTF_b, 2.6);

    if (verbosity >= VERBOSITY_SLIGHT){
        printf("CRT emulation EOTF constants:\nblack level: %f\nwhite level: %f\nb: %.16f\nk: %.16f\ns: %.16f\ni: %.16f\n", CRT_EOTF_blacklevel, CRT_EOTF_whitelevel, CRT_EOTF_b, CRT_EOTF_k, CRT_EOTF_s, CRT_EOTF_i);
        
        //screen barf a bunch of test values to graph elsewhere and make sure it's right
        /*
        for (int i=0; i<=2000; i++){
            double testinput = i * 0.0005;
            double testoutput = tolinear1886appx1(testinput);
            printf("%f, %f\n", testinput, testoutput);
        }
        */
        /*
        printf("inverse EOTF tests:\n");
        double testinput = 0.0;
        double testoutput = tolinear1886appx1(testinput);
        double retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.25;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.5;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.75;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 1.0;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.35;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.3501;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.3499;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = -0.1;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        */
        
    }
    printf("\n----------\n\n");
    return;
}

// Brute force the value of "b" for the BT.1886 Appendix 1 EOTF function using binary search.
// blacklevel is CRT luminosity in cd/m^2 given black input, divided by 100 (sane value 0.001)
// whitelevel is CRT luminosity in cd/m^2 given white input, divided by 100 (sane value 1.0)
void crtdescriptor::BruteForce1886B(){
    
    if (CRT_EOTF_blacklevel == 0.0){
       CRT_EOTF_b = 0.0;
       return;
    }
    
    double floor = 0.0;
    double ceiling = 1.0;
    double guess;
    int iters = 0;
    
    while(true){
        guess = (floor + ceiling) * 0.5;
        double gresult = (CRT_EOTF_whitelevel / pow(1.0 + guess, 2.6)) * pow(0.35 + guess, -0.4) * pow(guess, 3.0);
        // exact hit!
        if (gresult == CRT_EOTF_blacklevel){
            break;
        }
        iters++;
        // should have converged by now
        if (iters > 100){
            break;
        }
        double error = fabs(gresult - CRT_EOTF_blacklevel);
        // wolfram alpha was giving 16 digits, so lets hope for the same
        if (error < 1e-16){
            break;
        }
        if (gresult > CRT_EOTF_blacklevel){
            ceiling = guess;
        }
        else {
            floor = guess;
        }
    }
    
    CRT_EOTF_b = guess;
    return;
}

// The EOTF function from BT.1886 Appendix 1 for approximating the behavior of CRT televisions.
// The function from Appendix 1 is more faithful than the fairly useless Annex 1 function, which is just 2.4 gamma
// The function has been modified to handle negative inputs in the same fashion as IEC 61966-2-4 (a.k.a. xvYCC)
// (BT.1361 does something similar.)
// Dynamic range is restored in a post-processing step that chops off the black lift and then normalizes to 0-1.
// Initialize1886EOTF() must be run once before this can be used.
double crtdescriptor::tolinear1886appx1(double input){
    
    // Shift input by b
    input += CRT_EOTF_b;
    
    // Handle negative input by flipping sign 
    bool flipsign = false;
    if (input < 0){
        input *= -1.0;
        flipsign = true;
    }
    
    // The main EOTF function
    double output;
    if (input < (0.35 + CRT_EOTF_b)){
        output = CRT_EOTF_k * CRT_EOTF_s * pow(input, 3.0);
    }
    else {
        output = CRT_EOTF_k * pow(input, 2.6);
    }
    
    // Flip sign again if input was negative
    if (flipsign){
        output *= -1.0;
    }
    
    // Chop off the black lift and normalize to 0-1
    output -= CRT_EOTF_blacklevel;
    output /= (CRT_EOTF_whitelevel - CRT_EOTF_blacklevel);
    
    // fix floating point errors very near 0 or 1
    if ((output != 0.0) && (fabs(output - 0.0) < 1e-6)){
        output = 0.0;
    }
    else if ((output != 1.0) && (fabs(output - 1.0) < 1e-6)){
        output = 1.0;
    }
    
    return output;
}

// inverse of tolinear1886appx1()
// Initialize1886EOTF() must be run once before this can be used.
double crtdescriptor::togamma1886appx1(double input){
    
    // undo the chop and normalization post-processing
    input *= (CRT_EOTF_whitelevel - CRT_EOTF_blacklevel);
    input += CRT_EOTF_blacklevel;
    
    // Handle negative input by flipping sign 
    bool flipsign = false;
    if (input < 0){
        input *= -1.0;
        flipsign = true;
    }
    
    // The main EOTF function
    double output;
    if (input < CRT_EOTF_i){
        output = pow((1.0/CRT_EOTF_k) * (1.0/CRT_EOTF_s) * input, 1.0/3.0);
    }
    else {
        output = pow((1.0/CRT_EOTF_k) * input, 1.0/2.6);
    }
    
    // Flip sign again if input was negative
    if (flipsign){
        output *= -1.0;
    }
    
    //unshift
    output -= CRT_EOTF_b;
    
    // fix floating point errors very near 0 or 1
    if ((output != 0.0) && (fabs(output - 0.0) < 1e-6)){
        output = 0.0;
    }
    else if ((output != 1.0) && (fabs(output - 1.0) < 1e-6)){
        output = 1.0;
    }
    
    return output;
    
}

// set the global variables for NTSC 1953 white balance
// This is basically copy/paste from the first few steps of gamut boundary initialization.
bool crtdescriptor::InitializeNTSC1953WhiteBalanceFactors(){
    
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing NTSC 1953 white balance factors for CRT de/modulator emulation...\n");
    }
    
    bool output = true;
    
    double matrixP[3][3] = {
        {gamutpoints[GAMUT_NTSC_1953][1][0], gamutpoints[GAMUT_NTSC_1953][2][0], gamutpoints[GAMUT_NTSC_1953][3][0]},
        {gamutpoints[GAMUT_NTSC_1953][1][1], gamutpoints[GAMUT_NTSC_1953][2][1], gamutpoints[GAMUT_NTSC_1953][3][1]},
        {gamutpoints[GAMUT_NTSC_1953][1][2], gamutpoints[GAMUT_NTSC_1953][2][2], gamutpoints[GAMUT_NTSC_1953][3][2]}
    };
    
    double inverseMatrixP[3][3];
    output = Invert3x3Matrix(matrixP, inverseMatrixP);
    if (!output){
        printf("Disaster! Matrix P is not invertible! Bad stuff will happen!\n");       
    }
    
    vec3 matrixW;
    matrixW.x = gamutpoints[GAMUT_NTSC_1953][0][0] / gamutpoints[GAMUT_NTSC_1953][0][1];
    matrixW.y = 1.0;
    matrixW.z = gamutpoints[GAMUT_NTSC_1953][0][2] / gamutpoints[GAMUT_NTSC_1953][0][1];
    
    vec3 normalizationFactors = multMatrixByColor(inverseMatrixP, matrixW);
    
    double matrixC[3][3] = {
        {normalizationFactors.x, 0.0, 0.0},
        {0.0, normalizationFactors.y, 0.0},
        {0.0, 0.0, normalizationFactors.z}
    };
    
    double matrixNPM[3][3];
    mult3x3Matrices(matrixP, matrixC, matrixNPM);
    
    ntsc1953_wr = matrixNPM[1][0];
    ntsc1953_wg = matrixNPM[1][1];
    ntsc1953_wb = matrixNPM[1][2];
    
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("wr: %.10f, wg: %.10f, wb: %.10f\n----------\n", ntsc1953_wr, ntsc1953_wg, ntsc1953_wb);
    }
    
    return output;
}

void crtdescriptor::InitializeDemodulator(){
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing CRT demodulator matrix...\n");
    }
    
    // convert angles to radians
    double redangle = demodulatorinfo[demodulatorindex][0][0] * (std::numbers::pi_v<long double>/ 180.0); 
    double greenangle = demodulatorinfo[demodulatorindex][0][1] * (std::numbers::pi_v<long double>/ 180.0); 
    double blueangle = demodulatorinfo[demodulatorindex][0][2] * (std::numbers::pi_v<long double>/ 180.0); 
    
    // gains
    double redgain = demodulatorinfo[demodulatorindex][1][0];
    double greengain = demodulatorinfo[demodulatorindex][1][1];
    double bluegain = demodulatorinfo[demodulatorindex][1][2];
    // if gains are normalized to blue, denormalize
    if (bluegain == 1.0){
        redgain *= Uupscale;
        greengain *= Uupscale;
        bluegain *= Uupscale;
    }
    
    //printf("angles: red %f, green %f, blue %f\ngains: red %f, green %f, blue %f\n", redangle, greenangle, blueangle, redgain, greengain, bluegain);
    
    // depolarize to get UV coordinates
    double redx = redgain * cos(redangle);
    double redy = redgain * sin(redangle);
    double greenx = greengain * cos(greenangle);
    double greeny = greengain * sin(greenangle);
    double bluex = bluegain * cos(blueangle);
    double bluey = bluegain * sin(blueangle);
    
    //printf("UV coords: red: x %f, y %f; green x %f, y %f; blue x %f y %f\n", redx, redy, greenx, greeny, bluex, bluey);
    
    // constant matrix
    const double matrixB[2][2] = {
        {(1.0 - ntsc1953_wr) / Vupscale, (-1.0 * ntsc1953_wr) / Uupscale},
        {(-1.0 * ntsc1953_wg) / Vupscale, (-1.0 * ntsc1953_wg) / Uupscale}
    };
    
    //printf("MatrixB: [[ %f, %f ], [ %f, %f ]]\n", matrixB[0][0], matrixB[0][1], matrixB[1][0], matrixB[1][1]);
    
    // multiply matrixB by {{y},{x}} and add  {{wr},{wg}} to recover Krr, Krg, Kgr, Kgg, Kbr, and Kbg
    // (multiply 2x2 matrix by 1x2 matrix, maybe implement a function for this if we wind up needing to do it elsewhere)
    double Krr = (matrixB[0][0] * redy) + (matrixB[0][1] * redx) + ntsc1953_wr;
    double Krg = (matrixB[1][0] * redy) + (matrixB[1][1] * redx) + ntsc1953_wg;
    double Kgr = (matrixB[0][0] * greeny) + (matrixB[0][1] * greenx) + ntsc1953_wr;
    double Kgg = (matrixB[1][0] * greeny) + (matrixB[1][1] * greenx) + ntsc1953_wg;
    double Kbr = (matrixB[0][0] * bluey) + (matrixB[0][1] * bluex) + ntsc1953_wr;
    double Kbg = (matrixB[1][0] * bluey) + (matrixB[1][1] * bluex) + ntsc1953_wg;
    // recover Krb, Kgb, and Kbb by virtue of matrixK's rows being normalized to 1.0
    double Krb = 1.0 - (Krr + Krg);
    double Kgb = 1.0 - (Kgr + Kgg);
    double Kbb = 1.0 - (Kbr + Kbg);
        
    // copy our correction matrix
    demodulatorMatrix[0][0] = Krr;
    demodulatorMatrix[0][1] = Krg;
    demodulatorMatrix[0][2] = Krb;
    demodulatorMatrix[1][0] = Kgr;
    demodulatorMatrix[1][1] = Kgg;
    demodulatorMatrix[1][2] = Kgb;
    demodulatorMatrix[2][0] = Kbr;
    demodulatorMatrix[2][1] = Kbg;
    demodulatorMatrix[2][2] = Kbb;
    
    // screen barf
    if (verbosity >= VERBOSITY_SLIGHT){
        print3x3matrix(demodulatorMatrix);
        printf("\n----------\n");
    }
    
    return;
}





