#ifndef JZAZBZ_H
#define JZAZBZ_H

#include "vec3.h"

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

double PQ(double input);

double InversePQ(double input);

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

vec3 XYZtoJzazbz(vec3 input);

vec3 JzazbzToXYZ(vec3 input);



#endif
