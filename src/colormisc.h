#ifndef COLORMISC_H
#define COLORMISC_H

#include "vec3.h"

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
vec3 xycoordfromfromCCT(double cct);

#endif
