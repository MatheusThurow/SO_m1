CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -Wpedantic -Iinclude -static

.PHONY: all clean test
# Compila os dois processos do sistema.
all: cliente.exe servidor.exe

cliente.exe: src/cliente.cpp src/ipc.cpp include/sistema.hpp
	$(CXX) $(CXXFLAGS) src/cliente.cpp src/ipc.cpp -o cliente.exe

servidor.exe: src/servidor.cpp src/banco.cpp src/ipc.cpp include/sistema.hpp
	$(CXX) $(CXXFLAGS) src/servidor.cpp src/banco.cpp src/ipc.cpp -o servidor.exe

# Compila antes de executar os testes funcionais.
test: all
	python testes/testar.py

clean:
	cmd /c del /q cliente.exe servidor.exe
