#if !defined(__SUDSOLVE_H__)
#define __SUDSOLVE_H__

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t  i16;
typedef u8       b8;

#if !defined(TRUE)
    #define TRUE  1
    #define FALSE 0
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

#endif // __SUDSOLVE_H__
