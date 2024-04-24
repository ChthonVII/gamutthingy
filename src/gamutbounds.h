#ifndef GAMUTBOUNDS_H
#define GAMUTBOUNDS_H

#include "vec2.h"
#include "vec3.h"

#include <vector>
#include <string>

#define HUE_STEPS 1800 // 0.2 degrees
#define LUMA_STEPS 30 // 3.333...% Formerly was 20 // 5% but needed to be bigger; sometimes cusp was above the first coarse point
#define FINE_LUMA_STEPS 50 // 0.0666...% 
#define CHROMA_STEPS 50 // 2%
#define FINE_CHROMA_STEPS 20 // 0.1%

#define BOUND_NORMAL 0
#define BOUND_ABOVE 1
#define BOUND_BELOW 2

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

class warprange{
public:
    int index;
    float floor;
    float ceiling;
};

class gamutdescriptor{
public:
    int verbosemode;
    bool issourcegamut;
    bool needschromaticadapt;
    int CATtype;
    vec3 whitepoint;
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
    double cuspchromalist[HUE_STEPS];
    vec2 fakepoints[HUE_STEPS];
    vec2 ufakepoints[HUE_STEPS];
    bool rotationneeded[HUE_STEPS];
    int impingingslicecount[HUE_STEPS];
    std::vector<warprange> impingingslices[HUE_STEPS];
    warprange selfwarp[HUE_STEPS];
    double matrixChunghwa[3][3];
    
    // Spiral CARMISA needs a bunch of variables precomputed
    // primary and secondary color JzCzhz coordinates
    vec3 polarredpoint;
    vec3 polargreenpoint;
    vec3 polarbluepoint;
    vec3 polarcyanpoint;
    vec3 polarmagentapoint;
    vec3 polaryellowpoint;
    // adjusted primary and secondary color JzCzhz coordinates
    // (for two-step conversions if first step doesn't put its source primaries/secondaries on the destination primaries/secondaries)
    vec3 adjpolarredpoint;
    vec3 adjpolargreenpoint;
    vec3 adjpolarbluepoint;
    vec3 adjpolarcyanpoint;
    vec3 adjpolarmagentapoint;
    vec3 adjpolaryellowpoint;
    // max rotations for each primary/secondary
    double redrotation;
    double greenrotation;
    double bluerotation;
    double yellowrotation;
    double magentarotation;
    double cyanrotation;
    // distances between adjacent primaries and secondaries
    double redtoyellowpolardist;
    double yellowtogreenpolardist;
    double greentocyanpolardist;
    double cyantobluepolardist;
    double bluetomagentapolardist;
    double magentatoredpolardist;
    // differences in max rotations between adjacent primaries and secondaries
    double redtoyellowrotatediff;
    double yellowtogreenrotatediff;
    double greentocyanrotatediff;
    double cyantobluerotatediff;
    double bluetomagentarotatediff;
    double magentatoredrotatediff;
    // scaling parameters
    double spiralcarismafloor;
    double spiralcarismaceiling;
    double spiralcharismaexponent;
    int spiralcarismascalemode;
    
    bool initialize(std::string name, vec3 wp, vec3 rp, vec3 gp, vec3 bp, vec3 other_wp, bool issource, int verbose, int cattype, bool compressenabled);
    // resizes vectors ahead of time
    void reservespace();
    void initializeMatrixP();
    bool initializeInverseMatrixP();
    void initializeMatrixW();
    void initializeNormalizationFactors();
    void initializeMatrixC();
    void initializeMatrixNPM();
    bool initializeInverseMatrixNPM();
    bool initializeChromaticAdaptationToD65();
    bool initializePolarPrimaries(bool dosc, double scfloor, double scceil, double scexp, int scmode, int verbose);
    void initializeMatrixChunghwa(gamutdescriptor &othergamut);
    // checks if the supplied JzCzhz color is within this gamut.
    // if not, also sets errorsize to the sum of linear rgb over/underruns.
    // (or sets errorsize to 10k if JzCzhzToLinearRGB() encounters a NaN error)
    bool IsJzCzhzInBounds(vec3 color, double &errorsize);
    // Populates the gamut boundary descriptor using the algorithm from
    // Lihao, Xu, Chunzhi, Xu, & Luo, Ming Ronnier. "Accurate gamut boundary descriptor for displays." *Optics Express*, Vol. 30, No. 2, pp. 1615-1626. January 2022. (https://opg.optica.org/fulltext.cfm?rwjcode=oe&uri=oe-30-2-1615&id=466694)
    void FindBoundaries();
    // Samples the gamut boundaries for one hue slice 
    void ProcessSlice(int huestep, double maxluma, double maxchroma);
    
    // Precomputes which slices will rotate into which other slices over which chroma ranges under spiral carisma,
    // effectively creating a new "warped" gamut boundary.
    void WarpBoundaries();
    
    vec3 linearRGBtoXYZ(vec3 input);
    vec3 XYZtoLinearRGB(vec3 input);
    vec3 linearRGBtoJzCzhz(vec3 input);
    vec3 JzCzhzToLinearRGB(vec3 input);
    
    //vec3 linearRGBtoLCh(vec3 input);
    // TODO: LChtoLinearRGB
    
    // Finds the point where the line from the focal point (chroma 0, luma = focalpointluma) to color intercepts the gamut boundary in th 2D hue splice specified by hueindex.
    // boundtype is used for the VP gamut mapping algorithm 
    vec2 getBoundary2D(vec2 color, double focalpointluma, int hueindex, int boundtype);
    // Finds the point where the line from the focal point (chroma 0, luma = focalpointluma, hue = color's hue) to color intercepts the gamut boundary.
    // hueindex is the index of the adjacent sampled hue splice below color's hue. (This was computed before, so it's passed for efficiency's sake) 
    // boundtype is used for the VP gamut mapping algorithm
    vec3 getBoundary3D(vec3 color, double focalpointluma, int hueindex, int boundtype, bool dospiralcarisma);
    
    // This function is dead. It belonged to an attempted fix for VP's step3 issues that didn't work out well
    // Returns a vector representing the direction from the cusp to the just-above-the-cusp boundary node at the given hue.
    // hueindexA and hueindexB are the indices for the adjacent sampled hue slices
    //vec2 getLACSlope(int hueindexA, int hueindexB, double hue);
    
    // Given (linear) RGB input, returns Pr, Pg, or Pb with the largest absolute value.
    // Effectively "how close are we to a primary/secondary color?"
    // This is a terrible way to do this that is only being implemented to maybe sorta kinda mimic color correction circuits in old TVs.
    // Some of which used (sometimes gamma corrected) Pb Pr to differentiate areas to apply different corrections.
    double linearRGBfindmaxP(vec3 input);
    
    // given the x and y coordinates of a point in xyY space,
    // find a Y such that the corresponding linear RGB is 1.0 for lockcolor (LOCKRED, LOCKGREEN, or LOCKBLUE).
    // output the linear RGB triplet and set Y to the Y value
    // (this function is used by color correction circuits)
    vec3 xyYhillclimb(double x, double y, int lockcolor, double &Y);
    
    // Finds the rotation (in radians) to be applied to each primary/secondary color.
    // If the primary/secondary color is representable in the destination gamut, then 0.
    // If the primary/secondary color is not representable in the destination gamut...
    // ...and if its luma and chroma at the destination gamut's primary's/secondary's hue angle would be in bounds,
    //          or would be out of bounds, but less far,
    //          then the primary's/secondary's hue angle minus 
    // ...but if the rotated color would be at least equally far out of bounds, then 0.
    void FindPrimaryRotations(gamutdescriptor &othergamut, double maxscale, int verbose);
    
    // finds the max rotation in radians for a given hue
    double FindHueMaxRotation(double hue);
    
    // finds spiral carisma rotation in radians for a given JzCzhz color
    double FindHueRotation(vec3 input);
};

// returns the index of the adjacent sampled hue slice "below" hue,
// and stores how far hue is towards the next slice (on a 0 to 1 scale) to excess
int hueToFloorIndex(double hue, double &excess);

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
vec3 mapColor(vec3 color, gamutdescriptor &sourcegamut, gamutdescriptor &destgamut, bool expand, double remapfactor, double remaplimit, bool softknee, double kneefactor, int mapdirection, int safezonetype, bool dospiralcarisma);

// Scales the distance to a color according to parameters
// distcolor: distance from the focal point to the color
// distsource: distance from the focal point to the source gamut boundary
// distdest: distance from the focal point to the destination gamut boundary
// expand, rempafactor, rempalimit, softknee, kneefactor, safezonetype: same as for mapColor()
// changed: Whether a change was made is saved here. (No change if the color is in the safe zone.)
//      If no change, then the result should be discarded to reduce floating point errors
double scaledistance(bool &changed, double distcolor, double distsource, double distdest, bool expand, double remapfactor, double remaplimit, bool softknee, double kneefactor, int safezonetype);

#endif
