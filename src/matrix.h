#ifndef MATRIX_H
#define MATRIX_H

#include "vec3.h"

// screen barfs a 3x3 matrix
void print3x3matrix(double input[3][3]);

// multiplies matrix * color
vec3 multMatrixByColor(const double matrix[3][3], vec3 color);

// multiplies A*B and puts result in output
void mult3x3Matrices(const double A[3][3], const double B[3][3], double output[3][3]);

// inverts input and puts result in output
// returns false if matrix is not invertible; otherwise true
bool Invert3x3Matrix(const double input[3][3], double output[3][3]);

#endif
