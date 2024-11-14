#ifndef CRTEMULATION_H
#define CRTEMULATION_H

#include "vec3.h"
#include "constants.h"

// YIQ scaling factors
// moved to constants.h
//#define Udownscale 0.492111
//#define Vdownscale 0.877283
//#define Uupscale (1.0/Udownscale)
//#define Vupscale (1.0/Vdownscale)

class crtdescriptor{
public:
    int verbosity;
    
    // NTSC1953 white balance factors
    double ntsc1953_wr;
    double ntsc1953_wg;
    double ntsc1953_wb;
    
    // some variables for the BT.1886 EOTF that we can initialize once and then not have to recompute.
    double CRT_EOTF_blacklevel;
    double CRT_EOTF_whitelevel;
    double CRT_EOTF_b;
    double CRT_EOTF_k;
    double CRT_EOTF_s;
    double CRT_EOTF_i;
    
    int modulatorindex;
    double modulatorMatrix[3][3];
    int demodulatorindex;
    double demodulatorMatrix[3][3];
    int demodulatorrenormalization;
    double overallMatrix[3][3];
    double inverseOverallMatrix[3][3];

    // variables for clamping
    /*
    double redclamphighfactor = 1.0;
    double greenclamphighfactor = 1.0;
    double blueclamphighfactor = 1.0;
    double redclamplowfactor = 1.0;
    double greenclamplowfactor = 1.0;
    double blueclamplowfactor = 1.0;
    */

    // blacklevel is CRT luminosity in cd/m^2 given black input, divided by 100 (sane value 0.001)
    // whitelevel is CRT luminosity in cd/m^2 given white input, divided by 100 (sane value 1.0)
    bool Initialize(double blacklevel, double whitelevel, int modulatorindex_in, int demodulatorindex_in, int renorm, int verbositylevel);
    
    // The EOTF function from BT.1886 Appendix 1 for approximating the behavior of CRT televisions.
    // The function from Appendix 1 is more faithful than the fairly useless Annex 1 function, which is just 2.4 gamma
    // The function has been modified to handle negative inputs in the same fashion as IEC 61966-2-4 (a.k.a. xvYCC)
    // (BT.1361 does something similar.)
    // Dynamic range is restored in a post-processing step that chops off the black lift and then normalizes to 0-1.
    // Initialize1886EOTF() must be run once before tolinear1886appx1() and togamma1886appx1() can be used.
    void Initialize1886EOTF();
    // Brute force the value of "b" for the BT.1886 Appendix 1 EOTF function using binary search.
    void BruteForce1886B();
    double tolinear1886appx1(double input);
    double togamma1886appx1(double input);
    vec3 tolinear1886appx1vec3(vec3 input);
    vec3 togamma1886appx1vec3(vec3 input);
    
    bool InitializeNTSC1953WhiteBalanceFactors();
    bool InitializeModulator();
    void InitializeDemodulator();
    
    vec3 CRTEmulateGammaSpaceRGBtoLinearRGB(vec3 input);
    vec3 CRTEmulateLinearRGBtoGammaSpaceRGB(vec3 input);
};















#endif
