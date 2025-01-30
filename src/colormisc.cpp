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
// Either on daylight locus or on Plankian (black body) locus
vec3 xycoordfromfromCCT(double cct, int locus){


    // This approximation function is borrowed from grade: https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang
    // Unfortunately, grade doesn't cite where it came from.
    // This approximation function gives more accurate results for D65 than either function on wikipedia:
    // https://en.wikipedia.org/wiki/Standard_illuminant#Computation
    // https://en.wikipedia.org/wiki/Planckian_locus#Approximation
    if (locus == DAYLIGHTLOCUS){
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


    // black body calculation
    // borrowed from Bruce Lindbroom's view-source:http://www.brucelindbloom.com/javascript/ColorConv.js
    // With scientific constants updated to recent revisions

    /* 360nm to 830nm in 5nm increments */
    double CIE1931StdObs_x[95] = {
        0.000129900000, 0.000232100000, 0.000414900000, 0.000741600000, 0.001368000000, 0.002236000000,
        0.004243000000, 0.007650000000, 0.014310000000, 0.023190000000, 0.043510000000, 0.077630000000, 0.134380000000, 0.214770000000, 0.283900000000, 0.328500000000,
        0.348280000000, 0.348060000000, 0.336200000000, 0.318700000000, 0.290800000000, 0.251100000000, 0.195360000000, 0.142100000000, 0.095640000000, 0.057950010000,
        0.032010000000, 0.014700000000, 0.004900000000, 0.002400000000, 0.009300000000, 0.029100000000, 0.063270000000, 0.109600000000, 0.165500000000, 0.225749900000,
        0.290400000000, 0.359700000000, 0.433449900000, 0.512050100000, 0.594500000000, 0.678400000000, 0.762100000000, 0.842500000000, 0.916300000000, 0.978600000000,
        1.026300000000, 1.056700000000, 1.062200000000, 1.045600000000, 1.002600000000, 0.938400000000, 0.854449900000, 0.751400000000, 0.642400000000, 0.541900000000,
        0.447900000000, 0.360800000000, 0.283500000000, 0.218700000000, 0.164900000000, 0.121200000000, 0.087400000000, 0.063600000000, 0.046770000000, 0.032900000000,
        0.022700000000, 0.015840000000, 0.011359160000, 0.008110916000, 0.005790346000, 0.004109457000, 0.002899327000, 0.002049190000, 0.001439971000, 0.000999949300,
        0.000690078600, 0.000476021300, 0.000332301100, 0.000234826100, 0.000166150500, 0.000117413000, 0.000083075270, 0.000058706520, 0.000041509940, 0.000029353260,
        0.000020673830, 0.000014559770, 0.000010253980, 0.000007221456, 0.000005085868, 0.000003581652, 0.000002522525, 0.000001776509, 0.000001251141};
    double CIE1931StdObs_y[95] = {
        0.000003917000, 0.000006965000, 0.000012390000, 0.000022020000, 0.000039000000, 0.000064000000,
        0.000120000000, 0.000217000000, 0.000396000000, 0.000640000000, 0.001210000000, 0.002180000000, 0.004000000000, 0.007300000000, 0.011600000000, 0.016840000000,
        0.023000000000, 0.029800000000, 0.038000000000, 0.048000000000, 0.060000000000, 0.073900000000, 0.090980000000, 0.112600000000, 0.139020000000, 0.169300000000,
        0.208020000000, 0.258600000000, 0.323000000000, 0.407300000000, 0.503000000000, 0.608200000000, 0.710000000000, 0.793200000000, 0.862000000000, 0.914850100000,
        0.954000000000, 0.980300000000, 0.994950100000, 1.000000000000, 0.995000000000, 0.978600000000, 0.952000000000, 0.915400000000, 0.870000000000, 0.816300000000,
        0.757000000000, 0.694900000000, 0.631000000000, 0.566800000000, 0.503000000000, 0.441200000000, 0.381000000000, 0.321000000000, 0.265000000000, 0.217000000000,
        0.175000000000, 0.138200000000, 0.107000000000, 0.081600000000, 0.061000000000, 0.044580000000, 0.032000000000, 0.023200000000, 0.017000000000, 0.011920000000,
        0.008210000000, 0.005723000000, 0.004102000000, 0.002929000000, 0.002091000000, 0.001484000000, 0.001047000000, 0.000740000000, 0.000520000000, 0.000361100000,
        0.000249200000, 0.000171900000, 0.000120000000, 0.000084800000, 0.000060000000, 0.000042400000, 0.000030000000, 0.000021200000, 0.000014990000, 0.000010600000,
        0.000007465700, 0.000005257800, 0.000003702900, 0.000002607800, 0.000001836600, 0.000001293400, 0.000000910930, 0.000000641530, 0.000000451810};
    double CIE1931StdObs_z[95] = {
        0.000606100000, 0.001086000000, 0.001946000000, 0.003486000000, 0.006450001000, 0.010549990000,
        0.020050010000, 0.036210000000, 0.067850010000, 0.110200000000, 0.207400000000, 0.371300000000, 0.645600000000, 1.039050100000, 1.385600000000, 1.622960000000,
        1.747060000000, 1.782600000000, 1.772110000000, 1.744100000000, 1.669200000000, 1.528100000000, 1.287640000000, 1.041900000000, 0.812950100000, 0.616200000000,
        0.465180000000, 0.353300000000, 0.272000000000, 0.212300000000, 0.158200000000, 0.111700000000, 0.078249990000, 0.057250010000, 0.042160000000, 0.029840000000,
        0.020300000000, 0.013400000000, 0.008749999000, 0.005749999000, 0.003900000000, 0.002749999000, 0.002100000000, 0.001800000000, 0.001650001000, 0.001400000000,
        0.001100000000, 0.001000000000, 0.000800000000, 0.000600000000, 0.000340000000, 0.000240000000, 0.000190000000, 0.000100000000, 0.000049999990, 0.000030000000,
        0.000020000000, 0.000010000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000,
        0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000,
        0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000,
        0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000};

    // keeping the non-significant figures in comments and canceling them later to reduce floating point errors
    // c1 = 2*pi*h*c^2, where
    // h is the Plank constant
    // c is the speed of light
    // c2 = h*c / k, where
    // h is the Plank constant
    const double plank = 6.62607015; // *10^-34 (Lindbroom had old value of 6.626176)
    // c is the speed of light
    const double lightspeed = 2.99792458; // *10^8
    // k is the Boltzmann constant
    const double boltzmann = 1.380649; // * 10^-23 (Lindbroom had old value of 1.380662)
    double c1 = 2.0 * std::numbers::pi_v<double> * plank * lightspeed * lightspeed; // *10^-18
    double c2 = (plank * lightspeed) / boltzmann; // *10^-3

    // X= integral over lambda of X(lambda) * M(lambda, temp) d lambda
    // Y= integral over lambda of Y(lambda) * M(lambda, temp) d lambda
    // Z= integral over lambda of Z(lambda) * M(lambda, temp) d lambda
    // where
    // X(), Y(), Z() are CIE standard observer matching functions (here representated as a table)
    // M(lambda, temp) = c1 / (lambda^5 * (exp(c2/(lambda * temp)) - 1))

    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    unsigned int index = 0;

    for (unsigned int nm = 360; nm <= 830; nm += 5){
        double dWavelengthM = (double)nm / 1000.0;    // *10^-6
        double dWavelengthM5 = dWavelengthM * dWavelengthM * dWavelengthM * dWavelengthM * dWavelengthM; // *10^-30
        double blackbody = c1 / (dWavelengthM5 * pow(10.0, -12.0) * (exp(c2 / (cct * (dWavelengthM / 1000.0))) - 1.0));
        // The additional 10^-12 and 10-3 terms should make everything cancel: -12 = -30 - (-18)
        X += (blackbody * CIE1931StdObs_x[index]);
        Y += (blackbody * CIE1931StdObs_y[index]);
        Z += (blackbody * CIE1931StdObs_z[index]);
        index++;
    }
    // convert to xyz
    // x= X / (X+Y+Z)
    // y= Y / (X+Y+Z)
    // z = 1 - x - y
    double littlex = X / (X + Y + Z);
    double littley = Y / (X + Y + Z);
    double littlez = 1.0 - littlex - littley;

    return vec3(littlex, littley, littlez);
}
