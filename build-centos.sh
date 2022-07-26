make clean
make
if [ ! -d bin ];then
   mkdir -p bin
fi

if [ ! -d bin/log ];then
   mkdir -p bin/log
fi

mv chenWeb bin/

echo '=================================================';
echo '\033[1;32;40m[success]  Executable file is located ./bin/chenWeb\033[0m\n'
