#include "colormisc.h"
#include "constants.h"

#include <math.h>
#include <numbers>
#include <complex>
#include <stdio.h>

// constants for inverse hermite
const std::complex<double>two_pi_i = 2.0 * std::numbers::pi_v<double> * std::complex<double>(0.0,1.0);
const std::complex<double>cuberootrotate = exp(two_pi_i / 3.0);

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

// rec2084 gamma functions
// See https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/ColorSpaceUtility.hlsli#L75
// maxnits should be "the brightness level that SDR 'white' is rendered at within an HDR monitor" (probably 100-200ish)
// Google Chrome uses a default of 200 if autodetection fails.
double rec2084togamma(double input, double maxnits){

    input = clampdouble(input);

    const double m1 = 1305.0/8192.0;
    const double m2 = 2523.0/32.0;
    const double c1 = 107.0/128.0;
    const double c2 = 2413.0/128.0;
    const double c3 = 2392.0/128.0;

    const double Ym1 = pow(input * (maxnits / 10000.0), m1);

    double output = pow((c1 + (c2 * Ym1)) / (1.0 + (c3 * Ym1)), m2);

    output = clampdouble(output);

    return output;
}
double rec2084tolinear(double input, double maxnits){

    input = clampdouble(input);

    const double m1 = 1305.0/8192.0;
    const double m2 = 2523.0/32.0;
    const double c1 = 107.0/128.0;
    const double c2 = 2413.0/128.0;
    const double c3 = 2392.0/128.0;

    const double E1overm2 = pow(input, 1.0 / m2);
    double top = E1overm2 - c1;
    if (top < 0.0){ top = 0.0; }
    double output = pow(top / (c2 - (c3 * E1overm2)), 1.0 / m1);
    output *= (10000.0 / maxnits);

    output = clampdouble(output);

    return output;
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

// Add two angles, wrapping output to range 0 to 2pi radians
double AngleAdd(double angleA, double angleB){
    double output = angleA + angleB;
    while (output > (2.0 * std::numbers::pi_v<long double>)){
        output -= (2.0 * std::numbers::pi_v<long double>);
    }
    while (output < 0.0){
        output += (2.0 * std::numbers::pi_v<long double>);
    }
    return output;
}

// if input is <= floor returns 0
// if input is >= ceiling, returns 1
// otherwise returns position on 01 cubic hermite spline between floor and ceiling 
double cubichermitemap(double floor, double ceiling, double input){
    // sanitize input
    if (floor < 0.0){
        floor = 0.0;
    }
    if (ceiling > 1.0){
        ceiling = 1.0;
    }
    if (floor > ceiling){
        ceiling = floor;
    }
    if (input < 0.0){
        input = 0.0;
    }
    if (input > 1.0){
        input = 1.0;
    }
    
    // easy cases
    if (input <= floor){
        return 0.0;
    }
    if (input >= ceiling){
        return 1.0;
    }
    
    // shift & scale position relative to floor & ceiling to 0 to 1
    double scaledinput = (input - floor) / (ceiling - floor);
    // 01 hermite is -2t^3 + 3t^2
    double hermitevalue = (-2.0 * scaledinput * scaledinput * scaledinput) + (3 * scaledinput * scaledinput);
    
    return hermitevalue;
}

// inverse of cubichermitemap()
// if input is <= floor returns floor
// if input is >= ceiling, returns ceiling
// otherwise returns position on 01 inverse cubic hermite spline between floor and ceiling 
double inversecubichermitemap(double floor, double ceiling, double input){
    // sanitize input
    if (floor < 0.0){
        floor = 0.0;
    }
    if (ceiling > 1.0){
        ceiling = 1.0;
    }
    if (floor > ceiling){
        ceiling = floor;
    }
    if (input < 0.0){
        input = 0.0;
    }
    if (input > 1.0){
        input = 1.0;
    }
    
    // easy cases
    if (input <= floor){
        return floor;
    }
    if (input >= ceiling){
        return ceiling;
    }
    
    double hermitevalue = inversehermite(input);
    
    // shift & scale position relative to floor & ceiling to 0 to 1
    double scaledoutput = ((ceiling - floor) * hermitevalue) + floor;
    
    return scaledoutput;
}


// if input is <= floor returns 0
// if input is >= ceiling, returns 1
// otherwise returns position relative to floor and ceiling taken to power
// to avoid an ugly elbow, consider using floor=0 with power > 1, or ceiling=1 with power <1
double powermap(double floor, double ceiling, double input, double power){
    // sanitize input
    if (floor < 0.0){
        floor = 0.0;
    }
    if (ceiling > 1.0){
        ceiling = 1.0;
    }
    if (floor > ceiling){
        ceiling = floor;
    }
    if (input < 0.0){
        input = 0.0;
    }
    if (input > 1.0){
        input = 1.0;
    }
    if (power < 0){
        power = 0;
    }
    
    // easy cases
    if (input <= floor){
        return 0.0;
    }
    if (input >= ceiling){
        return 1.0;
    }
    
    // shift & scale position relative to floor & ceiling to 0 to 1
    double scaledinput = (input - floor) / (ceiling - floor);
    return pow(scaledinput, power);
}

// inverse of powermap()
// if input is <= floor returns floor
// if input is >= ceiling, returns ceiling
// otherwise returns position relative to floor and ceiling taken to inverse power
double inversepowermap(double floor, double ceiling, double input, double power){
    // sanitize input
    if (floor < 0.0){
        floor = 0.0;
    }
    if (ceiling > 1.0){
        ceiling = 1.0;
    }
    if (floor > ceiling){
        ceiling = floor;
    }
    if (input < 0.0){
        input = 0.0;
    }
    if (input > 1.0){
        input = 1.0;
    }
    if (power < 0){
        power = 0;
    }
    
    // easy cases
    if (input <= floor){
        return floor;
    }
    if (input >= ceiling){
        return ceiling;
    }
    
    double powvalue = pow(input, 1.0 / power);
    
    // shift & scale position relative to floor & ceiling to 0 to 1
    double scaledoutput = ((ceiling - floor) * powvalue) + floor;
    
    return scaledoutput;
}


// Compute the inverse of the 01 hermite cubic spline for domain = range = 0-1
// This turns out to be a miserable bitch because we have an intermediate term that's unavoidably a complex number,
// and then pow() picks the wrong cube root of that.
// here is the equation for the inverse: https://www.wolframalpha.com/input?i=inverse+-2x%5E3+%2B+3x%5E2+%3D+y
// 1/2 ((2 sqrt(x^2 - x) - 2 x + 1)^(1/3) + 1/(2 sqrt(x^2 - x) - 2 x + 1)^(1/3) + 1)
// Much credit to this stackoverflow answer: https://stackoverflow.com/questions/51397996/how-to-compute-a-cubic-root-of-a-complex-number-in-c
double inversehermite(double input){
    // todo: make these globals so we don't need to recompute each time function is called
    //const std::complex<double>two_pi_i = 2.0 * std::numbers::pi_v<double> * std::complex<double>(0.0,1.0);
    //const std::complex<double>cuberootrotate = exp(two_pi_i / 3.0);
    
    // make sure input is in range 0-1
    if (input > 1.0){
        input = 1.0;
    }
    if (input < 0.0){
        input = 0.0;
    }
    
    // this will always be a complex number
    std::complex<double> fml = sqrt(std::complex<double>((input * input) - input, 0.0));
    // add the other terms under the cube root
    fml = (2.0 * fml) - (2.0 * input) + 1.0;
    // the hitch here is that complex numbers have 3 cube roots, and pow() only gives us one of them.
    std::complex<double>bigterm = pow(fml, 1.0/3.0);
    // by trial and error, we can see that applying 2 rotations gets us answers in the range 0-1
    bigterm *= cuberootrotate * cuberootrotate;
    // finally apply the outer terms
    std::complex<double>coutput = 0.5 * (bigterm + (1.0/bigterm) + 1.0);
    // output only the real part
    // we could assert on the imaginary part being nonzero, but we wouldn't really have any way to recover from that.
    // and the function is testably correct for inputs in range 0-1.
    double output = std::real(coutput);
    // clean up floating point errors
    if (output > 1.0){
        output = 1.0;
    }
    if (output < 0.0){
        output = 0.0;
    }
    return output;
}

// Compute xy coordinates from CCT
// This approximation function is borrowed from grade: https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang
// Unfortunately, grade doesn't cite where it came from.
// This approximation function gives more accurate results for D65 than either function on wikipedia:
// https://en.wikipedia.org/wiki/Standard_illuminant#Computation
// https://en.wikipedia.org/wiki/Planckian_locus#Approximation
vec3 xycoordfromfromCCT(double cct){

    const double temp3 = 1000.0 / cct;
    const double temp6 = 1000000.0 / (cct * cct);
    const double temp9 = 1000000000.0 / (cct * cct * cct);

    double x;
    if (cct < 5500){
        x = 0.244058 + (0.0989971 * temp3) + (2.96545 * temp6) + (-4.59673 * temp9);
    }
    else if (cct < 8000){
        x = 0.200033 + (0.9545630 * temp3) + (-2.53169 * temp6) + (7.08578 * temp9);
    }
    else {
        x = 0.237045 + (0.2437440 * temp3) + (1.94062 * temp6) + (-2.11004 * temp9);
    }

    double y = -0.275275 + (2.87396 * x) - (3.02034 * x * x) + (0.0297408 * x * x * x);

    double z = 1.0 - x - y;

    return vec3(x, y, z);
}
