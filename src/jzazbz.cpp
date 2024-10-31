#include "jzazbz.h"

#include <math.h>
#include "matrix.h"

//#include <stdio.h>

// we also need inverses of the constant matrices
double InverseJzazbzLMSMatrix[3][3]; // gets initialized by initializeInverseMatrices()
double InverseJzazbzIabMatrix[3][3]; // gets initialized by initializeInverseMatrices()
bool initializeInverseMatrices(){
    if (!Invert3x3Matrix(JzazbzLMSMatrix, InverseJzazbzLMSMatrix)){
        return false;
    }
    if (!Invert3x3Matrix(JzazbzIabMatrix, InverseJzazbzIabMatrix)){
        return false;
    }
    return true;
}

// PQ function & inverse
double PQ(double input){
    double XX = pow(input / 10000.0, Jzazbz_n);
    return pow((Jzazbz_c1 + Jzazbz_c2*XX) / (1.0 + Jzazbz_c3*XX), Jzazbz_p);
}
// Warning: The Inverse PQ function can return NAN.
// Without doing a formal analysis and proof, I *assume* this is *always* the result of asking pow() to do something that leads to an imaginary or complex number, and *only* happens on inputs that fall outside any possible gamut.
double InversePQ(double input){
    double XX = pow(input, 1.0 / Jzazbz_p);
    return 10000.0 * pow((Jzazbz_c1 - XX) / ((Jzazbz_c3 * XX) - Jzazbz_c2) , 1.0 / Jzazbz_n);
}




vec3 XYZtoJzazbz(vec3 input){

    vec3 XYZD65 = input * Jzazbz_peak_lum;
    
    vec3 XYZprimeD65 = {(Jzazbz_b * XYZD65.x) - ((Jzazbz_b - 1.0) * XYZD65.z), (Jzazbz_g * XYZD65.y) - ((Jzazbz_g - 1.0) * XYZD65.x), XYZD65.z};
    
    vec3 LMS = multMatrixByColor(JzazbzLMSMatrix, XYZprimeD65);
    
    // Expanded intermediate LUTs expressly contain out-of-bounds colors that neede to be bypassed.
    if ((LMS.x < 0.0) || (LMS.y < 0.0) || (LMS.z < 0.0)){
        //printf("Yeah, gonna crash. %10f, %10f, %10f\n", LMS.x, LMS.y, LMS.z);
        return vec3(0.0, 0.0, 0.0);
    }

    vec3 LMSprime = {PQ(LMS.x), PQ(LMS.y), PQ(LMS.z)};
    
    vec3 Izazbz = multMatrixByColor(JzazbzIabMatrix, LMSprime);
    
    double Jz = (((1.0 + Jzazbz_d) * Izazbz.x) / (1.0 + (Jzazbz_d * Izazbz.x))) - Jzazbz_d0;
    
    vec3 output = {Jz, Izazbz.y, Izazbz.z};

    return output;
    
}

vec3 JzazbzToXYZ(vec3 input){

    double tempIz = input.x + Jzazbz_d0;

    double Iz = (tempIz) / (1.0 + Jzazbz_d - (Jzazbz_d * tempIz));
    
    vec3 Izazbz = {Iz, input.y, input.z};
    
    vec3 LMSprime = multMatrixByColor(InverseJzazbzIabMatrix, Izazbz);
    
    vec3 LMS = {InversePQ(LMSprime.x), InversePQ(LMSprime.y), InversePQ(LMSprime.z)};
    
    vec3 XYZprime = multMatrixByColor(InverseJzazbzLMSMatrix, LMS);
    
    vec3 XYZ;
    XYZ.x = (XYZprime.x + ((Jzazbz_b - 1.0) * XYZprime.z)) / Jzazbz_b;
    XYZ.y = (XYZprime.y + ((Jzazbz_g - 1.0) * XYZ.x)) / Jzazbz_g; // note that the X term is from the line above, not from XYZprime
    XYZ.z = XYZprime.z;
    
    vec3 output = XYZ / Jzazbz_peak_lum;
    
    return output;
}
