//
// Created by kirill on 7.03.17.
//

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>

#include "sp_mat.h"
#include "parser.h"
#include "sgpu.h"

#define ROOT 0
#define DIM_CART 2

#define IND(x,y,z) ((x) + (y)*NX + (z)*NX*(NYr + 2))

const char pathSetting[] = "../../../../initial/setting3.ini";
const char pathFunction[] = "../../../../initial/function3.txt";

const char pathResult1D[] = "../../../../result/Kirill/euler1D_3.txt";
const char pathResult3D[] = "../../../../result/Kirill/euler3D_3.txt";

//const char pathSetting[] = "setting3.ini";
//const char pathFunction[] = "function3.txt";
//const char pathResult3D[] = "res.txt";

int main(int argc, char **argv) {
  int sizeP, rankP;
  size_t sizeTime;
  omp_set_num_threads(2);

  MPI_Status status[4];
  double t0 = 0.0, t1 = 0.0;

  int blockYP = 0, blockZP = 0;

  MPI_Init(&argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD, &sizeP);
  MPI_Comm_rank(MPI_COMM_WORLD, &rankP);
  if (rankP == ROOT) printf("MPI RUN ON %d PROCESS\n", sizeP);

  SpMatrix mat;
  size_t dim;
  double coeffs[4];
  double* u = NULL, *u_chunk = NULL, *un_chunk;

  int NX, NY, NZ, NYr, NZr;

  if (rankP == ROOT) {
    int error;
    Setting setting;
    error = readSetting(pathSetting, &setting);

    if (error != OK) return error;

    dim = (size_t)(setting.NX + 2)*(setting.NY+2)*(setting.NZ+2);
    u = (double *) calloc(dim, sizeof(double));

    error = readFunction(pathFunction, u, setting.NX + 2, setting.NY + 2, setting.NZ + 2, 1);

    if (error != OK) return error;

    sizeTime = (size_t) ((setting.TFINISH - setting.TSTART) / setting.dt);

#if ENABLE_PARALLEL
    printf("PARALLEL VERSION 2.0!\n");
#endif

    printf("TimeSize -\t%lu\n", sizeTime);

    double dx = fabs(setting.XSTART - setting.XEND) / setting.NX;
    double dy = fabs(setting.YSTART - setting.YEND) / setting.NY;
    double dz = fabs(setting.ZSTART - setting.ZEND) / setting.NZ;

    coeffs[0] = 1.0 - 2.0*setting.dt*setting.SIGMA*(1.0/(dx*dx) + 1.0/(dy*dy) + 1.0/(dz*dz));
    coeffs[1] = setting.dt * setting.SIGMA / (dx * dx);
    coeffs[2] = setting.dt * setting.SIGMA / (dy * dy);
    coeffs[3] = setting.dt * setting.SIGMA / (dz * dz);

    NX = setting.NX + 2;
    NY = setting.NY + 2;
    NZ = setting.NZ + 2;
  }

  MPI_Bcast(&sizeTime, 1, MPI_UNSIGNED_LONG, ROOT, MPI_COMM_WORLD);
  MPI_Bcast(coeffs, 4, MPI_DOUBLE, ROOT, MPI_COMM_WORLD);
  MPI_Bcast(&NX, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
  MPI_Bcast(&NY, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
  MPI_Bcast(&NZ, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

  //  Определения числа процессов в каждом измерении
  get_blocks(&blockYP, &blockZP, sizeP);

//  if (rankP == ROOT) printf("blockY %d blockZ %d\n", blockYP, blockZP);

  NYr = (NY - 2)/blockYP;
  NZr = (NZ - 2)/blockZP;

  MPI_Comm gridComm;

  //  размер каждой размерности
  int dims[DIM_CART];   int periods[DIM_CART];
  int gridCoords[DIM_CART];

  dims[0] = blockZP; dims[1] = blockYP;
  //  наличие циклов в каждой размерности
  periods[0] = 0; periods[1] = 0;
  //  разрешение системе менять номера процессов
  int reorder = 0;
  MPI_Cart_create(MPI_COMM_WORLD, DIM_CART, dims, periods, reorder, &gridComm);

  // Определение координат процесса в решетке
  MPI_Cart_coords(gridComm, rankP, DIM_CART, gridCoords);

  size_t dimChunk = (size_t)NX*(NYr+2)*(NZr+2);
  u_chunk = (double *)calloc(dimChunk, sizeof(double));
  un_chunk = (double *)malloc(sizeof(double)*dimChunk);

  double *tmp;

  //  SCATTER
  scatter_by_block(u, u_chunk, NX, NY, NYr, NZr, gridComm, 2);

  size_t nonZero = dimChunk*7;

  initSpMat(&mat, nonZero, dimChunk);
//  createExplicitSpMat(&mat, coeffs, dimChunk, NX, NX*(NYr+2));
  createExplicitSpMatV2(&mat, coeffs, NX, NYr + 2, NZr + 2);

  int rank_left, rank_right, rank_down, rank_top;
  MPI_Cart_shift(gridComm, 1, -1, &rank_left, &rank_right);
  MPI_Cart_shift(gridComm, 0, 1, &rank_down, &rank_top);

//   printf("rank - %d; left %d; right %d; top %d; down %d\n", rankP, rank_left, rank_right, rank_top, rank_down);

  if (rankP == ROOT) {
    printf("START!\n");
    t0 = omp_get_wtime();
  }

  // Создание типа плоскости XY и XZ
  MPI_Datatype planeXY;
  MPI_Type_vector(NZr+2, NX, NX*(NYr+2), MPI_DOUBLE, &planeXY);
  MPI_Type_commit(&planeXY);

  MPI_Datatype planeXZ;
  MPI_Type_contiguous(NX*(NYr+2), MPI_DOUBLE, &planeXZ);
  MPI_Type_commit(&planeXZ);
  // *****************************

  for (int z = 0; z < NZr+2; z++) {
    for (int y = 0; y < NYr+2; y++) {
      for (int x = 0; x < NX; x++) {
        if (x==0)
          u_chunk[IND(x,y,z)]=u_chunk[IND(x+1,y,z)];
        if (x==NX-1)
          u_chunk[IND(x,y,z)]=u_chunk[IND(x-1,y,z)];
        if (y==0)
          u_chunk[IND(x,y,z)]=u_chunk[IND(x,y+1,z)];
        if (y==NYr+1)
          u_chunk[IND(x,y,z)]=u_chunk[IND(x,y-1,z)];
        if (z==0)
          u_chunk[IND(x,y,z)]=u_chunk[IND(x,y,z+1)];
        if (z==NZr+1)
          u_chunk[IND(x,y,z)]=u_chunk[IND(x,y,z-1)];
      }
    }
  }


  // ОСНОВНЫЕ ВЫЧИСЛЕНИЯ
  for (int t = 1; t <= sizeTime; t++) {
    //  ОБМЕН ГРАНИЦ ПО Y И Z

    //    Передача влево по Y
    MPI_Sendrecv(&u_chunk[IND(0, NYr, 0)],  1, planeXY, rank_left, 0,
                 &u_chunk[IND(0, 0, 0)], 1, planeXY, rank_right, 0, gridComm, &status[0]);

    //    Передача вправо по Y
    MPI_Sendrecv(&u_chunk[IND(0, 1, 0)], 1, planeXY, rank_right, 1,
                 &u_chunk[IND(0, NYr+1, 0)],  1, planeXY, rank_left, 1, gridComm, &status[1]);

    //    Передача вниз по Z
    MPI_Sendrecv(&u_chunk[IND(0, 0, 1)], 1, planeXZ, rank_down, 2,
                 &u_chunk[IND(0, 0, NZr+1)],  1, planeXZ, rank_top, 2, gridComm, &status[2]);

    //    Передача вверх по Z
    MPI_Sendrecv(&u_chunk[IND(0, 0, NZr)], 1, planeXZ, rank_top, 3,
                 &u_chunk[IND(0, 0, 0)], 1, planeXZ, rank_down, 3, gridComm, &status[3]);

    multMV(&un_chunk, mat, u_chunk);

    tmp = u_chunk;
    u_chunk = un_chunk;
    un_chunk = tmp;

  }
  //  *******************

  if (rankP == ROOT) {
    printf("FINISH!\n\n");
    t1 = omp_get_wtime();
  }

  //        GATHER
  gather_by_block(u, u_chunk, NX, NY, NYr, NZr, 2, gridComm);

  if (rankP == ROOT) {
    double diffTime = t1 - t0;
    printf("Time -\t%.3lf\n", diffTime);
//    writeFunction1D(pathResult1D, u, NX, NY, 1, 1);
    writeFunction3D(pathResult3D, u, NX, NY, NZ, 1);

    printf("DONE!!!\n\n");
    free(u);
  }

  MPI_Type_free(&planeXY);
  MPI_Type_free(&planeXZ);

  free(un_chunk);
  free(u_chunk);
  freeSpMat(&mat);

  MPI_Finalize();
  return 0;
}
