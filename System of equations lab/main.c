#include "return_codes.h"

#include <malloc.h>
#include <math.h>
#include <stdio.h>

int equals(float num, float zero)
{
	return fabsf(num - zero) <= 1e-5f ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("Wrong number of arguments");
		return ERROR_INVALID_DATA;
	}

	int size = 0;
	int rank;
	int extended_rank;

	FILE *in = fopen(argv[1], "r");

	if (in == NULL)
	{
		printf("cannot open an input file");
		return ERROR_FILE_NOT_FOUND;
	}

	FILE *out = fopen(argv[2], "w");

	if (out == NULL)
	{
		printf("cannot open an output file");
		fclose(in);
		return ERROR_FILE_NOT_FOUND;
	}

	fscanf(in, "%i", &size);

	float *A = malloc(sizeof(float) * size * (size + 1));
	if (A == NULL)
	{
		printf("cannot allocate memory");
		fclose(in);
		fclose(out);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	rank = size;

	/*for (int row = 0; row < size; row++)
	{
		for (int column = 0; column < (size + 1); column++)
		{
			fscanf(in, "%f", &A[row * (size + 1) + column]);
		}
	}*/

	for (int i = 0; i < size * (size + 1); i++)
	{
		fscanf(in, "%f", &A[i]);
	}

	int i = 0;
	int j = 0;
	while (i < size && j < size)
	{
		if (equals(A[i * (size + 1) + j], 0))
		{
			for (int k = i + 1; k < size; k++)
			{
				if (!equals(A[k * (size + 1) + j], 0))
				{
					for (int l = 0; l < size + 1; l++)
					{
						float t = A[k * (size + 1) + l];
						A[k * (size + 1) + l] = A[i * (size + 1) + l];
						A[i * (size + 1) + l] = t;
					}

					break;
				}
			}
		}
		if (!equals(A[i * (size + 1) + j], 0))
		{
			for (int k = i + 1; k < size; k++)
			{
				float coef = A[k * (size + 1) + j] / A[i * (size + 1) + j];
				for (int l = 0; l < size + 1; l++)
				{
					A[k * (size + 1) + l] = A[k * (size + 1) + l] - A[i * (size + 1) + l] * coef;
				}
			}
			i++;
			j++;
		}
		else	// for  many solutions and no solutions
		{
			rank--;
			j++;
		}
	}

	extended_rank = rank;
	for (i = 0; i < size - rank; i++)
	{
		if (!equals(A[(size - i - 1) * (size + 1) + size], 0))	  // for no solutions
		{
			extended_rank++;
			break;
		}
	}

	if (rank != extended_rank)
	{
		fprintf(out, "no solution\n");
	}
	else if (rank == size && rank == extended_rank)
	{
		float *res = malloc(sizeof(float) * size);
		if (res == NULL)
		{
			printf("cannot allocate memory");
			fclose(in);
			fclose(out);
			free(A);
			return ERROR_NOT_ENOUGH_MEMORY;
		}
		for (i = size - 1; i > -1; i--)
		{
			float d = 0;
			for (j = i + 1; j < size; j++)
			{
				float s = A[i * (size + 1) + j] * res[j];
				d += s;
			}
			res[i] = (A[i * (size + 1) + size] - d) / A[i * (size + 1) + i];
		}
		for (i = 0; i < size; i++)
		{
			fprintf(out, "%g\n", res[i]);
		}

		free(res);
	}
	else if (rank == extended_rank && extended_rank != size)
	{
		fprintf(out, "many solutions\n");
	}
	fclose(in);
	fclose(out);
	free(A);
	return 0;
}
