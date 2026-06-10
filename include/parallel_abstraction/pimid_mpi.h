/**
 * pimid_mpi.h — MPI-compatible API for unified in-process PIM simulation
 *
 * All MPI ranks run as threads in a single process.  Message passing uses
 * shared-memory mailboxes.  When running under ZSim/QEMU, magic ops inject
 * NoC + hierarchy timing.  When running natively, timing is a no-op.
 *
 * Usage:
 *   #include <pimid_mpi.h>          // direct include
 *   // OR
 *   LD_PRELOAD=libpimid_mpi.so      // intercept system MPI
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PIMID_MPI_H_
#define PIMID_MPI_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- MPI type/op/comm definitions ---- */

typedef int MPI_Comm;
typedef int MPI_Datatype;
typedef int MPI_Op;
typedef int MPI_Status;
typedef int MPI_Request;

#define MPI_COMM_WORLD  0
#define MPI_STATUS_IGNORE ((MPI_Status*)0)
#define MPI_STATUSES_IGNORE ((MPI_Status*)0)
#define MPI_REQUEST_NULL 0

/* Datatypes */
#define MPI_CHAR           1
#define MPI_INT            2
#define MPI_FLOAT          3
#define MPI_DOUBLE         4
#define MPI_LONG           5
#define MPI_UNSIGNED       6
#define MPI_UNSIGNED_LONG  7
#define MPI_BYTE           8
#define MPI_LONG_LONG      9
#define MPI_LONG_LONG_INT  9

/* Reduction operations */
#define MPI_SUM   1
#define MPI_MAX   2
#define MPI_MIN   3
#define MPI_PROD  4
#define MPI_LAND  5
#define MPI_LOR   6
#define MPI_BAND  7
#define MPI_BOR   8

/* Any source/tag */
#define MPI_ANY_SOURCE  (-1)
#define MPI_ANY_TAG     (-1)

/* Success code */
#define MPI_SUCCESS 0

/* ---- Core MPI API ---- */

int MPI_Init(int *argc, char ***argv);
int MPI_Finalize(void);
int MPI_Comm_rank(MPI_Comm comm, int *rank);
int MPI_Comm_size(MPI_Comm comm, int *size);

/* Point-to-point */
int MPI_Send(const void *buf, int count, MPI_Datatype datatype,
             int dest, int tag, MPI_Comm comm);
int MPI_Recv(void *buf, int count, MPI_Datatype datatype,
             int source, int tag, MPI_Comm comm, MPI_Status *status);
int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 int dest, int sendtag,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 int source, int recvtag,
                 MPI_Comm comm, MPI_Status *status);

/* Non-blocking */
int MPI_Isend(const void *buf, int count, MPI_Datatype datatype,
              int dest, int tag, MPI_Comm comm, MPI_Request *request);
int MPI_Irecv(void *buf, int count, MPI_Datatype datatype,
              int source, int tag, MPI_Comm comm, MPI_Request *request);
int MPI_Wait(MPI_Request *request, MPI_Status *status);
int MPI_Waitall(int count, MPI_Request requests[], MPI_Status statuses[]);

/* Collectives */
int MPI_Barrier(MPI_Comm comm);
int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype,
              int root, MPI_Comm comm);
int MPI_Reduce(const void *sendbuf, void *recvbuf, int count,
               MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm);
int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count,
                  MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm);
int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype,
               int root, MPI_Comm comm);
int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 MPI_Comm comm);

/* Utility */
int MPI_Type_size(MPI_Datatype datatype, int *size);
double MPI_Wtime(void);

#ifdef __cplusplus
}
#endif

#endif  /* PIMID_MPI_H_ */
