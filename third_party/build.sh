#!/bin/bash
#!/bin/sh

#build googletest
mkdir -p googletest/build && cd googletest/build
cmake .. && cmake --build .

#copy googletest lib
cd ../../
mkdir -p ./lib
cp googletest/build/lib/lib* ./lib
