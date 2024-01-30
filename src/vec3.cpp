#include "vec3.h"
#include "constants.h"

#include <stdio.h>
#include <math.h>
#include <numbers>


vec3:: vec3(){ x = y = z = 0;}

vec3::vec3(double a, double b, double c){
    x = a;
    y = b;
    z = c;
}

double vec3::magnitude(){
    return sqrt((x * x) + (y * y) + (z * z));
}

// assumes a LCh style colorspace and outputs the radians hue value as degrees
double vec3::polarangle(){
    return z * (180.0 / std::numbers::pi_v<long double>);
}

// normalizes the vector in place
void vec3::normalize(){
    double mag= magnitude();
    x /= mag;
    y /= mag;
    z /= mag;
}

// returns a normalized copy of the vector without altering the original
vec3 vec3::normalizedcopy(){
    double mag = magnitude();
    vec3 output;
    output.x = x / mag;
    output.y = y / mag;
    output.z = z / mag;
    return output;
}

// screen barf
void vec3::printout(){
    printf("{%.10f, %.10f, %.10f}\n", x, y, z);
    return;
}

bool vec3::isequal(vec3 other){
    if (fabs(x - other.x) > EPSILON) return false;
    if (fabs(y - other.y) > EPSILON) return false;
    if (fabs(z - other.z) > EPSILON) return false;
    return true;
}

double DotProduct(vec3 A, vec3 B){
    return (A.x * B.x) + (A.y * B.y) + (A.z * B.z);
}

vec3 CrossProduct(vec3 A, vec3 B){
    vec3 output;
    output.x = (A.y * B.z) - (A.z * B.y);
    output.y = (A.z * B.x) - (A.x * B.z);
    output.z = (A.x * B.y) - (A.y * B.x);
    return output;
}

// converts LAB-style colorspaces to their polar LCh-style cousins
// h is in radians
vec3 Polarize(vec3 input){
    vec3 output;
    output.x = input.x;
    output.y = sqrt((input.y * input.y) + (input.z * input.z));
    output.z = atan2(input.z, input.y);
    if (output.z < 0){
        output.z += 2.0 * std::numbers::pi_v<long double>;
    }
    
    return output;
}

// converts LCh-style colorpaces back to their LAB-style cartesian cousins
// h is in radians
vec3 Depolarize(vec3 input){
    vec3 output;
    output.x = input.x;
    output.y = input.y * cos(input.z);
    output.z = input.y * sin(input.z);
    
    
    return output;
}

// not used?
/*
// returns true if point B is between A and C
bool between(vec3 A, vec3 B, vec3 C){
    bool xOK = ((A.x >= B.x) && (B.x >= C.x)) || ((A.x <= B.x) && (B.x <= C.x));
    bool yOK = ((A.y >= B.y) && (B.y >= C.y)) || ((A.y <= B.y) && (B.y <= C.y));
    bool zOK = ((A.z >= B.z) && (B.z >= C.z)) || ((A.z <= B.z) && (B.x <= C.z));
    return xOK && yOK && zOK;
}
*/

// convert xyY to XYZ
vec3 xyYtoXYZ(vec3 input){
    /*
    X= xY/y
    Y=Y
    Z=((1-x-y)Y)/y
    */
    double X = (input.x * input.z)/input.y;
    double Z = ((1.0 - input.x - input.y) * input.z)/input.y;
    return vec3(X, input.z, Z);
}
