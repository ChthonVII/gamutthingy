#include "matrix.h"

#include <stdio.h>
#include <math.h>

// screen barfs a 3x3 matrix
void print3x3matrix(double input[3][3]){
    printf("{{%.10f, %.10f, %.10f},\n{%.10f, %.10f, %.10f},\n{%.10f, %.10f, %.10f}}\n", input[0][0], input[0][1], input[0][2], input[1][0], input[1][1], input[1][2], input[2][0], input[2][1], input[2][2]);
    return;
}


// multiplies matrix * color
// remember that a c++ 2-dimensional array is [row][col]
vec3 multMatrixByColor(const double matrix[3][3], vec3 color){
    vec3 output;
    output.x = matrix[0][0] * color.x + matrix[0][1] * color.y + matrix[0][2] * color.z;
    output.y = matrix[1][0] * color.x + matrix[1][1] * color.y + matrix[1][2] * color.z;
    output.z = matrix[2][0] * color.x + matrix[2][1] * color.y + matrix[2][2] * color.z;
    return output;
}

// multiplies A*B and puts result in output
// remember that a c++ 2-dimensional array is [row][col]
void mult3x3Matrices(const double A[3][3], const double B[3][3], double output[3][3]){
    for (int row=0; row<3; row++){
        for (int col=0; col<3; col++){
            output[row][col] = (A[row][0] * B[0][col]) + (A[row][1] * B[1][col]) + (A[row][2] * B[2][col]);
        }
    }
    return;
}

// inverts input and puts result in output
// returns false if matrix is not invertible; otherwise true
// remember that a c++ 2-dimensional array is [row][col]
bool Invert3x3Matrix(const double input[3][3], double output[3][3]){
    
    double determinant =  (input[0][0] * input[1][1] * input[2][2])
                                    + (input[0][1] * input[1][2] * input[2][0])
                                    + (input[0][2] * input[1][0] * input[2][1])
                                    - (input[0][0] * input[1][2] * input[2][1])
                                    - (input[0][1] * input[1][0] * input[2][2])
                                    - (input[0][2] * input[1][1] * input[2][0]);
                                        
    if (determinant == 0.0){
        return false;
    }
    
    double cofactormatrix[3][3];
    bool signchange = false;
    for (int row=0; row<3; row++){
        for (int col=0; col<3; col++){
            double minormatrix[4];
            int mmindex = 0;
            for (int mrow=0; mrow<3; mrow++){
                for (int mcol=0; mcol<3; mcol++){
                    if ((row != mrow) && (col != mcol)){
                        minormatrix[mmindex] = input[mrow][mcol]; 
                        mmindex++;
                    }
                }
            }
            double cofactor = (minormatrix[0] * minormatrix[3]) - (minormatrix[1] * minormatrix[2]);
            if (signchange){
                cofactor *= -1.0;
            }
            signchange = !signchange;
            cofactormatrix[row][col] = cofactor;
        }
    }
    
    double adjointmatrix[3][3];
    for (int row=0; row<3; row++){
        for (int col=0; col<3; col++){
            adjointmatrix[row][col] = cofactormatrix[col][row];
        }
    }
    
    // separate out this step for clarity
    for (int row=0; row<3; row++){
        for (int col=0; col<3; col++){
            output[row][col] = adjointmatrix[row][col] / determinant;
        }
    }
    
    return true;
}
