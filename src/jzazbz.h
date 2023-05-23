#ifndef JZAZBZ_H
#define JZAZBZ_H

#include "vec3.h"

// Jzazbz Implementation------------------------------------------------------------------------------------------------------------------------
// The academic paper:  https://opg.optica.org/oe/fulltext.cfm?uri=oe-25-13-15131&id=368272
// Some other implementations/notes
// https://im.snibgo.com/jzazbz.htm
// https://observablehq.com/@jrus/jzazbz
// https://trev16.hatenablog.com/entry/2021/06/10/010715
// Note that the XYZ values must be relative to D65 whitepoint.
// Note that the value set for Jzazbz_peak_lum has an effect on the hue angles!!!!
// Hue angles, especially red, shift pretty dramatically from 1.0 to 100.0
// But shifts are generally <1 degree from 100.0 to 10000.0
// Not sure if this is a flaw in Jzazbz's design or an accurate depiction of Bezold–Brücke shift...
// It's set to 200 here, since that's around what the devices in our core uses cases output.

#define Jzazbz_b 1.15
#define Jzazbz_g 0.66
#define Jzazbz_c1 (3424.0/4096.0)
#define Jzazbz_c2 (2413.0/128.0)
#define Jzazbz_c3 (2392.0/128.0)
#define Jzazbz_n (2610.0/16384.0)
#define Jzazbz_p (1.7*2523/32.0)
#define Jzazbz_d (-0.56)
#define Jzazbz_d0 (1.6295499532821566e-11)
#define Jzazbz_peak_lum 200.0

// PQ function & inverse
double PQ(double input);
double InversePQ(double input);
// Warning: The Inverse PQ function can return NAN.
// Without doing a formal analysis and proof, I *assume* this is *always* the result of asking pow() to do something that leads to an imaginary or complex number, and *only* happens on inputs that fall outside any possible gamut.

// Constant matrices from the paper
const double JzazbzLMSMatrix[3][3]{
    {0.41478972, 0.579999, 0.0146480},
    {-0.2015100, 1.120649, 0.0531008},
    {-0.0166008, 0.264800, 0.6684799}
};
const double JzazbzIabMatrix[3][3]{
    {0.5, 0.5, 0.0},
    {3.524000, -4.066708, 0.542708},
    {0.199076, 1.096799, -1.295875}
};
extern double InverseJzazbzLMSMatrix[3][3]; // gets initialized by initializeInverseMatrices()
extern double InverseJzazbzIabMatrix[3][3]; // gets initialized by initializeInverseMatrices()
bool initializeInverseMatrices();

// conversions
vec3 XYZtoJzazbz(vec3 input);
vec3 JzazbzToXYZ(vec3 input);


#endif
