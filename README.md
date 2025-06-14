# sudsolve
sudoku solver using Donald Knuth's dancing links algorithm and multithreading.

## usage
```bash
./build.[bat|sh]
./build/sudsolve[.exe] "tests/all_17_clue_sudokus.txt"
```

## details
the solver uses as many cores are available and the list of puzzles within the input file are distributed amongst them.

the input file format needs to be as follows:
```
<N>
<puzzle0>
...
<puzzleN>
```

DOS and UNIX line endings only matter when they are both present within the same input file.
