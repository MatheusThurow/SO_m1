#include "sistema.hpp"
#include <iostream>
#include <cstdlib>

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::flush;
using std::getline;
using std::string;
using std::system;

// Mantem as instrucoes fixas no topo do cliente não alterar pfv
class TelaCliente {
    HANDLE saida = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD modo_original = 0;
    bool vt = false;
    int largura = 0, altura = 0;

    void instrucoes() {
        cout << "Comandos: INSERT id nome | SELECT id | UPDATE id nome | DELETE id\n"
                  << "SAIR fecha o cliente. PARAR encerra o servidor. CLS limpa a tela.\n";
    }

  public:
    TelaCliente() {
        DWORD modo_entrada;
        vt = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &modo_entrada) &&
             GetConsoleMode(saida, &modo_original) &&
             SetConsoleMode(saida, modo_original | 0x0004 | ENABLE_PROCESSED_OUTPUT);
        if (vt)
            limpar();
        else
            instrucoes();
    }
    ~TelaCliente() {
        if (vt) {
            cout << "\x1b[r" << flush;
            SetConsoleMode(saida, modo_original);
        }
    }
    void limpar() {
        if (!vt) {
            DWORD modo;
            if (GetConsoleMode(saida, &modo))
                system("cls");
            instrucoes();
            return;
        }
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(saida, &info))
            return;
        largura = info.srWindow.Right - info.srWindow.Left + 1;
        altura = info.srWindow.Bottom - info.srWindow.Top + 1;
        cout << "\x1b[r\x1b[2J\x1b[H";
        if (largura >= 40 && altura >= 12) {
            cout << "CLIENTE M1 - COMANDOS\r\n"
                      << "INSERT id nome  |  SELECT id\r\n"
                      << "UPDATE id nome  |  DELETE id\r\n"
                      << "CLS: limpar     |  SAIR: fechar cliente\r\n"
                      << "PARAR: encerrar servidor\r\n"
                      << "--------------------------------------";
            cout << "\x1b[7;" << altura << "r\x1b[7;1H";
        } else {
            cout << "Amplie a janela para fixar as instrucoes.\r\n";
            instrucoes();
        }
        cout << flush;
    }
    void preparar() {
        if (!vt)
            return;
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(saida, &info) &&
            (largura != info.srWindow.Right - info.srWindow.Left + 1 ||
             altura != info.srWindow.Bottom - info.srWindow.Top + 1))
            limpar();
        cout << "> " << flush;
    }
};

int main() {
    TelaCliente tela;
    string linha;
    while (true) {
        tela.preparar();
        // Le uma linha inteira, preservando os espacos dos nomes.
        if (!getline(cin, linha))
            break;
        // SAIR fecha so o cliente; CLS limpa a tela sem mexer no banco ou no log.
        if (linha == "SAIR")
            break;
        if (linha == "cls" || linha == "CLS") {
            tela.limpar();
            continue;
        }
        if (linha.empty())
            continue;
        if (linha.size() > MAX_REQUISICAO) {
            cerr << "Comando muito longo.\n";
            return 1;
        }
        // O cliente nao acessa o banco: envia a linha ao servidor por IPC.
        string resposta;
        if (!enviar_requisicao(linha, resposta))
            return 1;
        // So chega aqui depois de receber o resultado; o proximo pedido vem depois.
        cout << resposta << endl;
        // PARAR foi enviado ao servidor; sai apos receber a confirmacao final.
        if (linha == "PARAR")
            break;
    }
}
