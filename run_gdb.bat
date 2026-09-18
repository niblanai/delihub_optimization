@echo off
cd /d e:\tifany\build_debug
C:\msys64\ucrt64\bin\gdb.exe -batch ^
  -ex "set pagination off" ^
  -ex "run" ^
  -ex "bt full" ^
  -ex "quit" ^
  --args DeliHub.exe ^
  > e:\tifany\gdb_output.txt 2>&1
echo GDB finished. Check e:\tifany\gdb_output.txt
