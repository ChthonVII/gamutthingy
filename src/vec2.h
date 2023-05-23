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
    // normalizes the vector in place
    void normalize();
    // returns a normalized copy of the vector without altering the original
    vec2 normalizedcopy();
    double magnitude();
    // screen barf
    void printout();
    bool isequal(vec2 other);
};

double DotProduct(vec2 A, vec2 B);

// returns clockwise angle (in radians) between two vectors
double clockwiseAngle(vec2 A, vec2 B);

// finds the intersection between lines AB and CD and puts the result in output
// returns false if parallel; otherwise true
bool lineIntersection2D(vec2 A, vec2 B, vec2 C, vec2 D, vec2 &output);

// assuming A, B, and C are on a line, returns true if B is between A and C; otherwise false
bool isBetween2D(vec2 A, vec2 B, vec2 C);

#endif
