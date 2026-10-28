#include "SexyMatrix.h"
volatile float matrix_input = 1.25f;
int main() {
    Sexy::SexyTransform2D transform;
    transform.Translate(matrix_input, 2.0f);
    transform.Scale(1.125f, 0.875f);
    transform.RotateRad(matrix_input);
    volatile float result = transform.m02;
    return result == 0.0f;
}
