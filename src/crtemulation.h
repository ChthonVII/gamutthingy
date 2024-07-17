#ifndef CRTEMULATION_H
#define CRTEMULATION_H

// YIQ scaling factors
#define Udownscale 0.492111
#define Vdownscale 0.877283
#define Uupscale 1.0/Udownscale
#define Vupscale 1.0/Vdownscale

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
    
    bool Initialize(double blacklevel, double whitelevel, int pverbosity);
    
    void Initialize1886EOTF();
    void BruteForce1886B();
    double tolinear1886appx1(double input);
    double togamma1886appx1(double input);
    bool InitializeNTSC1953WhiteBalanceFactors();
    
};















#endif
