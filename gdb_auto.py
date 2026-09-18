import subprocess
import sys
import os

# This script runs GDB on DeliHub.exe and waits for crash
# Since the app needs user interaction, we use --command file approach
# GDB will catch SIGSEGV and write bt full to file

gdb_commands = r"""
set pagination off
set logging file C:/Users/Public/gdb_bt_full.txt
set logging overwrite on
set logging enabled on
handle SIGSEGV stop print
handle SIGABRT stop print
catch signal SIGSEGV
catch signal SIGABRT
run
"""

with open("C:/Users/Public/gdb_init.txt", "w") as f:
    f.write(gdb_commands)

print("GDB init script written to C:/Users/Public/gdb_init.txt")
print("Now run manually:")
print(r'  cd e:\tifany\build_debug')
print(r'  C:\msys64\ucrt64\bin\gdb.exe -x C:/Users/Public/gdb_init.txt DeliHub.exe')
print("After crash, type: bt full")
print("Then type: quit")
print("Output will be in C:/Users/Public/gdb_bt_full.txt")
