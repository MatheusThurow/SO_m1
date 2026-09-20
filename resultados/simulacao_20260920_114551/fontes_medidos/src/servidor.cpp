#include "sistema.hpp"
#include <climits>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>

struct Pedido {
    std::string texto;
    HANDLE pipe; // A thread responde ao cliente que enviou este pedido.
};

static std::queue<Pedido> fila;
static bool encerrando = false;
static Mutex mutex_fila;
static Mutex mutex_log;
static HANDLE tem_requisicao;
static std::ofstream arquivo_log;

static DWORD WINAPI atender(LPVOID argumento) {
    int numero = *static_cast<int*>(argumento);
    while (true) {
        // O semaforo conta pedidos disponiveis, sem espera ocupada.
        WaitForSingleObject(tem_requisicao, INFINITE);
        Pedido pedido;
        {
            GuardaMutex guarda(mutex_fila);
            if (fila.empty() && encerrando) break;
            if (fila.empty()) continue;
            pedido = fila.front();
            fila.pop();
        }
        std::string resposta = executar_requisicao(pedido.texto);
        {
            GuardaMutex guarda(mutex_log);
            arquivo_log << "[thread " << numero << "] " << pedido.texto
                        << " => " << resposta << std::endl;
            std::cout << "[thread " << numero << "] " << pedido.texto
                      << " => " << resposta << std::endl;
        }
        // Fora dos mutexes: um cliente lento nao bloqueia o banco ou o log.
        responder_e_fechar(pedido.pipe, resposta);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    int quantidade_threads = NUM_THREADS;
    if (argc > 2) {
        std::cerr << "Uso: servidor.exe [threads de 1 a 64]\n";
        return 1;
    }
    if (argc == 2) {
        std::istringstream entrada(argv[1]);
        char sobra;
        if (!(entrada >> quantidade_threads) || (entrada >> sobra) ||
            quantidade_threads < 1 || quantidade_threads > 64) {
            std::cerr << "Informe de 1 a 64 threads.\n";
            return 1;
        }
    }
    HANDLE pipe = criar_pipe();
    if (pipe == INVALID_HANDLE_VALUE) return 1;
    arquivo_log.open("servidor.log");
    tem_requisicao = CreateSemaphoreA(nullptr, 0, LONG_MAX, nullptr);
    if (!arquivo_log || !tem_requisicao) {
        std::cerr << "Falha ao abrir log ou criar semaforo.\n";
        CloseHandle(pipe);
        if (tem_requisicao) CloseHandle(tem_requisicao);
        return 1;
    }
    HANDLE threads[64];
    int numeros[64];
    int criadas = 0;
    for (int i = 0; i < quantidade_threads; ++i) {
        numeros[i] = i + 1;
        threads[i] = CreateThread(nullptr, 0, atender, &numeros[i], 0, nullptr);
        if (!threads[i]) break;
        ++criadas;
    }
    bool sucesso = criadas == quantidade_threads;
    if (sucesso) {
        arquivo_log << "SERVIDOR PRONTO: " << quantidade_threads << " threads" << std::endl;
        std::cout << "Servidor pronto com " << quantidade_threads << " threads." << std::endl;
        while (true) {
            BOOL conectado = ConnectNamedPipe(pipe, nullptr);
            if (!conectado && GetLastError() != ERROR_PIPE_CONNECTED) {
                std::cerr << "Falha ao receber conexao.\n";
                sucesso = false;
                break;
            }
            char dados[MAX_REQUISICAO + 1];
            DWORD lidos = 0;
            BOOL recebido = ReadFile(pipe, dados, sizeof(dados), &lidos, nullptr);
            bool valido = recebido && lidos > 0 && lidos <= MAX_REQUISICAO;
            std::string pedido = valido ? std::string(dados, lidos) : "";
            if (pedido == "PARAR") break;
            // Mantem uma instancia de escuta enquanto a thread responde na anterior.
            HANDLE proximo = criar_pipe(false);
            if (proximo == INVALID_HANDLE_VALUE) {
                responder_e_fechar(pipe, "ERRO: falha ao preparar nova conexao");
                pipe = INVALID_HANDLE_VALUE;
                sucesso = false;
                break;
            }
            if (valido) {
                GuardaMutex guarda(mutex_fila);
                fila.push(Pedido{pedido, pipe});
                ReleaseSemaphore(tem_requisicao, 1, nullptr);
            } else {
                responder_e_fechar(pipe, "ERRO: requisicao invalida");
            }
            pipe = proximo;
        }
    } else {
        std::cerr << "Falha ao criar o pool.\n";
    }
    {
        GuardaMutex guarda(mutex_fila);
        encerrando = true;
    }
    // Acorda todas as threads; pedidos pendentes sao concluidos primeiro.
    if (criadas) {
        ReleaseSemaphore(tem_requisicao, criadas, nullptr);
        WaitForMultipleObjects(criadas, threads, TRUE, INFINITE);
        for (int i = 0; i < criadas; ++i) CloseHandle(threads[i]);
    }
    CloseHandle(tem_requisicao);
    if (pipe != INVALID_HANDLE_VALUE) {
        if (sucesso) responder_e_fechar(pipe, "OK: servidor encerrado");
        else CloseHandle(pipe);
    }
    return sucesso ? 0 : 1;
}
