#ifndef MATRIX_H
#define MATRIX_H

#include "vec3.h"


void print3x3matrix(double input[3][3]);

vec3 multMatrixByColor(const double matrix[3][3], vec3 color);

void mult3x3Matrices(const double A[3][3], const double B[3][3], double output[3][3]);

bool Invert3x3Matrix(const double input[3][3], double output[3][3]);

#endif
