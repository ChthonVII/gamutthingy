#ifndef GAMUTBOUNDS_H
#define GAMUTBOUNDS_H

#include "vec2.h"
#include "vec3.h"

//#include <stdlib.h>
//#include <string.h>
#include <vector>
#include <string>

#define HUE_STEPS 1800 // 0.2 degrees
#define LUMA_STEPS 20 // 5%
#define FINE_LUMA_STEPS 50 // 0.1% 
#define CHROMA_STEPS 50 // 2%
#define FINE_CHROMA_STEPS 20 // 0.1%

class boundarypoint{
public:
    double x;
    double y;
    double angle;
    bool iscusp;
};

class gridpoint{
public:
    double x;
    double y;
    double angle;
    bool inbounds;
    bool checkright;
    bool checkdown;
};

class gamutdescriptor{
public:
    int verbosemode;
    bool issourcegamut;
    bool needschromaticadapt;
    vec3 whitepoint;
    vec3 otherwhitepoint;
    vec3 redpoint;
    vec3 greenpoint;
    vec3 bluepoint;
    double matrixP[3][3];
    double inverseMatrixP[3][3];
    double matrixC[3][3];
    double matrixNPM[3][3];
    double inverseMatrixNPM[3][3];
    vec3 matrixW;
    vec3 destMatrixW;
    vec3 normalizationFactors;
    double matrixMtoD65[3][3];
    double matrixNPMadaptToD65[3][3];
    double inverseMatrixNPMadaptToD65[3][3];
    std::string gamutname;
    std::vector<boundarypoint> data[HUE_STEPS];
    double cusplumalist[HUE_STEPS];
    
    bool initialize(std::string name, vec3 wp, vec3 rp, vec3 gp, vec3 bp, bool issource, vec3 dwp, int verbose);
    void reservespace();
    void initializeMatrixP();
    bool initializeInverseMatrixP();
    void initializeMatrixW();
    void initializeNormalizationFactors();
    void initializeMatrixC();
    void initializeMatrixNPM();
    bool initializeInverseMatrixNPM();
    bool initializeChromaticAdaptationToD65();
    void FindBoundaries();
    void ProcessSlice(int huestep, double maxluma, double maxchroma);
    
    
    vec3 linearRGBtoXYZ(vec3 input);
    vec3 XYZtoLinearRGB(vec3 input);
    vec3 linearRGBtoJzCzhz(vec3 input);
    vec3 JzCzhzToLinearRGB(vec3 input);
    
    vec3 linearRGBtoLCh(vec3 input);
    // TODO: LChtoLinearRGB
    
    vec2 getBoundary2D(vec2 color, double focalpointluma, int hueindex);
    vec3 getBoundary3D(vec3 color, double focalpointluma, int hueindex);
    
};

int hueToFloorIndex(double hue, double &excess);

vec3 mapColor(vec3 color, gamutdescriptor sourcegamut, gamutdescriptor destgamut, bool expand, double remapfactor, double remaplimit, bool softknee, double kneefactor, int mapdirection, int safezonetype);


#endif
