#include "cielab.h"

#include <math.h>

// LAB implementation ------------------------------------------------------------------------------------
// http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_Lab.html
double LABfx(double input){
    if (input > CIELAB_EPSILON){
        return cbrt(input);
    }
    return ((input * CIELAB_KAPPA) + 16.0) / 116.0;
}

vec3 XYZtoLAB(vec3 input, vec3 refwhite){
    refwhite.x = refwhite.x / refwhite.y;
    refwhite.z = refwhite.z / refwhite.y;
    refwhite.y = 1.0;
    vec3 normXYZ;
    normXYZ.x = input.x / refwhite.x;
    normXYZ.y = input.y / refwhite.y;
    normXYZ.z = input.z / refwhite.z;
    double fx = LABfx(normXYZ.x);
    double fy = LABfx(normXYZ.y);
    double fz = LABfx(normXYZ.z);
    vec3  output;
    output.x = (116.0 * fy) - 16.0; // L
    output.y = 500.0 * (fx - fy); // a
    output.z = 200.0 * (fy - fz); // b
    return output;
}

// TODO: LAB back to XYZ
