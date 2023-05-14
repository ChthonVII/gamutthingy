#ifndef COLORMISC_H
#define COLORMISC_H

//#include "../../png.h"
#include <png.h> // Linux should have libpng-dev installed; Windows users can figure stuff out.

double clampdouble(double input);

png_byte quasirandomdither(double input, int x, int y);

png_byte toRGB8nodither(double input);

double togamma(double input);

double tolinear(double input);


#endif
