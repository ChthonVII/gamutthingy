#ifndef OKLAB_H
#define OKLAB_H

#include "vec3.h"

// https://bottosson.github.io/posts/oklab/
// https://en.wikipedia.org/wiki/Oklab_color_space

// Constant matrices from the paper
const double OklabM1[3][3]{
    {+0.8189330101, +0.3618667424, -0.1288597137},
    {+0.0329845436, +0.9293118715, +0.0361456387},
    {+0.0482003018, +0.2643662691, +0.6338517070}
};
const double OklabM2[3][3]{
    {+0.2104542553, +0.7936177850, -0.0040720468},
    {+1.9779984951, -2.4285922050, +0.4505937099},
    {+0.0259040371, +0.7827717662, -0.8086757660}
};
extern double InverseOklabM1[3][3]; // gets initialized by initializeInverseOklabMatrices()
extern double InverseOklabM2[3][3]; // gets initialized by initializeInverseOklabMatrices()
bool initializeInverseOklabMatrices();

// conversions
vec3 XYZtoOklab(vec3 input);
vec3 OklabToXYZ(vec3 input);

#endif
