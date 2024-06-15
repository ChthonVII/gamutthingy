#ifndef VEC3_H
#define VEC3_H

class vec3 {
public:
    double x;
    double y;
    double z;
    
    // constructors
    vec3();
    vec3(double x, double y, double z);
    
    // operators
    vec3 operator+(vec3 const& other){
        vec3 output;
        output.x = x + other.x;
        output.y = y + other.y;
        output.z = z + other.z;
        return output;
    }
    vec3 operator-(vec3 const& other){
        vec3 output;
        output.x = x - other.x;
        output.y = y - other.y;
        output.z = z - other.z;
        return output;
    }
    vec3 operator*(double other){
        vec3 output;
        output.x = x*other;
        output.y = y*other;
        output.z = z*other;
        return output;
    }
    vec3 operator/(double other){
        vec3 output;
        output.x = x/other;
        output.y = y/other;
        output.z = z/other;
        return output;
    }
    
    // functions
    // normalizes the vector in place
    void normalize();
    // returns a normalized copy of the vector without altering the original
    vec3 normalizedcopy();
    double magnitude();
    // assumes a LCh-style colorspace and outputs the radians hue value as degrees
    double polarangle();
    // screen barf
    void printout();
    bool isequal(vec3 other);
};

double DotProduct(vec3 A, vec3 B);

vec3 CrossProduct(vec3 A, vec3 B);

double Distance3D(vec3 A, vec3 B);

// converts LAB-style colorspaces to their polar LCh-style cousins, or vice versa
// h is in radians
vec3 Polarize(vec3 input);
vec3 Depolarize(vec3 input);

// convert xyY to XYZ
vec3 xyYtoXYZ(vec3 input);

//convert XYZ to xyY
vec3 XYZtoxyY(vec3 input);

#endif
