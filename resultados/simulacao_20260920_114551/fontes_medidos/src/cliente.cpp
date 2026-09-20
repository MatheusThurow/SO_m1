#include "sistema.hpp"
#include <iostream>
#include <cstdlib>

// As margens de rolagem reservam o topo apenas no terminal interativo.
// Quando a saida e redirecionada (testes), ela continua sendo texto simples.
class TelaCliente {
    HANDLE saida = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD modo_original = 0;
    bool vt = false;
    int largura = 0, altura = 0;

    void instrucoes() {
        std::cout << "Comandos: INSERT id nome | SELECT id | UPDATE id nome | DELETE id\n"
                  << "SAIR fecha o cliente. PARAR encerra o servidor. CLS limpa a tela.\n";
    }
public:
    TelaCliente() {
        DWORD modo_entrada;
        vt = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &modo_entrada)
          && GetConsoleMode(saida, &modo_original)
          && SetConsoleMode(saida, modo_original | 0x0004 | ENABLE_PROCESSED_OUTPUT);
        if (vt) limpar();
        else instrucoes();
    }
    ~TelaCliente() {
        if (vt) {
            std::cout << "\x1b[r" << std::flush;
            SetConsoleMode(saida, modo_original);
        }
    }
    void limpar() {
        if (!vt) {
            DWORD modo;
            if (GetConsoleMode(saida, &modo)) std::system("cls");
            instrucoes();
            return;
        }
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(saida, &info)) return;
        largura = info.srWindow.Right - info.srWindow.Left + 1;
        altura = info.srWindow.Bottom - info.srWindow.Top + 1;
        std::cout << "\x1b[r\x1b[2J\x1b[H";
        if (largura >= 40 && altura >= 12) {
            std::cout << "CLIENTE M1 - COMANDOS\r\n"
                      << "INSERT id nome  |  SELECT id\r\n"
                      << "UPDATE id nome  |  DELETE id\r\n"
                      << "CLS: limpar     |  SAIR: fechar cliente\r\n"
                      << "PARAR: encerrar servidor\r\n"
                      << "--------------------------------------";
            // Somente as linhas 7 ate o final da janela podem rolar.
            std::cout << "\x1b[7;" << altura << "r\x1b[7;1H";
        } else {
            std::cout << "Amplie a janela para fixar as instrucoes.\r\n";
            instrucoes();
        }
        std::cout << std::flush;
    }
    void preparar() {
        if (!vt) return;
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(saida, &info) &&
            (largura != info.srWindow.Right - info.srWindow.Left + 1 ||
             altura != info.srWindow.Bottom - info.srWindow.Top + 1)) limpar();
        std::cout << "> " << std::flush;
    }
};

int main() {
    TelaCliente tela;
    std::string linha;
    while (true) {
        tela.preparar();
        if (!std::getline(std::cin, linha)) break;
        if (linha == "SAIR") break;
        if (linha == "cls" || linha == "CLS") {
            tela.limpar();
            continue;
        }
        if (linha.empty()) continue;
        if (linha.size() > MAX_REQUISICAO) {
            std::cerr << "Comando muito longo.\n";
            return 1;
        }
        std::string resposta;
        if (!enviar_requisicao(linha, resposta)) return 1;
        std::cout << resposta << std::endl;
        if (linha == "PARAR") break;
    }
}
