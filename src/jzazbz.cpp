#include "jzazbz.h"

//#include <stdio.h>
#include <math.h>
#include "matrix.h"

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
// It's set to 100 here, since that's the standard for CRT televisions. (Though some models migh have had values up to 175 or even 220ish)

double InverseJzazbzLMSMatrix[3][3]; // gets initialized by initializeInverseMatrices()
double InverseJzazbzIabMatrix[3][3]; // gets initialized by initializeInverseMatrices()

double PQ(double input){
    double XX = pow(input / 10000.0, Jzazbz_n);
    return pow((Jzazbz_c1 + Jzazbz_c2*XX) / (1.0 + Jzazbz_c3*XX), Jzazbz_p);
}

double InversePQ(double input){
    double XX = pow(input, 1.0 / Jzazbz_p);
    return 10000.0 * pow((Jzazbz_c1 - XX) / ((Jzazbz_c3 * XX) - Jzazbz_c2) , 1.0 / Jzazbz_n);
}


bool initializeInverseMatrices(){
    if (!Invert3x3Matrix(JzazbzLMSMatrix, InverseJzazbzLMSMatrix)){
        return false;
    }
    if (!Invert3x3Matrix(JzazbzIabMatrix, InverseJzazbzIabMatrix)){
        return false;
    }
    return true;
}

vec3 XYZtoJzazbz(vec3 input){

    vec3 XYZD65 = input * Jzazbz_peak_lum;
    
    vec3 XYZprimeD65 = {(1.15 * XYZD65.x) - ((1.15 - 1.0) * XYZD65.z), (0.66 * XYZD65.y) - ((0.66 - 1.0) * XYZD65.x), XYZD65.z};
    
    vec3 LMS = multMatrixByColor(JzazbzLMSMatrix, XYZprimeD65);
    
    vec3 LMSprime = {PQ(LMS.x), PQ(LMS.y), PQ(LMS.z)};
    
    vec3 Izazbz = multMatrixByColor(JzazbzIabMatrix, LMSprime);
    
    double Jz = (((1.0 + Jzazbz_d) * Izazbz.x) / (1.0 + (Jzazbz_d * Izazbz.x))) - Jzazbz_d0;
    
    vec3 output = {Jz, Izazbz.y, Izazbz.z};
        
    return output;
    
}

vec3 JzazbzToXYZ(vec3 input){

    double tempIz = input.x + Jzazbz_d0;
    //if (isnan(tempIz)) printf("tempIz is Nan\n");
    double Iz = (tempIz) / (1.0 + Jzazbz_d - (Jzazbz_d * tempIz));
    //if (isnan(Iz)) printf("Iz is Nan\n");
    
    vec3 Izazbz = {Iz, input.y, input.z};
    
    vec3 LMSprime = multMatrixByColor(InverseJzazbzIabMatrix, Izazbz);
    //if (isnan(LMSprime.x) || isnan(LMSprime.y) || isnan(LMSprime.z)) printf("LMSprime is Nan\n");
    
    vec3 LMS = {InversePQ(LMSprime.x), InversePQ(LMSprime.y), InversePQ(LMSprime.z)};
    //if (isnan(LMS.x) || isnan(LMS.y) || isnan(LMS.z)) printf("LMS is Nan\n");
    
    vec3 XYZprime = multMatrixByColor(InverseJzazbzLMSMatrix, LMS);
    
    vec3 XYZ;
    XYZ.x = (XYZprime.x + ((1.15 - 1.0) * XYZprime.z)) / 1.15;
    XYZ.y = (XYZprime.y + ((0.66 - 1.0) * XYZ.x)) / 0.66; // note that the X term is from the line above, not from XYZprime
    XYZ.z = XYZprime.z;
    
    vec3 output = XYZ / Jzazbz_peak_lum;
    
    return output;
}
