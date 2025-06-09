if [ ! -d ./build/ ]; then
    mkdir ./build;
fi

clang++ -v -O3 -ffast-math -flto -march=native -fuse-ld=mold -static ./sudsolve.cpp -o ./build/sudsolve

if [ -f ./build/sudsolve ]; then
    strip -s ./build/sudsolve;
    ./build/sudsolve;
fi
