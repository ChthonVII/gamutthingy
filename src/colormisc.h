#ifndef COLORMISC_H
#define COLORMISC_H

//#include "../../png.h"
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


#endif
