@echo off

if not exist .\build mkdir .\build
pushd .\build

set CommonCompilerFlags=      ^
    /O2                       ^
    /Oi                       ^
    /Ot                       ^
    /GL                       ^
    /Qpar                     ^
    /fp:fast                  ^
    /fp:except-               ^
    /Ob3                      ^
    /Oy-                      ^
    /GS-                      ^
    /guard:cf-                ^
    /Qvec-report:2            ^
    /arch:AVX512              ^
    /nologo                   ^
    /FC                       ^
    /WX                       ^
    /MT                       ^
    /GR-                      ^
    /Gm-                      ^
    /GF                       ^
    /guard:ehcont-            ^
    /EHa-                     ^
    /Zi                       ^
    /diagnostics:caret        ^
    /MP32                     ^
    /D_CRT_SECURE_NO_WARNINGS ^
    /DNDEBUG

set CommonLinkerFlags=^
    /LTCG             ^
    /OPT:REF          ^
    /OPT:ICF          ^
    /INCREMENTAL:NO   ^
    /time

del *.pdb > NUL 2> NUL

cl.exe %CommonCompilerFlags% ..\sudsolve.cpp -link %CommonLinkerFlags%

set LastError=%ERRORLEVEL%
popd

if not %LastError% == 0 goto :end

.\build\sudsolve.exe

:end
