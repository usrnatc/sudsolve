#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t i16;
typedef u8 b8;

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
    u32 Count;
    i16 RowID;
    i16 ColID;
};

struct DLX {
    Node* Rows[MAX_ROWS];
    Node Columns[MAX_COLS];
    Node NodeArena[MAX_DLX_NODES];
    i16 Solution[SIZE * SIZE];
    Node Head;
    u32 NodesUsedCount;
    u32 SolutionSize;
};


void 
AddNode(DLX *Dlx, u16 RowIdx, u16 ColIdx) 
{
    if (Dlx->NodesUsedCount >= MAX_DLX_NODES)
        return;

    Node* NewNode = &Dlx->NodeArena[Dlx->NodesUsedCount++];

    NewNode->RowID = RowIdx;
    NewNode->ColID = ColIdx;
    NewNode->Column = &Dlx->Columns[ColIdx];
    ++Dlx->Columns[ColIdx].Count;
    NewNode->Down = &Dlx->Columns[ColIdx];
    NewNode->Up = Dlx->Columns[ColIdx].Up;
    Dlx->Columns[ColIdx].Up->Down = NewNode;
    Dlx->Columns[ColIdx].Up = NewNode;

    if (!Dlx->Rows[RowIdx]) {
        NewNode->Left = NewNode;
        NewNode->Right = NewNode;
        Dlx->Rows[RowIdx] = NewNode;
    } else {
        NewNode->Left = Dlx->Rows[RowIdx]->Left;
        NewNode->Right = Dlx->Rows[RowIdx];
        Dlx->Rows[RowIdx]->Left->Right = NewNode;
        Dlx->Rows[RowIdx]->Left = NewNode;
    }
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
    b8* InitialRowHas,
    b8* InitialColHas,
    b8* InitialBoxHas
) {
    int BoxIndex = (R / BOX_SIZE) * BOX_SIZE + (C / BOX_SIZE);

    if (InitialRowHas[R * (SIZE + 1) + Num])
        return FALSE;

    if (InitialColHas[C * (SIZE + 1) + Num])
        return FALSE;

    if (InitialBoxHas[BoxIndex * (SIZE + 1) + Num])
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

        Dlx->SolutionSize--;

        for (Node *HNode = RowNode->Left; HNode != RowNode; HNode = HNode->Left)
            Uncover(HNode->Column);
    }

    Uncover(ColToCover);

    return FALSE;
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

int
main(int ArgC, char** ArgV)
{
    if (ArgC != 2) {
        printf("Usage: ./sudsolve <sudoku_file>\n");

        return 1;
    }

    FILE* PuzzleFile = fopen(ArgV[1], "rb");

    if (!PuzzleFile) {
        printf("Could not open file \"%s\"\n", ArgV[1]);

        return 2;
    }

    fseek(PuzzleFile, 0, SEEK_END);

    u32 PuzzleInputSize = ftell(PuzzleFile);

    fseek(PuzzleFile, 0, SEEK_SET);

    char* String = (char*) malloc(PuzzleInputSize + 1);

    fread(String, 1, PuzzleInputSize, PuzzleFile);
    fclose(PuzzleFile);

    u32 P = 0;

    while (String[P] != '\n')
        ++P;
    ++P;

    u32 TotalPuzzles = (u32) ((PuzzleInputSize - P) / 82);
    u32 OutputSize = P + TotalPuzzles * 164;
    char* Output = (char*) malloc(OutputSize + 2);

    memcpy(Output, String, P);

    u32 InOffs = P;
    u32 OutOffs = P;

    for (u32 I = 0; I <= TotalPuzzles; ++I) {
        memcpy(Output + OutOffs, String + InOffs, 81);
        OutOffs += 81;
        Output[OutOffs++] = ',';
        memcpy(Output + OutOffs, String + InOffs, 81);

        char* Board = Output + OutOffs;
        b8 InitialRowHas[SIZE * (SIZE + 1)] = {};
        b8 InitialColHas[SIZE * (SIZE + 1)] = {};
        b8 InitialBoxHas[SIZE * (SIZE + 1)] = {};
        DLX Dlx = {};

        Dlx.Head.Left = &Dlx.Head;
        Dlx.Head.Right = &Dlx.Head;
        Dlx.Head.Up = &Dlx.Head;
        Dlx.Head.Down = &Dlx.Head;
        Dlx.Head.ColID = -1;

        for (u16 I = 0; I < MAX_COLS; ++I) {
            Node *ColNode = &Dlx.Columns[I];

            ColNode->ColID = I;
            ColNode->Up = ColNode;
            ColNode->Down = ColNode;
            ColNode->Left = Dlx.Head.Left;
            ColNode->Right = &Dlx.Head;
            Dlx.Head.Left->Right = ColNode;
            Dlx.Head.Left = ColNode;
        }

        for (u8 RInit = 0; RInit < SIZE; ++RInit) {
            for (u8 CInit = 0; CInit < SIZE; ++CInit) {
                if (Board[RInit * SIZE + CInit] != UNKNOWN_CELL) {
                    u8 Val = Board[RInit * SIZE + CInit] - '0';
                    u8 BoxIdx = (RInit / BOX_SIZE) * BOX_SIZE + (CInit / BOX_SIZE);

                    InitialRowHas[RInit * (SIZE + 1) + Val] = TRUE;
                    InitialColHas[CInit * (SIZE + 1) + Val] = TRUE;
                    InitialBoxHas[BoxIdx * (SIZE + 1) + Val] = TRUE;
                }
            }
        }

        for (u8 I = 0; I < SIZE; ++I) {
            for (u8 J = 0; J < SIZE; ++J) {
                if (Board[I * SIZE + J] == UNKNOWN_CELL) {
                    for (u8 Num = 1; Num <= SIZE; Num++) {
                        if (!IsCandidateValid(I, J, Num, InitialRowHas, InitialColHas, InitialBoxHas))
                            continue;

                        u16 RowIdx = I * SIZE * SIZE + J * SIZE + (Num - 1);
                        u16 BoxIdx = (I / BOX_SIZE) * BOX_SIZE + (J / BOX_SIZE);
                        u16 C1CellPos = I * SIZE + J;
                        u16 C2RowNum  = SIZE * SIZE + I * SIZE + (Num - 1);
                        u16 C3ColNum  = 2 * SIZE * SIZE + J * SIZE + (Num - 1);
                        u16 C4BoxNum  = 3 * SIZE * SIZE + BoxIdx * SIZE + (Num - 1);
                        
                        AddNode(&Dlx, RowIdx, C1CellPos);
                        AddNode(&Dlx, RowIdx, C2RowNum);
                        AddNode(&Dlx, RowIdx, C3ColNum);
                        AddNode(&Dlx, RowIdx, C4BoxNum);
                    }
                } else {
                    u8 Num = Board[I * SIZE + J] - '0';
                    u16 RowIdx = I * SIZE * SIZE + J * SIZE + (Num - 1);
                    u16 BoxIdx = (I / BOX_SIZE) * BOX_SIZE + (J / BOX_SIZE);
                    u16 C1CellPos = I * SIZE + J;
                    u16 C2RowNum  = SIZE * SIZE + I * SIZE + (Num - 1);
                    u16 C3ColNum  = 2 * SIZE * SIZE + J * SIZE + (Num - 1);
                    u16 C4BoxNum  = 3 * SIZE * SIZE + BoxIdx * SIZE + (Num - 1);

                    AddNode(&Dlx, RowIdx, C1CellPos);
                    AddNode(&Dlx, RowIdx, C2RowNum);
                    AddNode(&Dlx, RowIdx, C3ColNum);
                    AddNode(&Dlx, RowIdx, C4BoxNum);
                }
            }
        }

        for (u8 R = 0; R < SIZE; ++R) {
            for (u8 C = 0; C < SIZE; ++C) {
                if (Board[R * SIZE + C] != UNKNOWN_CELL) {
                    u8 Num = Board[R * SIZE + C] - '0';
                    u16 RowIdx = R * SIZE * SIZE + C * SIZE + (Num - 1);
                    Node *ChosenRowNode = Dlx.Rows[RowIdx];
                    
                    if (!ChosenRowNode)
                        continue;

                    Dlx.Solution[Dlx.SolutionSize++] = ChosenRowNode->RowID;

                    if (ChosenRowNode->Column->Right->Left == ChosenRowNode->Column)
                         Cover(ChosenRowNode->Column);

                    for (Node *NodeInRow = ChosenRowNode->Right; NodeInRow != ChosenRowNode; NodeInRow = NodeInRow->Right) {
                        if (NodeInRow->Column->Right->Left == NodeInRow->Column)
                            Cover(NodeInRow->Column);
                    }
                }
            }
        }
        
        if (Solve(&Dlx)) {
            for (u32 K = 0; K < Dlx.SolutionSize; ++K) {
                u16 RowID = Dlx.Solution[K];
                u8 Num = (RowID % SIZE) + 1;

                RowID /= SIZE;

                u16 J = RowID % SIZE;

                RowID /= SIZE;

                u16 I = RowID;

                Board[I * SIZE + J] = Num + '0';
            }
        }

        OutOffs += 81;
        Output[OutOffs++] = '\n';
        InOffs += 82;
    }

    PuzzleFile = fopen("./PuzzleOutput.txt", "wb");

    if (!PuzzleFile) {
        printf("Could not open \"PuzzleOutput.txt\"\n");
        free(String);
        free(Output);

        return 3;
    }

    fwrite(Output, 1, OutOffs, PuzzleFile);
    fclose(PuzzleFile);
    free(String);
    free(Output);

    return 0;
}
