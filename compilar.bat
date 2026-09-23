@echo off
setlocal
cd /d "%~dp0"
rem Gera dois executaveis separados: cliente e servidor.
g++ -std=c++11 -Wall -Wextra -Wpedantic -Iinclude -static src/cliente.cpp src/ipc.cpp -o cliente.exe
if errorlevel 1 exit /b 1
g++ -std=c++11 -Wall -Wextra -Wpedantic -Iinclude -static src/servidor.cpp src/banco.cpp src/ipc.cpp -o servidor.exe
if errorlevel 1 exit /b 1
echo Compilacao concluida: cliente.exe e servidor.exe.
