@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ============================================
REM TOOLS
REM ============================================

set "NASM=U:\Ungint\NASM\nasm.exe"
set "QEMU=U:\Ungint\QEMU\qemu-system-x86_64.exe"
set "CC=U:\Ungint\CROSS_COMPILER\bin\x86_64-elf-gcc.exe"
set "LD=U:\Ungint\CROSS_COMPILER\bin\x86_64-elf-ld.exe"
set "OBJCOPY=U:\Ungint\CROSS_COMPILER\bin\x86_64-elf-objcopy.exe"

REM ============================================
REM PATHS
REM ============================================

set "OUT_DIR=Temp"
set "COMBINED=run.bin"

set "KERNEL_DIR=Kernel"
set "KERNEL_SRC=Kernel\SourceI"
set "KERNEL_INC=Kernel\Include"
set "KERNEL_OBJ=Temp\Kernel"

set "LINKER_SCRIPT=Kernel\linker.ld"

set "BUILD_LOG=Temp\build.log"
set "COMBINED_TMP=Temp\combined.bin"

REM ============================================
REM TIMER START
REM ============================================

call :read_time START_TOTAL_MS

REM ============================================
REM PREPARE
REM ============================================

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%KERNEL_OBJ%" mkdir "%KERNEL_OBJ%"

del /q "%BUILD_LOG%" >nul 2>&1
del /q "%COMBINED%" >nul 2>&1
del /q "%COMBINED_TMP%" >nul 2>&1

del /q "%KERNEL_OBJ%\*.o" >nul 2>&1
del /q "%KERNEL_OBJ%\kernel.elf" >nul 2>&1

echo ============================================ > "%BUILD_LOG%"
echo UngintOS Build Log >> "%BUILD_LOG%"
echo ============================================ >> "%BUILD_LOG%"
echo. >> "%BUILD_LOG%"

REM ============================================
REM COUNT FILES
REM ============================================

set /a ASM_COUNT=0

for %%F in (Boot\*.asm) do (
    if exist "%%F" set /a ASM_COUNT+=1
)

set /a SRC_COUNT=0

for %%F in ("%KERNEL_SRC%\*.c") do (
    if exist "%%F" set /a SRC_COUNT+=1
)

REM ============================================
REM TOTAL STEPS
REM ============================================

set /a TOTAL_STEPS=ASM_COUNT+1+SRC_COUNT+1+1+3+1
set /a STEP=0

REM ============================================
REM ANSI ESC
REM ============================================

for /F "delims=" %%E in ('echo prompt $E^| cmd') do set "ESC=%%E"

REM ============================================
REM BUILD ASM
REM ============================================

for %%F in (Boot\*.asm) do (

    if exist "%%F" (

        echo ===== NASM: %%F =====>>"%BUILD_LOG%"

        "%NASM%" -f bin "%%F" -o "%OUT_DIR%\%%~nF.bin" >>"%BUILD_LOG%" 2>&1

        if errorlevel 1 goto ERROR_ASM

        set /a STEP+=1
        call :progress
    )
)

REM ============================================
REM BUILD MAIN.C
REM ============================================

if not exist "%KERNEL_DIR%\main.c" goto ERROR_MAIN

echo ===== GCC: main.c =====>>"%BUILD_LOG%"

"%CC%" -ffreestanding -ffunction-sections -m64 -mno-red-zone -mno-sse -mno-mmx -mno-sse2 -mno-80387 -I"%KERNEL_INC%" -I"%KERNEL_DIR%" -c "%KERNEL_DIR%\main.c" -o "%KERNEL_OBJ%\main.o" >>"%BUILD_LOG%" 2>&1

if errorlevel 1 goto ERROR_MAIN

set /a STEP+=1
call :progress

REM ============================================
REM BUILD SOURCEI
REM ============================================

for %%F in ("%KERNEL_SRC%\*.c") do (

    if exist "%%F" (

        echo ===== GCC: %%F =====>>"%BUILD_LOG%"

        "%CC%" -ffreestanding -ffunction-sections -m64 -mno-red-zone -mno-sse -mno-mmx -mno-sse2 -mno-80387 -I"%KERNEL_INC%" -I"%KERNEL_DIR%" -c "%%F" -o "%KERNEL_OBJ%\%%~nF.o" >>"%BUILD_LOG%" 2>&1

        if errorlevel 1 goto ERROR_C

        set /a STEP+=1
        call :progress
    )
)

REM ============================================
REM COLLECT OBJECT FILES
REM ============================================

set "OBJ_FILES=%KERNEL_OBJ%\main.o"

for %%F in ("%KERNEL_OBJ%\*.o") do (
    if /I not "%%~nxF"=="main.o" (
        set "OBJ_FILES=!OBJ_FILES! %%F"
    )
)

REM ============================================
REM LINK
REM ============================================

echo ===== LD: kernel.elf =====>>"%BUILD_LOG%"

"%LD%" -nostdlib -T "%LINKER_SCRIPT%" -o "%KERNEL_OBJ%\kernel.elf" !OBJ_FILES! >>"%BUILD_LOG%" 2>&1

if errorlevel 1 goto ERROR_LINK

set /a STEP+=1
call :progress

REM ============================================
REM OBJCOPY
REM ============================================

echo ===== OBJCOPY: kernel.bin =====>>"%BUILD_LOG%"

"%OBJCOPY%" -O binary "%KERNEL_OBJ%\kernel.elf" "%OUT_DIR%\kernel.bin" >>"%BUILD_LOG%" 2>&1

if errorlevel 1 goto ERROR_OBJCOPY

set /a STEP+=1
call :progress

REM ============================================
REM CREATE EMPTY COMBINED
REM ============================================

del /q "%COMBINED_TMP%" >nul 2>&1
type nul > "%COMBINED_TMP%"

REM ============================================
REM ADD BOOT.BIN
REM ============================================

if exist "%OUT_DIR%\boot.bin" (

    copy /b "%COMBINED_TMP%"+"%OUT_DIR%\boot.bin" "%OUT_DIR%\combine.tmp" >nul 2>>"%BUILD_LOG%"

    if errorlevel 1 goto ERROR_COMBINE

    move /y "%OUT_DIR%\combine.tmp" "%COMBINED_TMP%" >nul 2>>"%BUILD_LOG%"

    if errorlevel 1 goto ERROR_COMBINE
)

set /a STEP+=1
call :progress

REM ============================================
REM ADD BOOT2.BIN
REM ============================================

if exist "%OUT_DIR%\boot2.bin" (

    copy /b "%COMBINED_TMP%"+"%OUT_DIR%\boot2.bin" "%OUT_DIR%\combine.tmp" >nul 2>>"%BUILD_LOG%"

    if errorlevel 1 goto ERROR_COMBINE

    move /y "%OUT_DIR%\combine.tmp" "%COMBINED_TMP%" >nul 2>>"%BUILD_LOG%"

    if errorlevel 1 goto ERROR_COMBINE
)

set /a STEP+=1
call :progress

REM ============================================
REM ADD KERNEL.BIN
REM ============================================

if exist "%OUT_DIR%\kernel.bin" (

    copy /b "%COMBINED_TMP%"+"%OUT_DIR%\kernel.bin" "%OUT_DIR%\combine.tmp" >nul 2>>"%BUILD_LOG%"

    if errorlevel 1 goto ERROR_COMBINE

    move /y "%OUT_DIR%\combine.tmp" "%COMBINED_TMP%" >nul 2>>"%BUILD_LOG%"

    if errorlevel 1 goto ERROR_COMBINE
)

REM ============================================
REM FINALIZE RUN.BIN
REM ============================================

move /y "%COMBINED_TMP%" "%COMBINED%" >nul 2>>"%BUILD_LOG%"

if errorlevel 1 goto ERROR_COMBINE

set /a STEP+=1
call :progress

REM ============================================
REM PAD
REM ============================================

for %%F in ("%COMBINED%") do set "CUR_SIZE=%%~zF"

if !CUR_SIZE! LSS 131072 (

    echo ===== PAD run.bin =====>>"%BUILD_LOG%"

    fsutil file seteof "%COMBINED%" 131072 >>"%BUILD_LOG%" 2>&1

    if errorlevel 1 goto ERROR_PAD
)

set /a STEP+=1
call :progress

REM ============================================
REM TIMER END
REM ============================================

call :read_time END_TOTAL_MS

set /a ELAPSED_MS = END_TOTAL_MS - START_TOTAL_MS

if !ELAPSED_MS! LSS 0 set /a ELAPSED_MS += 86400000

set /a ELAPSED_SEC = ELAPSED_MS / 1000
set /a ELAPSED_FRAC = (ELAPSED_MS %% 1000) / 10

if !ELAPSED_FRAC! LSS 10 set "ELAPSED_FRAC=0!ELAPSED_FRAC!"

REM Xóa dòng progress cuối
<nul set /p "=!ESC![2K!ESC![G"

echo Build finished in !ELAPSED_SEC!.!ELAPSED_FRAC!s

"%QEMU%" -drive file=run.bin,format=raw -drive file=U:\Ungint\OS\Data\Disk.img,format=raw -d cpu_reset,guest_errors,int -D qemu.log
exit /b 0


REM ============================================
REM READ TIME
REM Trả về tổng milliseconds từ nửa đêm
REM Cú pháp: call :read_time VAR_NAME
REM ============================================

:read_time

setlocal EnableDelayedExpansion

for /f "tokens=1-4 delims=:., " %%A in ("%TIME%") do (
    set "H=%%A"
    set "M=%%B"
    set "S=%%C"
    set "MS=%%D"
)

REM Xử lý giờ có space ở đầu (locale US: " 9:30:15.42")
if "!H:~0,1!"==" " set "H=!H:~1!"

REM Xử lý milliseconds 1 chữ số (VD: ".4" thay vì ".42")
if "!MS!"=="" set "MS=0"
if !MS! LSS 10 set "MS=0!MS!"
if !MS! LSS 100 set "MS=!MS!0"

REM Đảm bảo H có 2 chữ số
if !H! LSS 10 set "H=0!H!"

REM Tính tổng ms
set /a TOTAL = (1!H! - 100) * 3600000 + (1!M! - 100) * 60000 + (1!S! - 100) * 1000 + (1!MS! - 100) * 10

endlocal & set "%1=%TOTAL%"
goto :eof


REM ============================================
REM PROGRESS
REM ============================================

:progress

setlocal EnableDelayedExpansion

if !STEP! GTR !TOTAL_STEPS! set /a STEP=TOTAL_STEPS

set /a PCT=(STEP*100)/TOTAL_STEPS
set /a FILLED=(PCT*90)/100
set /a EMPTY=90-FILLED

set "BAR="

for /l %%I in (1,1,!FILLED!) do (
    set "BAR=!BAR!#"
)

for /l %%I in (1,1,!EMPTY!) do (
    set "BAR=!BAR!-"
)

REM ===== Tính elapsed time =====

for /f "tokens=1-4 delims=:., " %%A in ("%TIME%") do (
    set "NOW_H=%%A"
    set "NOW_M=%%B"
    set "NOW_S=%%C"
    set "NOW_MS=%%D"
)

if "!NOW_H:~0,1!"==" " set "NOW_H=!NOW_H:~1!"
if "!NOW_MS!"=="" set "NOW_MS=0"

REM Pad MS thành 3 chữ số: "4" -> "004", "42" -> "042", "420" -> "420"
if !NOW_MS! LSS 10 set "NOW_MS=00!NOW_MS!"
if !NOW_MS! LSS 100 set "NOW_MS=0!NOW_MS!"

if !NOW_H! LSS 10 set "NOW_H=0!NOW_H!"

set /a NOW_TOTAL_MS = (1!NOW_H! - 100) * 3600000 + (1!NOW_M! - 100) * 60000 + (1!NOW_S! - 100) * 1000 + (1!NOW_MS! - 1000)

set /a ELAPSED = NOW_TOTAL_MS - START_TOTAL_MS

if !ELAPSED! LSS 0 set /a ELAPSED += 86400000

set /a ELAPSED_S = ELAPSED / 1000
set /a ELAPSED_F = (ELAPSED %% 1000) / 10

if !ELAPSED_F! LSS 10 set "ELAPSED_F=0!ELAPSED_F!"

REM ===== Format timer string =====
set "TIMER_STR=!ELAPSED_S!.!ELAPSED_F!s"

REM ===== Tính padding để căn phải =====
REM Tổng chiều rộng = 90 (bar) + 5 ("[ ] ") + 4 ("100%") = 99
REM Timer string căn phải ở cột 99
set "TIMER_LEN=0"
set "TMP=!TIMER_STR!"
:count_len
if not "!TMP!"=="" (
    set "TMP=!TMP:~1!"
    set /a TIMER_LEN+=1
    goto :count_len
)

set /a PAD = 99 - TIMER_LEN
if !PAD! LSS 0 set /a PAD=0

set "PAD_STR="
for /l %%I in (1,1,!PAD!) do (
    set "PAD_STR=!PAD_STR! "
)

REM ===== Vẽ 2 dòng =====
REM Dòng 1: progress bar
REM Dòng 2: timer căn phải

REM Xóa dòng 1 và in progress bar
<nul set /p "=!ESC![2K!ESC![G"
<nul set /p "=[!BAR!] !PCT!%%"

REM Xuống dòng, xóa dòng 2, in timer căn phải
echo.
<nul set /p "=!ESC![2K!ESC![G"
<nul set /p "=!PAD_STR!!TIMER_STR!"

REM Quay lại dòng 1
<nul set /p "=!ESC![1A"
<nul set /p "=!ESC![G"

endlocal
goto :eof


REM ============================================
REM ERRORS
REM ============================================

:ERROR_ASM
echo.
echo.
echo ============================================
echo ERROR: NASM build failed
echo ============================================
goto SHOW_LOG

:ERROR_MAIN
echo.
echo.
echo ============================================
echo ERROR: main.c build failed
echo ============================================
goto SHOW_LOG

:ERROR_C
echo.
echo.
echo ============================================
echo ERROR: C source build failed
echo ============================================
goto SHOW_LOG

:ERROR_LINK
echo.
echo.
echo ============================================
echo ERROR: Kernel linking failed
echo ============================================
goto SHOW_LOG

:ERROR_OBJCOPY
echo.
echo.
echo ============================================
echo ERROR: objcopy failed
echo ============================================
goto SHOW_LOG

:ERROR_COMBINE
echo.
echo.
echo ============================================
echo ERROR: Combining binaries failed
echo ============================================
goto SHOW_LOG

:ERROR_PAD
echo.
echo.
echo ============================================
echo ERROR: Padding run.bin failed
echo ============================================
goto SHOW_LOG


REM ============================================
REM SHOW LOG
REM ============================================

:SHOW_LOG

echo.
echo Build log:
echo --------------------------------------------
type "%BUILD_LOG%"
echo --------------------------------------------
echo.
echo Build failed!
echo.

pause
exit /b 1