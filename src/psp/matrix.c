#include "matrix.h"

#include <string.h>

static float current_matrix[4][4] __attribute__((aligned(16)));

void PSP_MatrixLoad(const float (*matrix)[4][4])
{
	memcpy(current_matrix, *matrix, sizeof(current_matrix));
}

void PSP_MatrixApply(const float (*matrix)[4][4])
{
	float result[4][4];

	for (int row = 0; row < 4; row++) {
		for (int column = 0; column < 4; column++) {
			result[row][column] = 0.0f;
			for (int inner = 0; inner < 4; inner++)
				result[row][column] += (*matrix)[row][inner] *
					current_matrix[inner][column];
		}
	}

	memcpy(current_matrix, result, sizeof(current_matrix));
}

void PSP_MatrixTransform(float *x, float *y, float *z, float *w)
{
	float input[4] = { *x, *y, *z, 1.0f };
	float output[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	for (int column = 0; column < 4; column++)
		for (int row = 0; row < 4; row++)
			output[column] += input[row] * current_matrix[row][column];

	*x = output[0];
	*y = output[1];
	*z = output[2];
	*w = output[3];
}
