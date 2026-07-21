@echo off

:: ======清理编译缓存======
:: 删除bootloader缓存
rmdir /s /q build\bootloader 2>nul
:: 删除分区表缓存
rmdir /s /q build\partition_table 2>nul
:: 删除CMake缓存文件
del /f /q build\CMakeCache.txt 2>nul
del /f /q build\CMakeFiles\cmake.check_cache 2>nul

:: 重新生成CMake配置
idf.py reconfigure
echo.
echo ======================================
echo 缓存清理+工程重配置完成，可以编译烧录
echo ======================================
echo 常用指令：idf.py -p COM7 flash monitor
echo.
pause
cmd /k