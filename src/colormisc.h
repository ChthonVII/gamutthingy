#ifndef COLORMISC_H
#define COLORMISC_H

#include "vec3.h"
#include "vec2.h"

#include <png.h> // Linux should have libpng-dev installed; Windows users can figure stuff out.


// clamp a double between 0.0 and 1.0
double clampdouble(double input);

// convert a 0-1 double value to 0-255 png_byte value with Martin Roberts' quasirandom dithering
png_byte quasirandomdither(double input, int x, int y);

// return to RGB8 with just rounding
png_byte toRGB8nodither(double input);

// sRGB gamma functions
double togamma(double input);
double tolinear(double input);

// rec2084 gamma functions
double rec2084togamma(double input, double maxnits);
double rec2084tolinear(double input, double maxnits);

// Calculate angleA minus angleB assuming both are in range 0 to 2pi radians
// Answer will be in range -pi to +pi radians.
double AngleDiff(double angleA, double angleB);

// Add two angles, wrapping output to range 0 to 2pi radians
double AngleAdd(double angleA, double angleB);

// if input is <= floor returns 0
// if input is >= ceiling, returns 1
// otherwise returns position on 01 hermite cubic spline between floor and ceiling 
double cubichermitemap(double floor, double ceiling, double input);

// inverse of cubichermitemap()
// if input is <= floor returns floor
// if input is >= ceiling, returns ceiling
// otherwise returns position on 01 inverse cubic hermite spline between floor and ceiling 
double inversecubichermitemap(double floor, double ceiling, double input);

// if input is <= floor returns 0
// if input is >= ceiling, returns 1
// otherwise returns position relative to floor and ceiling taken to power
// to avoid an ugly elbow, consider using floor=0 with power > 1, or ceiling=1 with power <1
double powermap(double floor, double ceiling, double input, double power);

// inverse of powermap()
// if input is <= floor returns floor
// if input is >= ceiling, returns ceiling
// otherwise returns position relative to floor and ceiling taken to inverse power
double inversepowermap(double floor, double ceiling, double input, double power);

//Compute the inverse of the 01 hermite cubic spline for domain = range = 0-1
double inversehermite(double input);

// Compute xy coordinates from CCT
vec3 xycoordfromfromCCT(double cct, int locus, double mpcd, int mpcdtype);
// subroutines for various loci
vec3 xycoordfromfromCCTdaylight(double cct);
vec3 xycoordfromfromCCTdaylightdogway(double cct);
vec3 xycoordfromfromCCTplankian(double cct);

// convert CIE1931 xy coords to CIE1960 uv coords and back
// (used for CCT+MPCD calculations)
vec2 xytocie1960uv(vec2 input);
vec2 cie1960uvtoxy(vec2 input);

// convert CIE1931 xy coords to Judd1935 coords and back
// (used for CCT+MPCD calculations)
vec2 xytojuddxy(vec2 input);
vec2 juddxytoxy(vec2 input);

// convert CIE1931 xy coords to MacAdam's xy-uv transformation equivalent to Judd1935 UCS and back
vec2 xytojuddmacadamuv(vec2 input);
vec2 juddmacadamuvtoxy(vec2 input);

// Convert integer value 0 through (steps-1) to floating point value 0.0 to 1.0
// Uses "middle of the bin," plus a parabolic fudge factor to pull 0 to 0.0 and (steps-1) to 1.0,
// with less impact on values in the middle of the scale than just dividing by (steps-1).
// (The common practice of dividing by (steps-1) is equivalent to using a linear fudge factor.)
double BetterDAC(unsigned int input, unsigned int steps);
// Convert floating point value 0.0 to 1.0 to integer value 0 through (steps-1)
// Inverts BetterDAC()
unsigned int BetterADC(double input, unsigned int steps);

#endif
