#if !defined(__SUDSOLVE_H__)
#define __SUDSOLVE_H__

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef int16_t  i16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef uint64_t u64;
typedef double   f64;
typedef u8       b8;

#if !defined(TRUE)
    #define TRUE  1
    #define FALSE 0
#endif

#define ARRAY_COUNT(X) (sizeof(X) / sizeof(*(X)))

#if defined(_WIN32)
    #define THREAD_LOCAL __declspec(thread)
#elif defined(__linux__)
    #define THREAD_LOCAL __thread
#else
    #error "Bad Operating System"
#endif

#define SIZE            9
#define BOX_SIZE        3
#define MAX_ROWS        (SIZE * SIZE * SIZE)
#define MAX_COLS        (4 * SIZE * SIZE)
#define MAX_DLX_NODES   (MAX_ROWS * 4)
#define UNKNOWN_CELL    '0'

struct Node {
    Node* Left;
    Node* Right;
    Node* Up;
    Node* Down;
    Node* Column;
    u32   Count;
    i16   RowID;
    i16   ColID;
};

struct DLX {
    Node* Rows[MAX_ROWS];
    Node  Columns[MAX_COLS];
    Node  NodeArena[MAX_DLX_NODES];
    i16   Solution[SIZE * SIZE];
    Node  Head;
    u32   NodesUsedCount;
    u32   SolutionSize;
};

struct WorkOrder {
    DLX State;
    u16 InitialRowHas[SIZE] = {};
    u16 InitialColHas[SIZE] = {};
    u16 InitialBoxHas[SIZE] = {};
    char* Board;
};

struct WorkQueue {
    char* OutputBuffer;
    char* InputPuzzles;
    volatile u64 NextPuzzleIndex;
    volatile u64 PuzzlesCompleted;
    volatile u64 PuzzlesFailed;
    u32 HeaderSize;
    u32 TotalPuzzles;
};

struct FileContents {
    void* Data;
    u32 DataSize;
};

b8 SolvePuzzle(WorkQueue* Queue);

#endif // __SUDSOLVE_H__
