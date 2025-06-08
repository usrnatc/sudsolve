if [ ! -d ./build/ ]; then
    mkdir ./build;
fi

clang++ -O3 -ffast-math -flto -march=native ./sudsolve.cpp -o ./build/sudsolve

if [ -f ./build/sudsolve ]; then
    strip -s ./build/sudsolve;
fi
