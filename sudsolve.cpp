#include <stdio.h>
#include <stdlib.h>

#include "sudsolve.h"

void inline
MemCpy(void* _Dst, const void* _Src, u64 N)
{
    u8* Dst = (u8*) _Dst;
    const u8* Src = (const u8*) _Src;

    for (u64 I = 0; I < N; ++I)
        Dst[I] = Src[I];
}

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

    u32 PuzzleDataSize = PuzzleInputSize - P;
    u32 TotalPuzzles = (PuzzleDataSize + 81) / 82;
    u32 OutputSize = P + TotalPuzzles * 164;
    char* Output = (char*) malloc(OutputSize + 2);

    MemCpy(Output, String, P);

    u32 InOffs = P;
    u32 OutOffs = P;

    for (u32 I = 0; I < TotalPuzzles; ++I) {
        MemCpy(Output + OutOffs, String + InOffs, 81);
        OutOffs += 81;
        Output[OutOffs++] = ',';
        MemCpy(Output + OutOffs, String + InOffs, 81);

        char* Board = Output + OutOffs;
        u16 InitialRowHas[SIZE] = {};
        u16 InitialColHas[SIZE] = {};
        u16 InitialBoxHas[SIZE] = {};
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

                    InitialRowHas[RInit] |= (1 << Val);
                    InitialColHas[CInit] |= (1 << Val);
                    InitialBoxHas[BoxIdx] |= (1 << Val);
                }
            }
        }

        for (u8 R = 0; R < SIZE; ++R) {
            for (u8 C = 0; C < SIZE; ++C) {
                if (Board[R * SIZE + C] == UNKNOWN_CELL) {
                    for (u8 Num = 1; Num <= SIZE; Num++) {
                        if (!IsCandidateValid(R, C, Num, InitialRowHas, InitialColHas, InitialBoxHas))
                            continue;

                        u16 RowIdx = R * SIZE * SIZE + C * SIZE + (Num - 1);
                        u16 BoxIdx = (R / BOX_SIZE) * BOX_SIZE + (C / BOX_SIZE);
                        u16 ColIdx[4] = {
                            (u16) (R * SIZE + C),
                            (u16) (SIZE * SIZE + R * SIZE + (Num - 1)),
                            (u16) (2 * SIZE * SIZE + C * SIZE + (Num - 1)),
                            (u16) (3 * SIZE * SIZE + BoxIdx * SIZE + (Num - 1))
                        };

                        AddRow(&Dlx, RowIdx, ColIdx);
                    }
                } else {
                    u8 Num = Board[R * SIZE + C] - '0';
                    u16 RowIdx = R * SIZE * SIZE + C * SIZE + (Num - 1);
                    u16 BoxIdx = (R / BOX_SIZE) * BOX_SIZE + (C / BOX_SIZE);
                    u16 ColIdx[4] = {
                        (u16) (R * SIZE + C),
                        (u16) (SIZE * SIZE + R * SIZE + (Num - 1)),
                        (u16) (2 * SIZE * SIZE + C * SIZE + (Num - 1)),
                        (u16) (3 * SIZE * SIZE + BoxIdx * SIZE + (Num - 1))
                    };

                    AddRow(&Dlx, RowIdx, ColIdx);

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
