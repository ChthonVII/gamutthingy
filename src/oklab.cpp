#include "oklab.h"

#include <math.h>
#include "matrix.h"

// we also need inverses of the constant matrices
double InverseOklabM1[3][3]; // gets initialized by initializeInverseMatrices()
double InverseOklabM2[3][3]; // gets initialized by initializeInverseMatrices()
bool initializeInverseOklabMatrices(){
    if (!Invert3x3Matrix(OklabM1, InverseOklabM1)){
        return false;
    }
    if (!Invert3x3Matrix(OklabM2, InverseOklabM2)){
        return false;
    }
    return true;
}


// conversions
vec3 XYZtoOklab(vec3 input){
    vec3 lms = multMatrixByColor(OklabM1, input);
    // supposedly don't need this for cbrt()
    //if (lms.x < 0) lms.x = 0;
    //if (lms.y < 0) lms.y = 0;
    //if (lms.z < 0) lms.z = 0;
    lms.x = cbrt(lms.x);
    lms.y = cbrt(lms.y);
    lms.z = cbrt(lms.z);
    vec3 lab = multMatrixByColor(OklabM2, lms);
    return lab;
}


vec3 OklabToXYZ(vec3 input){
    vec3 lms = multMatrixByColor(InverseOklabM2, input);
    lms.x = pow(lms.x, 3.0);
    lms.y = pow(lms.y, 3.0);
    lms.z = pow(lms.z, 3.0);
    vec3 xyz = multMatrixByColor(InverseOklabM1, lms);
    return xyz;
}
