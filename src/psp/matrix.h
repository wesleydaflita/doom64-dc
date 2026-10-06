#ifndef DOOM64_PSP_MATRIX_H
#define DOOM64_PSP_MATRIX_H

void PSP_MatrixLoad(const float (*matrix)[4][4]);
void PSP_MatrixApply(const float (*matrix)[4][4]);
void PSP_MatrixTransform(float *x, float *y, float *z, float *w);

#define mat_load PSP_MatrixLoad
#define mat_apply PSP_MatrixApply

#endif
