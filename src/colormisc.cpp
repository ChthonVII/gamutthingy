#include "colormisc.h"

#include <math.h>
#include <numbers>


// clamp a double between 0.0 and 1.0
double clampdouble(double input){
    if (input < 0.0) return 0.0;
    if (input > 1.0) return 1.0;
    return input;
}

// convert a 0-1 double value to 0-255 png_byte value with Martin Roberts' quasirandom dithering
// see: https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
// Aside from being just beautifully elegant, this dithering method is also perfect for our use case,
// in which our input might be might be a "swizzled" texture.
// These pose a problem for many other dithering methods.
// Any kind of error diffusion will diffuse error across swizzled tile boundaries.
// Even plain old ordered dithering is locally wrong where swizzled tile boundaries
// often don't line up with the Bayer matrix boundaries.
// But quasirandom doesn't care where you cut and splice it; it's still balanced.
// (Blue noise would work too, but that's a huge amount of overhead, while this is a very short function.)
png_byte quasirandomdither(double input, int x, int y){
    x++; // avoid x=0
    y++; // avoid y=0
    double dummy;
    double dither = modf(((double)x * 0.7548776662) + ((double)y * 0.56984029), &dummy);
    if (dither < 0.5){
        dither = 2.0 * dither;
    }
    else if (dither > 0.5) {
        dither = 2.0 - (2.0 * dither);
    }
    // if we ever get exactly 0.5, don't touch it; otherwise we might end up adding 1.0 to a black that means transparency.
    int output = (int)((input * 255.0) + dither);
    if (output > 255) output = 255;
    if (output < 0) output = 0;
    return (png_byte)output;
}

// return to RGB8 with just rounding
png_byte toRGB8nodither(double input){
    int output = (int)((input * 255.0) + 0.5);
    if (output > 255) output = 255;
    if (output < 0) output = 0;
    return (png_byte)output;
}

// sRGB gamma functions
double togamma(double input){
    if (input <= 0.0031308){
        return clampdouble(input * 12.92);
    }
    return clampdouble((1.055 * pow(input, (1.0/2.4))) - 0.055);
}
double tolinear(double input){
    if (input <= 0.04045){
        return clampdouble(input / 12.92);
    }
    return clampdouble(pow((input + 0.055) / 1.055, 2.4));
}

// Calculate angleA minus angleB assuming both are in range 0 to 2pi radians
// Answer will be in range -pi to +pi radians.
double AngleDiff(double angleA, double angleB){
    
    // skip the easy case
    if (angleA == angleB){
        return 0.0;
    }
    
    // sanitize input
    while (angleA > (2.0 * std::numbers::pi_v<long double>)){
        angleA -= (2.0 * std::numbers::pi_v<long double>);
    }
    while (angleA < 0.0){
        angleA += (2.0 * std::numbers::pi_v<long double>);
    }
    while (angleB > (2.0 * std::numbers::pi_v<long double>)){
        angleB -= (2.0 * std::numbers::pi_v<long double>);
    }
    while (angleB < 0.0){
        angleB += (2.0 * std::numbers::pi_v<long double>);
    }
    
    //subtract
    double output = angleA - angleB;
    
    // if difference is greater than 180 degrees, take the exemplary angle instead
    double outabs = fabs(output);
    if (outabs > std::numbers::pi_v<long double>){
        bool isneg = output <= 0.0;
        output = (2.0 * std::numbers::pi_v<long double>) - outabs;
        // flip the sign
        if (!isneg){
            output *= -1.0;
        }
    }
    
    return output;
}
