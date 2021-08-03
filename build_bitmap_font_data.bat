@echo off
where python
if !errorlevel! == 1 (
    echo Couldn't find python.exe in PATH
    goto end   
)

python convert_bitmap_font_to_c_array.py -gw 10 -gh 10 -gr 16 -i gb_font.bmp -o k15_gb_font.h
echo successfully wrote k15_gb_font.h

:end
    pause