if [ ! -d build ];then
   mkdir -p build
fi
cd build
cmake ..
make -j
cd ..
if [ ! -d bin ];then
   mkdir -p bin
fi
mv build/bin/chenWeb bin/

if [ ! -d bin/log ];then
   mkdir -p bin/log
fi

rm -r -f ./build


echo '=================================================';
echo 'Executable file is located ./bin/chenWeb'
echo '=================================================';
