#ifndef CIELAB_H
#define CIELAB_H

#include "vec3.h"

//http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_Lab.html
#define CIELAB_EPSILON 216.0 / 24389.0
#define CIELAB_KAPPA 24389.0 / 27.0

double LABfx(double input);
vec3 XYZtoLAB(vec3 input, vec3 refwhite);

#endif
