if [ ! -d build ];then
   mkdir -p build
fi
cd build
cmake ..
make
cd ..
if [ ! -d bin ];then
   mkdir -p bin
fi
mv build/bin/chenWeb bin/
rm -r -f ./build