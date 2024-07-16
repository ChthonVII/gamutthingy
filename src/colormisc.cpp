#include "colormisc.h"
#include "constants.h"

#include <math.h>
#include <numbers>
#include <complex>
#include <stdio.h>

// constants for inverse hermite
const std::complex<double>two_pi_i = 2.0 * std::numbers::pi_v<double> * std::complex<double>(0.0,1.0);
const std::complex<double>cuberootrotate = exp(two_pi_i / 3.0);

// globals for BT.1886 Appendix 1 EOTF function
double CRT_EOTF_blacklevel;
double CRT_EOTF_whitelevel;
double CRT_EOTF_b;
double CRT_EOTF_k;
double CRT_EOTF_s;
double CRT_EOTF_i;



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

// The EOTF function from BT.1886 Appendix 1 for approximating the behavior of CRT televisions.
// The function from Appendix 1 is more faithful than the fairly useless Annex 1 function, which is just 2.4 gamma
// The function has been modified to handle negative inputs in the same fashion as IEC 61966-2-4 (a.k.a. xvYCC)
// (BT.1361 does something similar.)
// Dynamic range is restored in a post-processing step that chops off the black lift and then normalizes to 0-1.
// Initialize1886EOTF() must be run once before this can be used.
double tolinear1886appx1(double input){
    
    // Shift input by b
    input += CRT_EOTF_b;
    
    // Handle negative input by flipping sign 
    bool flipsign = false;
    if (input < 0){
        input *= -1.0;
        flipsign = true;
    }
    
    // The main EOTF function
    double output;
    if (input < (0.35 + CRT_EOTF_b)){
        output = CRT_EOTF_k * CRT_EOTF_s * pow(input, 3.0);
    }
    else {
        output = CRT_EOTF_k * pow(input, 2.6);
    }
    
    // Flip sign again if input was negative
    if (flipsign){
        output *= -1.0;
    }
    
    // Chop off the black lift and normalize to 0-1
    output -= CRT_EOTF_blacklevel;
    output /= (CRT_EOTF_whitelevel - CRT_EOTF_blacklevel);
    
    // fix floating point errors very near 0 or 1
    if ((output != 0.0) && (fabs(output - 0.0) < 1e-6)){
        output = 0.0;
    }
    else if ((output != 1.0) && (fabs(output - 1.0) < 1e-6)){
        output = 1.0;
    }
    
    return output;
}

// inverse of tolinear1886appx1()
// Initialize1886EOTF() must be run once before this can be used.
double togamma1886appx1(double input){
    
    // undo the chop and normalization post-processing
    input *= (CRT_EOTF_whitelevel - CRT_EOTF_blacklevel);
    input += CRT_EOTF_blacklevel;
    
    // Handle negative input by flipping sign 
    bool flipsign = false;
    if (input < 0){
        input *= -1.0;
        flipsign = true;
    }
    
    // The main EOTF function
    double output;
    if (input < CRT_EOTF_i){
        output = pow((1.0/CRT_EOTF_k) * (1.0/CRT_EOTF_s) * input, 1.0/3.0);
    }
    else {
        output = pow((1.0/CRT_EOTF_k) * input, 1.0/2.6);
    }
    
    // Flip sign again if input was negative
    if (flipsign){
        output *= -1.0;
    }
    
    //unshift
    output -= CRT_EOTF_b;
    
    // fix floating point errors very near 0 or 1
    if ((output != 0.0) && (fabs(output - 0.0) < 1e-6)){
        output = 0.0;
    }
    else if ((output != 1.0) && (fabs(output - 1.0) < 1e-6)){
        output = 1.0;
    }
    
    return output;
    
}


// Brute force the value of "b" for the BT.1886 Appendix 1 EOTF function using binary search.
// blacklevel is CRT luminosity in cd/m^2 given black input, divided by 100 (sane value 0.001)
// whitelevel is CRT luminosity in cd/m^2 given white input, divided by 100 (sane value 1.0)
double BruteForce1886B(double blacklevel, double whitelevel){
    
    if (blacklevel == 0.0){
        return 0.0;
    }
    
    double floor = 0.0;
    double ceiling = 1.0;
    double guess;
    int iters = 0;
    
    while(true){
        guess = (floor + ceiling) * 0.5;
        double gresult = (whitelevel / pow(1.0 + guess, 2.6)) * pow(0.35 + guess, -0.4) * pow(guess, 3.0);
        // exact hit!
        if (gresult == blacklevel){
            break;
        }
        iters++;
        // should have converged by now
        if (iters > 100){
            break;
        }
        double error = fabs(gresult - blacklevel);
        // wolfram alpha was giving 16 digits, so lets hope for the same
        if (error < 1e-16){
            break;
        }
        if (gresult > blacklevel){
            ceiling = guess;
        }
        else {
            floor = guess;
        }
    }
    
    return guess;
}

void Initialize1886EOTF(double blacklevel, double whitelevel, int verbosity){
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing CRT EOTF emulation...\n");
    }
    
    if (blacklevel < 0.0){
        printf("Negative luminosity is impossible. Setting CRT black level to 0.\n");
        blacklevel = 0;
    }
    if (whitelevel < 0.0){
        printf("Negative luminosity is impossible. Setting CRT white level to 0.\n");
        whitelevel = 0;
    }
    if (blacklevel >= whitelevel){
        printf("Black brighter than white is impossible. Setting black and white levels to defaults.\n");
        blacklevel = 0.001;
        whitelevel = 1.0;
    }
    CRT_EOTF_blacklevel = blacklevel;
    CRT_EOTF_whitelevel = whitelevel;
    CRT_EOTF_b = BruteForce1886B(blacklevel, whitelevel);
    CRT_EOTF_k = whitelevel / pow(1.0 + CRT_EOTF_b, 2.6);
    CRT_EOTF_s = pow(0.35 + CRT_EOTF_b, -0.4);
    CRT_EOTF_i = CRT_EOTF_k * pow(0.35 + CRT_EOTF_b, 2.6);

    if (verbosity >= VERBOSITY_SLIGHT){
        printf("CRT emulation EOTF constants:\nblack level: %f\nwhite level: %f\nb: %.16f\nk: %.16f\ns: %.16f\ni: %.16f\n", CRT_EOTF_blacklevel, CRT_EOTF_whitelevel, CRT_EOTF_b, CRT_EOTF_k, CRT_EOTF_s, CRT_EOTF_i);
        
        //screen barf a bunch of test values to graph elsewhere and make sure it's right
        /*
        for (int i=0; i<=2000; i++){
            double testinput = i * 0.0005;
            double testoutput = tolinear1886appx1(testinput);
            printf("%f, %f\n", testinput, testoutput);
        }
        */
        /*
        printf("inverse EOTF tests:\n");
        double testinput = 0.0;
        double testoutput = tolinear1886appx1(testinput);
        double retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.25;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.5;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.75;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 1.0;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.35;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.3501;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.3499;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = -0.1;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        */
        
    }
    printf("\n----------\n\n");
    return;
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
