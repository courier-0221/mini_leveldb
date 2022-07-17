#!/bin/bash
#!/bin/sh

#build leveldb
git submodule update --init --recursive
cd leveldb
mkdir -p build && cd build
cmake .. && make
#copy lib&include
cd ../../
mkdir -p ./_install
mkdir -p ./_install/lib
mkdir -p ./_install/include
cp leveldb/build/lib* ./_install/lib/ -rf
cp leveldb/include/ ./_install/ -rf
