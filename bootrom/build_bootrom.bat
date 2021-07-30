@echo off
rgbasm main.z80 -o main.obj
rgblink main.obj -o bootrom.gb
rgbfix -v -p 0 bootrom.gb