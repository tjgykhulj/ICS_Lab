/*
name: 汤劲戈
number: 5130309006
 * 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	int i, j, k, t, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, p = -1;
	/*
	 * m=n=32时很简单
	 * B=2^b=32可算出一行可以放八个数字，故切割成8*8的小块
	 这样除了冷缓，每次取A[k][t]都会hit，而存入B[t][k]时可能改变缓存，所以在这种情况下
		 先把值记在tmp0中，循环结束后再存入。
		final: miss = 287<300
	 */
	if (M == 32)
	for (i=0; i<N; i+=8)
		for (j=0; j<M; j+=8)
		for (k=i; k<i+8; k++) {
			for (t=j; t<j+8; t++)
			if (k-i == t-j) { tmp0 = A[k][t]; p = k-i; } else B[t][k] = A[k][t];
			if (p != -1) {	B[p+j][p+i] = tmp0; p = -1; }
		}
	/*
	 * n=m=64时稍微复杂一些
	 * 我一开始考虑的是分k=i~i+3和k=i+4~i+7两种情况，
	 将A[k][j~j+7]分两人次放入B[j~j+3][k]和B[j+4~j+7][k](可以大量减少miss)
	 具体似乎可以减少到一千六左右，记不大清了。但是仍然达不到要求<1200
		 我发现一次只取四个放四个，不够有利。
		 可以先将B[j+4~j+7][k]放入B[j~j+3][k+4]，然后k=i..i+3
		这样就能将B的左上角 放好，左下角暂放于右上角。
		然后可以通过T=B的右上角，B右上＝A左下转置过去，再将B左下＝T
			 最后差右下角单独完成即可
		final: miss = 1179 < 1300
	 */
	if (M == 64)
		for (i=0; i<N; i+=8)
		for (j=0; j<M; j+=8)  {
			for (k=i; k<i+4; k++) {
				tmp0 = A[k][j+0];
				tmp1 = A[k][j+1];
				tmp2 = A[k][j+2];
				tmp3 = A[k][j+3];
				tmp4 = A[k][j+4];
				tmp5 = A[k][j+5];
				tmp6 = A[k][j+6];
				t	   = A[k][j+7];
				B[j+0][k] = tmp0;
				B[j+1][k] = tmp1;
				B[j+2][k] = tmp2;
				B[j+3][k] = tmp3;
				B[j+0][k+4] = tmp4;
				B[j+1][k+4] = tmp5;
				B[j+2][k+4] = tmp6;
				B[j+3][k+4] = t;
			}
			for (k=j; k<j+4; k++) {
				tmp4 = A[i+4][k];
				tmp5 = A[i+5][k];
				tmp6 = A[i+6][k];
				t	    = A[i+7][k];
				tmp0 = B[k][i+4];
				tmp1 = B[k][i+5];
				tmp2 = B[k][i+6];
				tmp3 = B[k][i+7];
				B[k][i+4] = tmp4;
				B[k][i+5] = tmp5;
				B[k][i+6] = tmp6;
				B[k][i+7] = t;
				B[k+4][i+0] = tmp0;
				B[k+4][i+1] = tmp1;
				B[k+4][i+2] = tmp2;
				B[k+4][i+3] = tmp3;
			}
			for (k=i+4; k<i+8; k++) {
				tmp4 = A[k][j+4];
				tmp5 = A[k][j+5];
				tmp6 = A[k][j+6];
				t	   = A[k][j+7];
				B[j+4][k] = tmp4;
				B[j+5][k] = tmp5;
				B[j+6][k] = tmp6;
				B[j+7][k] = t;
			}
		}
	/*
	 * M=61 N=67
	 * 这一部分的行列互换优化其实是靠运气试出来的
	 * s=14也是后期调出来的，手算后感觉有点道理，于是最终版本就这样了。
	 * final: miss = 1807 < 2000
	 */ 
#define s 14
	if (M == 61) 
	for (i=0; i<N; i+=s)
	for (j=0; j<M; j+=s)
		for (k=j; k<j+s && k<M; k++)
		for (t=i; t<i+s && t<N; t++)  B[k][t] = A[t][k];
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

