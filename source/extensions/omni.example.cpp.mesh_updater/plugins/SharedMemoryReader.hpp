// do - 2023.12.13
// The test code for data sharing between C++ process and Python process by using shared memory
#pragma once
#include <cstring>
#include <string>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <chrono>
#define MAX_VERTS 500000
#define MAX_TRIS 500000

struct float3_t
{
  float x;
  float y;
  float z;
};

struct int3_t
{
  int x;
  int y;
  int z;
};


struct defaultMesh
{
  double capture_time;
  int num_vertices;
  int num_triangles;
  float3_t* vertices;
  int3_t* triangles;
};

class SharedMemoryReader
{
public:
  SharedMemoryReader()
  {
  }

  SharedMemoryReader(key_t key)
  {
    init(key);
  }

  ~SharedMemoryReader()
  {
    delete[] _mesh.vertices;
    delete[] _mesh.triangles;
    // Detach Shared Memory
    shmdt(_shmptr);
    // Delete Shared Memory
    shmctl(_shmid, IPC_RMID, NULL);
    // Delete Semaphore
    semctl(_semid, 0, IPC_RMID);
  }

  void init(key_t key)
  {
    _key = key;
    _size = 10*1024*1024;
    // attach shm and semaphore

    _shmid = shmget(_key, _size, IPC_CREAT | 0666);
    if (_shmid == -1)
    {
      printf("Failed to get shared memory id. Retrying...\n");
      sleep(1);
    }
    // Attach Shared Memory

    _shmptr = (char*)shmat(_shmid, NULL, 0);
    if (_shmptr == (void *)-1) {
      perror("Failed to attach shared memory id");
      // exit(1);
    }

    _semid = semget(_key, 1,IPC_CREAT | 0666);
    if (_semid == -1)
    {
      printf("Failed to get semaphore id. Retrying...\n");
      sleep(1);
    }
    semctl(_semid, 0, SETVAL, 1);

    // _shmptr = (char*)shmat(_shmid, NULL, 0);
    printf("Shared memory and Semaphore connected. KEY = %d\n", _key);

    _mesh.capture_time = 0.0f;
    _mesh.num_vertices = 0;
    _mesh.num_triangles = 0;
    _mesh.vertices = new float3_t[MAX_VERTS];
    _mesh.triangles = new int3_t[MAX_TRIS];
  }

  void LockSemaphore()
  {
    _sb = {0, -1, 0};
    if (semop(_semid, &_sb, 1) == -1)
    {
      perror("Failed to lock semaphore");
      // exit(1);
    }
  }

  void UnlockSemaphore()
  {
    _sb.sem_op = 1;
    if(semop(_semid, &_sb, 1))
    {
      perror("Failed to unlock semaphore");
      // exit(1);
    }
  }

  void ReadAndDeserializeBinary()
  {
    _mesh.capture_time = 0.0f;
    _mesh.num_vertices = 0;
    _mesh.num_triangles = 0;
    memset(_mesh.vertices, 0, MAX_VERTS * sizeof(float3_t));
    memset(_mesh.triangles, 0, MAX_TRIS * sizeof(int3_t));

    LockSemaphore();

    const char* ptr = _shmptr;

    std::memcpy(&_mesh.capture_time, ptr, sizeof(_mesh.capture_time));
    ptr += sizeof(_mesh.capture_time);

    std::memcpy(&_mesh.num_vertices, ptr, sizeof(_mesh.num_vertices));
    ptr += sizeof(_mesh.num_vertices);

    std::memcpy(&_mesh.num_triangles, ptr, sizeof(_mesh.num_triangles));
    ptr += sizeof(_mesh.num_triangles);

    printf("[Read] num_vertices: %d, num_triangles: %d\n", _mesh.num_vertices, _mesh.num_triangles);

    std::memcpy(_mesh.vertices, ptr, _mesh.num_vertices * sizeof(float3_t));
    ptr += _mesh.num_vertices * sizeof(float3_t);

    std::memcpy(_mesh.triangles, ptr, _mesh.num_triangles * sizeof(int3_t));
    ptr += _mesh.num_triangles * sizeof(int3_t);

    memset(_shmptr, 0, _size);

    UnlockSemaphore();

  }

  double GetCaptureTime()
  {
    return _mesh.capture_time;
  }

  int GetNumVertices()
  {
    return _mesh.num_vertices;
  }

  int GetNumTriangles()
  {
    return _mesh.num_triangles;
  }

  float3_t* GetVertexPtr()
  {
    return _mesh.vertices;
  }

  int3_t* GetTrianglePtr()
  {
    return _mesh.triangles;
  }


private:
  struct sembuf _sb;
  std::string _target;
  key_t _key;
  size_t _size;
  int _shmid, _semid;
  char* _shmptr;
  defaultMesh _mesh;
  std::chrono::high_resolution_clock::time_point _start,_end;

};
