#include "vec2.h"
#include "constants.h"

#include <stdio.h>
#include <math.h>
#include <numbers>

vec2::vec2(){ x = y = 0;}

vec2::vec2(double a, double b){
    x = a;
    y = b;
}

double vec2::magnitude(){
    return sqrt((x * x) + (y * y));
}

// normalizes the vector in place
void vec2::normalize(){
    double mag= magnitude();
    x /= mag;
    y /= mag;
}

// returns a normalized copy of the vector without altering the original
vec2 vec2::normalizedcopy(){
    double mag = magnitude();
    vec2 output;
    output.x = x / mag;
    output.y = y / mag;
    return output;
}

void vec2::printout(){
    printf("{%.10f, %.10f}\n", x, y);
    return;
}

bool vec2::isequal(vec2 other){
    if (fabs(x - other.x) > EPSILON) return false;
    if (fabs(y - other.y) > EPSILON) return false;
    return true;
}

double DotProduct(vec2 A, vec2 B){
    return (A.x * B.x) + (A.y * B.y);
}


// returns clockwise angle (in radians) between two vectors
// see https://stackoverflow.com/questions/14066933/direct-way-of-computing-the-clockwise-angle-between-two-vectors
double clockwiseAngle(vec2 A, vec2 B){
    double dot = DotProduct(A, B);
    double det = (B.x * A.y) - (B.y * A.x);
    //printf("angle inputs %f, %f and %f, %f; dot %f, det %f, angle (rad) %f\n", A.x, A.y, B.x, B.y, dot, det, atan2(det, dot));
    return atan2(det, dot);
}

// finds the intersection between lines AB and CD and puts the result in output
// returns false if parallel; otherwise true
bool lineIntersection2D(vec2 A, vec2 B, vec2 C, vec2 D, vec2 &output){
    double a1 = B.y - A.y;
    double b1 = A.x - B.x;
    double c1 = (a1 * A.x) + (b1 * A.y);
    
    double a2 = D.y - C.y;
    double b2 = C.x - D.x;
    double c2 = (a2 * C.x) + (b2 * C.y);
    
    double determinant = (a1 * b2) - (a2 * b1);
    
    if (fabs(determinant) < EPSILONZERO){
        return false;
    }
    
    output.x = ((b2 * c1) - (b1 * c2)) / determinant;
    output.y = ((a1 * c2) - (a2 * c1)) / determinant;
    return true;
}

// assuming A, B, and C are on a line, returns true if B is between A and C; otherwise false
bool isBetween2D(vec2 A, vec2 B, vec2 C){
    return (
        (((A.x >= B.x) && (B.x >= C.x)) || ((A.x <= B.x) && (B.x <= C.x))) &&
        (((A.y >= B.y) && (B.y >= C.y)) || ((A.y <= B.y) && (B.y <= C.y)))
    );
}
