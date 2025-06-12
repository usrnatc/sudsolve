#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(__linux__)
    #include <time.h>
    #include <sys/time.h>
    #include <sched.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/mman.h>
#endif

#include "sudsolve.h"

THREAD_LOCAL static WorkOrder* LocalWorkOrder = NULL;

void inline
MemCpy(void* _Dst, const void* _Src, u64 N)
{
    u8* Dst = (u8*) _Dst;
    const u8* Src = (const u8*) _Src;

    for (u64 I = 0; I < N; ++I)
        Dst[I] = Src[I];
}

void inline
MemSet(void* _Dst, const u8 Val, u64 N)
{
    u8* Dst = (u8*) _Dst;

    for (u64 I = 0; I < N; ++I)
        Dst[I] = Val;
}


void inline
PrintBoard(char* Board)
{
    printf("+---+---+---+---+---+---+---+---+---+\n");
    for (u8 I = 0; I < SIZE; ++I) {
        for (u8 J = 0; J < SIZE; ++J) {
            u8 Char;

            if (Board[I * SIZE + J] == UNKNOWN_CELL)
                Char = ' ';
            else
                Char = Board[I * SIZE + J];

            printf("| %c ", Char);
        }
        printf("|\n+---+---+---+---+---+---+---+---+---+\n");
    }
    printf("\n");
}

#if defined(_WIN32)

void
FreeFileData(FileContents* Contents)
{
    if (Contents && Contents->Data)
        VirtualFree(Contents->Data, 0, MEM_RELEASE);
}

FileContents
ReadEntireFile(char* FileName)
{
    FileContents Result = {};
    HANDLE FileHandle = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (FileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER FileSize;

        if (GetFileSizeEx(FileHandle, &FileSize)) {
            u32 FileSize32 = (u32) FileSize.QuadPart;

            Result.Data = VirtualAlloc(NULL, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            if (Result.Data) {
                u32 BytesRead;

                if (ReadFile(FileHandle, Result.Data, FileSize32, (LPDWORD) &BytesRead, NULL)) {
                    if (FileSize32 == BytesRead) {
                        Result.DataSize = FileSize32;
                    } else {
                        printf("[ERROR] Failed to read entire file, truncated read\n");
                        FreeFileData(&Result);
                        Result.Data = NULL;
                    }
                } else {
                    printf("[ERROR] Failed to read file\n");
                    FreeFileData(&Result);
                    Result.Data = NULL;
                }
            } else {
                printf("[ERROR] Failed to allocate memory for file\n");
            }
        } else {
            printf("[ERROR] Failed to get file size\n");
        }

        CloseHandle(FileHandle);
    } else {
        printf("[ERROR] Failed to create file for reading\n");
    }

    return Result;
}

b8
WriteEntireFile(char* FileName, void* Data, u32 DataSize)
{
    b8 Result = FALSE;
    HANDLE FileHandle = CreateFile(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);

    if (FileHandle != INVALID_HANDLE_VALUE) {
        u32 BytesWritten;

        if (WriteFile(FileHandle, Data, DataSize, (LPDWORD) &BytesWritten, NULL)) {
            if (DataSize == BytesWritten)
                Result = TRUE;
            else
                printf("[ERROR] Failed to write entire file, truncated write\n");
        } else {
            printf("[ERROR] Failed to write file\n");
        }

        CloseHandle(FileHandle);
    } else {
        printf("[ERROR] Failed to create file for writing\n");
    }

    return Result;
}

u64
LockedAddAndReturnPreviousValue(u64 volatile* Value, u64 Delta)
{
    u64 Result = InterlockedExchangeAdd64((volatile LONG64*) Value, Delta);

    return Result;
}

DWORD WINAPI
WorkerThread(void* Parameter)
{
    WorkQueue* Queue = (WorkQueue*) Parameter;

    while (SolvePuzzle(Queue)) {}

    if (LocalWorkOrder) {
        free(LocalWorkOrder);
        LocalWorkOrder = NULL;
    }

    return 0;
}

static void
CreateWorkThread(void* Parameter)
{
    DWORD ThreadID;
    HANDLE ThreadHandle = CreateThread(NULL, 0, WorkerThread, Parameter, 0, &ThreadID);

    CloseHandle(ThreadHandle);
}

static u32
GetCPUCoreCount(void)
{
    SYSTEM_INFO Info;

    GetSystemInfo(&Info);

    return Info.dwNumberOfProcessors;
}

f64 inline
GetWallTime(void)
{
    LARGE_INTEGER Time;
    LARGE_INTEGER Freq;

    if (!QueryPerformanceFrequency(&Freq))
        return 0.0;

    if (!QueryPerformanceCounter(&Time))
        return 0.0;

    return ((f64) Time.QuadPart / Freq.QuadPart) * 1000.0;
}

#elif defined(__linux__)

void
FreeFileData(FileContents* Contents)
{
    if (Contents && Contents->Data)
        munmap(Contents->Data, Contents->DataSize);
}

FileContents
ReadEntireFile(char* FileName)
{
    FileContents Result = {};
    i32 FileDescriptor = open(FileName, O_RDONLY);

    if (FileDescriptor != -1) {
        struct stat StatBuf = {};
        i32 StatBufResult = fstat(FileDescriptor, &StatBuf);

        if (StatBufResult != -1) {
            u32 FileSize32 = (u32) StatBuf.st_size;

            Result.Data = mmap(NULL, FileSize32, PROT_READ, MAP_PRIVATE, FileDescriptor, 0);

            if (Result.Data != MAP_FAILED) {
                Result.DataSize = FileSize32;
                madvise(Result.Data, Result.DataSize, MADV_WILLNEED);
            } else {
                printf("[ERROR] Failed to map file\n");
            }
        } else {
            printf("[ERROR] Failed to get file size\n");
        }

        close(FileDescriptor);
    } else {
        printf("[ERROR] Failed to open file for reading\n");
    }

    return Result;
}

b8
WriteEntireFile(char* FileName, void* Data, u32 DataSize)
{
    b8 Result = FALSE;
    i32 FileDescriptor = open(FileName, O_WRONLY | O_CREAT, 0777);

    if (FileDescriptor > 0) {
        i32 BytesWritten = write(FileDescriptor, Data, DataSize);

        if (BytesWritten == DataSize)
            Result = TRUE;
        else
            printf("[ERROR] Failed to write entire file, truncated write\n");

        close(FileDescriptor);
    } else {
        printf("[ERROR] Failed to open file for writing\n");
    }

    return Result;
}

u64
LockedAddAndReturnPreviousValue(u64 volatile* Value, u64 Delta)
{
    u64 Result = __sync_fetch_and_add(Value, Delta);

    return Result;
}

void*
WorkerThread(void* Parameter)
{
    WorkQueue* Queue = (WorkQueue*) Parameter;

    while (SolvePuzzle(Queue)) {}

    if (LocalWorkOrder) {
        free(LocalWorkOrder);
        LocalWorkOrder = NULL;
    }

    return NULL;
}

static void
CreateWorkThread(void* Parameter)
{
    pthread_t ThreadID;

    pthread_create(&ThreadID, NULL, WorkerThread, Parameter);
}

static u32
GetCPUCoreCount(void)
{
    cpu_set_t CPUSet;

    sched_getaffinity(0, sizeof(CPUSet), &CPUSet);

    return CPU_COUNT(&CPUSet);
}

f64 inline
GetWallTime(void)
{
    struct timeval Time;

    if (gettimeofday(&Time, NULL))
        return 0.0;

    return ((f64) Time.tv_sec + ((f64) Time.tv_usec * 0.000001)) * 1000.0;
}

#else
    #error "Bad Operating System"
#endif

#define BEGIN_TIMER(Name) f64 StartTime ## Name = GetWallTime();
#define END_TIMER(Name) f64 EndTime ## Name = GetWallTime(); f64 TotalTime ## Name = EndTime ## Name - StartTime ## Name

void 
AddRow(DLX *Dlx, u16 RowIdx, u16 ColIdx[4]) 
{
    if (Dlx->NodesUsedCount + 4 > MAX_DLX_NODES)
        return;

    u32 BaseIdx = Dlx->NodesUsedCount;
    Dlx->NodesUsedCount += 4;

    for (u8 K = 0; K < 4; ++K) {
        Node* NewNode = &Dlx->NodeArena[BaseIdx + K];

        NewNode->RowID = RowIdx;
        NewNode->ColID = ColIdx[K];
        NewNode->Column = &Dlx->Columns[ColIdx[K]];
        NewNode->Down = NewNode->Column;
        NewNode->Up = NewNode->Column->Up;
        NewNode->Column->Up->Down = NewNode;
        NewNode->Column->Up = NewNode;
        ++NewNode->Column->Count;
    }

    for (u8 K = 0; K < 4; ++K) {
        Node* HNode = &Dlx->NodeArena[BaseIdx + K];

        HNode->Left = &Dlx->NodeArena[BaseIdx + (K + 3) % 4];
        HNode->Right = &Dlx->NodeArena[BaseIdx + (K + 1) % 4];
    }

    Dlx->Rows[RowIdx] = &Dlx->NodeArena[BaseIdx];
}

void
Cover(Node *ColHeader) 
{
    ColHeader->Right->Left = ColHeader->Left;
    ColHeader->Left->Right = ColHeader->Right;

    for (Node *RowNode = ColHeader->Down; RowNode != ColHeader; RowNode = RowNode->Down) {
        for (Node *HNode = RowNode->Right; HNode != RowNode; HNode = HNode->Right) {
            HNode->Down->Up = HNode->Up;
            HNode->Up->Down = HNode->Down;
            --HNode->Column->Count;
        }
    }
}

void
Uncover(Node *ColHeader) 
{
    for (Node *RowNode = ColHeader->Up; RowNode != ColHeader; RowNode = RowNode->Up) {
        for (Node *HNode = RowNode->Left; HNode != RowNode; HNode = HNode->Left) {
            HNode->Down->Up = HNode;
            HNode->Up->Down = HNode;
            ++HNode->Column->Count;
        }
    }

    ColHeader->Right->Left = ColHeader;
    ColHeader->Left->Right = ColHeader;
}

u8 inline
IsCandidateValid(
    u16 R, 
    u16 C, 
    u16 Num, 
    u16* InitialRowHas,
    u16* InitialColHas,
    u16* InitialBoxHas
) {
    u32 BoxIdx = (R / BOX_SIZE) * BOX_SIZE + (C / BOX_SIZE);
    u16 Mask = (1 << Num);

    if (InitialRowHas[R] & Mask)
        return FALSE;

    if (InitialColHas[C] & Mask)
        return FALSE;

    if (InitialBoxHas[BoxIdx] & Mask)
        return FALSE;

    return TRUE;
}

u8
Solve(DLX *Dlx) 
{
    if (Dlx->Head.Right == &Dlx->Head)
        return TRUE;

    Node *ColToCover = Dlx->Head.Right;

    if (ColToCover->Count > 1) {
        for (Node *TempCol = ColToCover->Right; TempCol != &Dlx->Head; TempCol = TempCol->Right) {
            if (TempCol->Count < ColToCover->Count) {
                ColToCover = TempCol;

                if (ColToCover->Count <= 1)
                    break;
            }
        }
    }
    
    if (!ColToCover->Count)
        return FALSE;

    Cover(ColToCover);

    for (Node *RowNode = ColToCover->Down; RowNode != ColToCover; RowNode = RowNode->Down) {
        Dlx->Solution[Dlx->SolutionSize++] = RowNode->RowID;

        for (Node *HNode = RowNode->Right; HNode != RowNode; HNode = HNode->Right)
            Cover(HNode->Column);

        if (Solve(Dlx))
            return TRUE;

        --Dlx->SolutionSize;

        for (Node *HNode = RowNode->Left; HNode != RowNode; HNode = HNode->Left)
            Uncover(HNode->Column);
    }

    Uncover(ColToCover);

    return FALSE;
}

b8
SolvePuzzle(WorkQueue* Queue)
{
    u64 PuzzleIdx = LockedAddAndReturnPreviousValue(&Queue->NextPuzzleIndex, 1);

    if (PuzzleIdx >= Queue->TotalPuzzles)
        return FALSE;

    if (!LocalWorkOrder)
        LocalWorkOrder = (WorkOrder*) malloc(sizeof(WorkOrder));

    WorkOrder* Order = LocalWorkOrder;

    MemSet(Order, 0, sizeof(WorkOrder));

    DLX* Dlx = &Order->State;

    Dlx->Head.Left = &Dlx->Head;
    Dlx->Head.Right = &Dlx->Head;
    Dlx->Head.Up = &Dlx->Head;
    Dlx->Head.Down = &Dlx->Head;
    Dlx->Head.ColID = -1;

    for (u16 I = 0; I < MAX_COLS; ++I) {
        Node* ColNode = &Dlx->Columns[I];

        ColNode->ColID = I;
        ColNode->Up = ColNode;
        ColNode->Down = ColNode;
        ColNode->Left = Dlx->Head.Left;
        ColNode->Right = &Dlx->Head;
        Dlx->Head.Left->Right = ColNode;
        Dlx->Head.Left = ColNode;
        ColNode->Count = 0;
    }

    char* InputPuzzle = Queue->InputPuzzles + (PuzzleIdx * 82);
    u32 OutputOffs = Queue->HeaderSize + (PuzzleIdx * 164) + 82;

    Order->Board = Queue->OutputBuffer + OutputOffs;
    MemCpy(Order->Board, InputPuzzle, SIZE * SIZE);

    for (u8 RInit = 0; RInit < SIZE; ++RInit) {
        for (u8 CInit = 0; CInit < SIZE; ++CInit) {
            if (Order->Board[RInit * SIZE + CInit] != UNKNOWN_CELL) {
                u8 Val = Order->Board[RInit * SIZE + CInit] - '0';
                u8 BoxIdx = (RInit / BOX_SIZE) * BOX_SIZE + (CInit / BOX_SIZE);

                Order->InitialRowHas[RInit] |= (1 << Val);
                Order->InitialColHas[CInit] |= (1 << Val);
                Order->InitialBoxHas[BoxIdx] |= (1 << Val);
            }
        }
    }

    for (u8 R = 0; R < SIZE; ++R) {
        for (u8 C = 0; C < SIZE; ++C) {
            if (Order->Board[R * SIZE + C] == UNKNOWN_CELL) {
                for (u8 Num = 1; Num <= SIZE; Num++) {
                    if (!IsCandidateValid(R, C, Num, Order->InitialRowHas, Order->InitialColHas, Order->InitialBoxHas))
                        continue;

                    u16 RowIdx = R * SIZE * SIZE + C * SIZE + (Num - 1);
                    u16 BoxIdx = (R / BOX_SIZE) * BOX_SIZE + (C / BOX_SIZE);
                    u16 ColIdx[4] = {
                        (u16) (R * SIZE + C),
                        (u16) (SIZE * SIZE + R * SIZE + (Num - 1)),
                        (u16) (2 * SIZE * SIZE + C * SIZE + (Num - 1)),
                        (u16) (3 * SIZE * SIZE + BoxIdx * SIZE + (Num - 1))
                    };

                    AddRow(Dlx, RowIdx, ColIdx);
                }
            } else {
                u8 Num = Order->Board[R * SIZE + C] - '0';
                u16 RowIdx = R * SIZE * SIZE + C * SIZE + (Num - 1);
                u16 BoxIdx = (R / BOX_SIZE) * BOX_SIZE + (C / BOX_SIZE);
                u16 ColIdx[4] = {
                    (u16) (R * SIZE + C),
                    (u16) (SIZE * SIZE + R * SIZE + (Num - 1)),
                    (u16) (2 * SIZE * SIZE + C * SIZE + (Num - 1)),
                    (u16) (3 * SIZE * SIZE + BoxIdx * SIZE + (Num - 1))
                };

                AddRow(Dlx, RowIdx, ColIdx);

                Node *ChosenRowNode = Dlx->Rows[RowIdx];
                
                if (!ChosenRowNode)
                    continue;

                Dlx->Solution[Dlx->SolutionSize++] = ChosenRowNode->RowID;

                if (ChosenRowNode->Column->Right->Left == ChosenRowNode->Column)
                     Cover(ChosenRowNode->Column);

                for (Node *NodeInRow = ChosenRowNode->Right; NodeInRow != ChosenRowNode; NodeInRow = NodeInRow->Right) {
                    if (NodeInRow->Column->Right->Left == NodeInRow->Column)
                        Cover(NodeInRow->Column);
                }
            }
        }
    }

    if (Solve(Dlx)) {
        for (u32 K = 0; K < Dlx->SolutionSize; ++K) {
            u16 RowID = Dlx->Solution[K];
            u8 Num = (RowID % SIZE) + 1;

            RowID /= SIZE;

            u16 J = RowID % SIZE;

            RowID /= SIZE;

            u16 I = RowID;

            Order->Board[I * SIZE + J] = Num + '0';
        }
    } else {
        LockedAddAndReturnPreviousValue(&Queue->PuzzlesFailed, 1);
    }

    LockedAddAndReturnPreviousValue(&Queue->PuzzlesCompleted, 1);

    return TRUE;
}

int
main(int ArgC, char** ArgV)
{
    if (ArgC != 2) {
        printf("Usage: ./sudsolve <sudoku_file>\n");

        return 1;
    }

    u32 CoreCount = GetCPUCoreCount();
    FileContents PuzzleInput = ReadEntireFile(ArgV[1]);
    char* PuzzleInputData = (char*) PuzzleInput.Data;
    u32 P = 0;
    u8 LineEndSize = 1;

    while (PuzzleInputData[P] != '\n')
        ++P;

    if (P > 0 && PuzzleInputData[P - 1] == '\r')
        LineEndSize = 2;

    ++P;

    u32 PuzzleDataSize = PuzzleInput.DataSize - P;
    u32 LineLengthPerPuzzle = 81 + LineEndSize;
    u32 TotalPuzzles = (PuzzleDataSize + LineLengthPerPuzzle - 1) / LineLengthPerPuzzle;
    u32 OutputSize = P + TotalPuzzles * 164;
    char* Output = (char*) malloc(OutputSize + 2);

    MemCpy(Output, PuzzleInputData, P);

    u32 OutOffs = P;

    for (u32 I = 0; I < TotalPuzzles; ++I) {
        u32 InOffs = P + I * LineLengthPerPuzzle;

        MemCpy(Output + OutOffs, PuzzleInputData + InOffs, 81);
        Output[OutOffs + 81] = ',';
        MemCpy(Output + OutOffs + 82, PuzzleInputData + InOffs, 81);
        Output[OutOffs + 163] = '\n';
        OutOffs += 164;
    }

    WorkQueue Queue = {};

    Queue.OutputBuffer = Output;
    Queue.InputPuzzles = PuzzleInputData + P;
    Queue.HeaderSize = P;
    Queue.TotalPuzzles = TotalPuzzles;
    Queue.NextPuzzleIndex = 0;
    Queue.PuzzlesCompleted = 0;
    Queue.PuzzlesFailed = 0;

    LockedAddAndReturnPreviousValue(&Queue.NextPuzzleIndex, 0);
    BEGIN_TIMER(SolvePuzzles);

    for (u32 CoreIndex = 0; CoreIndex < (CoreCount - 1); ++CoreIndex)
        CreateWorkThread(&Queue);

    while (Queue.PuzzlesCompleted < TotalPuzzles) {
        if (SolvePuzzle(&Queue)) {
            u32 Progress = 100 * (u32) Queue.PuzzlesCompleted / TotalPuzzles;

            printf("\r[INFO]\t%3d%% complete", Progress);
            fflush(stdout);
        }
    }

    END_TIMER(SolvePuzzles);
    printf(
        "\r[INFO]\t100%% complete\n\n"
        "**********************************************************\n"
        "** STATISTICS\n"
        "**********************************************************\n"
        "** Failed                :  %15llu puzzles\n" 
        "** Solved                :  %15llu puzzles\n" 
        "** TotalTimeSolvePuzzles : ~%15f ms\n" 
        "** TimePerPuzzle         : ~%15f ms\n" 
        "**********************************************************\n\n",
        Queue.PuzzlesFailed,
        TotalPuzzles - Queue.PuzzlesFailed,
        TotalTimeSolvePuzzles,
        ((f64) TotalTimeSolvePuzzles / TotalPuzzles)
    );

    if (!WriteEntireFile((char*) "./PuzzleOutput.txt", Output, OutOffs)) {
        free(Output);
        FreeFileData(&PuzzleInput);

        return 2;
    }

    free(Output);
    FreeFileData(&PuzzleInput);

    return 0;
}
