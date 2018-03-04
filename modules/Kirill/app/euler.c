//
// Created by kirill on 7.03.17.
//

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#include "multMV.h"
#include "createSpMat.h"
#include "parser.h"
#include "sgpu.h"

#define DIM_CART 2
#define SHIFT 1
#define RESERVE SHIFT*2

#define IND(x,y,z) ((x) + (y)*NX + (z)*NX*(NYr + RESERVE))

int main(int argc, char **argv) {
  int sizeP, rankP;

  size_t sizeTime;

  MPI_Status status[4];
  double t0 = 0.0, t1 = 0.0;

  int blockYP = 0, blockZP = 0;

  MPI_Init(&argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD, &sizeP);
  MPI_Comm_rank(MPI_COMM_WORLD, &rankP);

  char nameHost[40];
  gethostname(nameHost, 40);
  printf("rank - %d name host - %s\n",rankP, nameHost);

  SpMatrix mat;
  int dim;
  TYPE coeffs[4];
  TYPE* u = NULL, *u_chunk = NULL, *un_chunk;

  int NX, NY, NZ, NYr, NZr;

  if (rankP == ROOT) {
    int error;
    Setting setting;
    error = readSetting(INPUT_EULER_SETTING_PATH, &setting);

    if (error != OK) return error;

    dim = (setting.NX+RESERVE)*(setting.NY+RESERVE)*(setting.NZ+RESERVE);
    u = (TYPE *)calloc(dim, sizeof(TYPE));

    error = readFunction(INPUT_EULER_FUNCTION_PATH, u, setting.NX+RESERVE,
                         setting.NY+RESERVE, setting.NZ+RESERVE, SHIFT);

    if (error != OK) return error;

    sizeTime = (size_t) ((setting.TFINISH - setting.TSTART) / setting.dt);

    #if ENABLE_PARALLEL
      printf("PARALLEL VERSION 2.0!\n");
      omp_set_num_threads(2);
    #endif

    printf("TimeSize -\t%lu\n", sizeTime);

    TYPE dx = ABS(setting.XSTART - setting.XEND) / setting.NX;
    TYPE dy = ABS(setting.YSTART - setting.YEND) / setting.NY;
    TYPE dz = ABS(setting.ZSTART - setting.ZEND) / setting.NZ;

    coeffs[0] = 1.f - 2.f*setting.dt*setting.SIGMA*(1.f/(dx*dx) + 1.f/(dy*dy) + 1.f/(dz*dz));
    coeffs[1] = setting.dt * setting.SIGMA / (dx * dx);
    coeffs[2] = setting.dt * setting.SIGMA / (dy * dy);
    coeffs[3] = setting.dt * setting.SIGMA / (dz * dz);
//    coeffs[0] = 1;
//    coeffs[1] = 2;
//    coeffs[2] = 3;
//    coeffs[3] = 4;

    NX = setting.NX + RESERVE;
    NY = setting.NY + RESERVE;
    NZ = setting.NZ + RESERVE;
  }

  MPI_Bcast(&sizeTime, 1, MPI_UNSIGNED_LONG, ROOT, MPI_COMM_WORLD);
  MPI_Bcast(coeffs, 4, MPI_TYPE, ROOT, MPI_COMM_WORLD);
  MPI_Bcast(&NX, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
  MPI_Bcast(&NY, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
  MPI_Bcast(&NZ, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

  //  Определения числа процессов в каждом измерении
  get_blocks(&blockYP, &blockZP, sizeP);

//  if (rankP == ROOT) printf("blockY %d blockZ %d\n", blockYP, blockZP);

  NYr = (NY - RESERVE)/blockYP;
  NZr = (NZ - RESERVE)/blockZP;

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

  int dimChunk = NX*(NYr+RESERVE)*(NZr+RESERVE);
  u_chunk = (TYPE *)aligned_alloc(ALIGNMENT, sizeof(TYPE)*dimChunk);
  un_chunk = (TYPE *)aligned_alloc(ALIGNMENT, sizeof(TYPE)*dimChunk);

  TYPE *tmp;

  // SCATTER
  scatter_by_block(u, u_chunk, NX, NY, NYr, NZr, gridComm, RESERVE);

  int nonZero = dimChunk*NR;

  initSpMat(&mat, nonZero, dimChunk);
  createExplicitSpMatV2(&mat, coeffs, NX, NYr + RESERVE, NZr + RESERVE);

  int rank_left, rank_right, rank_down, rank_top;
  MPI_Cart_shift(gridComm, 1, -1, &rank_left, &rank_right);
  MPI_Cart_shift(gridComm, 0, 1, &rank_down, &rank_top);


//   printf("rank - %d; left %d; right %d; top %d; down %d\n", rankP, rank_left, rank_right, rank_top, rank_down);

  // Создание типа плоскости XY и XZ
  MPI_Datatype planeXY;
  MPI_Type_vector(NZr+RESERVE, NX, NX*(NYr+RESERVE), MPI_TYPE, &planeXY);
  MPI_Type_commit(&planeXY);

  MPI_Datatype planeXZ;
  MPI_Type_contiguous(NX*(NYr+RESERVE), MPI_TYPE, &planeXZ);
  MPI_Type_commit(&planeXZ);
  // *****************************

  copyingBorders(u_chunk, NX, NYr+RESERVE, NZr+RESERVE);

  if (rankP == ROOT) {
    printf("START!\n");
    t0 = MPI_Wtime();
  }
#if FPGA_RUN || CPUGPU_RUN
  multMV_altera(un_chunk, mat, u_chunk,sizeTime);
  tmp = u_chunk;
  u_chunk = un_chunk;
  un_chunk = tmp;
#else

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


    multMV(un_chunk, mat, u_chunk, NX, (NYr+RESERVE), (NZr+RESERVE), coeffs);
    tmp = u_chunk;
    u_chunk = un_chunk;
    un_chunk = tmp;

  }
  //  *******************
#endif

  if (rankP == ROOT) {
    printf("FINISH!\n\n");
    t1 = MPI_Wtime();
  }

  // GATHER
  gather_by_block(u, u_chunk, NX, NY, NYr, NZr, RESERVE, gridComm);

  if (rankP == ROOT) {
    double diffTime = t1 - t0;
    printf("Time -\t%.3lf\n", diffTime);
    writeFunction3D(RESULT_EULER_PATH, u, NX, NY, NZ, SHIFT);

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
