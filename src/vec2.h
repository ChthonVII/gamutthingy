#ifndef VEC2_H
#define VEC2_H

// 2D stuff
class vec2 {
public:
    double x;
    double y;
    
    // constructors
    vec2();
    vec2(double x, double y);
    
    // operators
    vec2 operator+(vec2 const& other){
        vec2 output;
        output.x = x + other.x;
        output.y = y + other.y;
        return output;
    }
    vec2 operator-(vec2 const& other){
        vec2 output;
        output.x = x - other.x;
        output.y = y - other.y;
        return output;
    }
    vec2 operator*(double other){
        vec2 output;
        output.x = x*other;
        output.y = y*other;
        return output;
    }
    vec2 operator/(double other){
        vec2 output;
        output.x = x/other;
        output.y = y/other;
        return output;
    }
    
    // functions
    void normalize();
    vec2 normalizedcopy();
    double magnitude();
    void printout();
    bool isequal(vec2 other);
};

double DotProduct(vec2 A, vec2 B);

double clockwiseAngle(vec2 A, vec2 B);

bool lineIntersection2D(vec2 A, vec2 B, vec2 C, vec2 D, vec2 &output);

bool isBetween2D(vec2 A, vec2 B, vec2 C);

#endif
