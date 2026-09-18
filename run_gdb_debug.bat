@echo off
echo =============================================
echo  DeliHub Debug Session
echo  1. Login to the app normally
echo  2. Wait for the crash
echo  3. In THIS window, type: bt full
echo  4. Then type: quit
echo  Output: C:\Users\Public\gdb_bt_full.txt
echo =============================================
cd /d e:\tifany\build_debug
C:\msys64\ucrt64\bin\gdb.exe ^
  -ex "set pagination off" ^
  -ex "set logging file C:/Users/Public/gdb_bt_full.txt" ^
  -ex "set logging overwrite on" ^
  -ex "set logging enabled on" ^
  -ex "handle SIGSEGV stop print" ^
  -ex "handle SIGABRT stop print" ^
  -ex "run" ^
  DeliHub.exe
