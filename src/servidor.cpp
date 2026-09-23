#include "sistema.hpp"
#include <climits>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>

using std::cerr;
using std::cout;
using std::endl;
using std::istringstream;
using std::ofstream;
using std::queue;
using std::string;

// Guarda o comando e a conexao do cliente que deve receber a resposta.
struct Pedido {
    string texto;
    HANDLE pipe;
};

// A fila liga a recepcao dos pedidos ao processamento pelo pool.
// Estas variaveis sao compartilhadas pelas threads do servidor, nao pelo cliente.
static queue<Pedido> fila;
static bool encerrando = false;
static Mutex mutex_fila;
static Mutex mutex_log;
static HANDLE tem_requisicao;
static ofstream arquivo_log;

static DWORD WINAPI atender(LPVOID argumento) {
    int numero = *static_cast<int *>(argumento);
    while (true) {
        // Semaforo: espera ate haver pedidos na fila.
        WaitForSingleObject(tem_requisicao, INFINITE);
        Pedido pedido;
        {
            // Somente uma thread por vez retira pedidos da fila.
            GuardaMutex guarda(mutex_fila);
            if (fila.empty() && encerrando)
                break;
            if (fila.empty())
                continue;
            pedido = fila.front();
            fila.pop();
        }
        // O bloco anterior ja liberou o mutex da fila. O banco usa outro mutex.
        string resposta = executar_requisicao(pedido.texto);
        {
            // Evita misturar mensagens de threads diferentes no log e no terminal.
            GuardaMutex guarda(mutex_log);
            // Registra thread, comando e resultado; endl termina a linha e faz flush.
            arquivo_log << "[thread " << numero << "] " << pedido.texto << " => " << resposta
                        << endl;
            // Exibe no servidor o mesmo resultado registrado no arquivo.
            cout << "[thread " << numero << "] " << pedido.texto << " => " << resposta
                      << endl;
        }
        // Responde fora dos mutexes: a espera pelo cliente nao prende o banco ou o log.
        responder_e_fechar(pedido.pipe, resposta);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    // Padrao: 4 threads. Exemplo: servidor.exe 8 escolhe 8 (limite: 1 a 64).
    int quantidade_threads = NUM_THREADS;
    if (argc > 2) {
        cerr << "Uso: servidor.exe [threads de 1 a 64]\n";
        return 1;
    }
    if (argc == 2) {
        istringstream entrada(argv[1]);
        char sobra;
        if (!(entrada >> quantidade_threads) || (entrada >> sobra) || quantidade_threads < 1 ||
            quantidade_threads > 64) {
            cerr << "Informe de 1 a 64 threads.\n";
            return 1;
        }
    }
    HANDLE pipe = criar_pipe();
    if (pipe == INVALID_HANDLE_VALUE)
        return 1;
    // Salva automaticamente na pasta de trabalho em que o servidor foi iniciado.
    // Ao iniciar novamente, sobrescreve o log anterior (nao usa modo append).
    // Para guardar uma execucao, copie ou renomeie o log antes de reiniciar.
    arquivo_log.open("servidor.log");
    // Comeca em zero: as threads aguardam ate chegar trabalho.
    tem_requisicao = CreateSemaphoreA(nullptr, 0, LONG_MAX, nullptr);
    if (!arquivo_log || !tem_requisicao) {
        cerr << "Falha ao abrir log ou criar semaforo.\n";
        CloseHandle(pipe);
        if (tem_requisicao)
            CloseHandle(tem_requisicao);
        return 1;
    }
    HANDLE threads[64];
    // Os argumentos das threads permanecem validos ate o pool terminar.
    int numeros[64];
    int criadas = 0;
    // Cria o pool de threads reutilizadas no atendimento.
    for (int i = 0; i < quantidade_threads; ++i) {
        numeros[i] = i + 1;
        threads[i] = CreateThread(nullptr, 0, atender, &numeros[i], 0, nullptr);
        if (!threads[i])
            break;
        ++criadas;
    }
    bool sucesso = criadas == quantidade_threads;
    if (sucesso) {
        arquivo_log << "SERVIDOR PRONTO: " << quantidade_threads << " threads" << endl;
        cout << "Servidor pronto com " << quantidade_threads << " threads." << endl;
        while (true) {
            // A thread principal recebe conexoes; as threads do pool executam o CRUD.
            BOOL conectado = ConnectNamedPipe(pipe, nullptr);
            if (!conectado && GetLastError() != ERROR_PIPE_CONNECTED) {
                cerr << "Falha ao receber conexao.\n";
                sucesso = false;
                break;
            }
            char dados[MAX_REQUISICAO + 1];
            DWORD lidos = 0;
            BOOL recebido = ReadFile(pipe, dados, sizeof(dados), &lidos, nullptr);
            bool valido = recebido && lidos > 0 && lidos <= MAX_REQUISICAO;
            string pedido = valido ? string(dados, lidos) : "";
            // PARAR encerra a recepcao; os pedidos ja aceitos ainda serao concluidos.
            if (pedido == "PARAR")
                break;
            // Recebe novos clientes enquanto o pool atende os anteriores.
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
                // Cada pedido adicionado libera um sinal para o pool.
                ReleaseSemaphore(tem_requisicao, 1, nullptr);
            } else {
                responder_e_fechar(pipe, "ERRO: requisicao invalida");
            }
            pipe = proximo;
        }
    } else {
        cerr << "Falha ao criar o pool.\n";
    }
    {
        GuardaMutex guarda(mutex_fila);
        encerrando = true;
    }
    // Conclui os pedidos antes de encerrar as threads.
    if (criadas) {
        // Acorda tambem as threads sem trabalho para verificarem o encerramento.
        ReleaseSemaphore(tem_requisicao, criadas, nullptr);
        // Aguarda todas as threads terminarem antes de fechar seus handles.
        WaitForMultipleObjects(criadas, threads, TRUE, INFINITE);
        for (int i = 0; i < criadas; ++i)
            CloseHandle(threads[i]);
    }
    CloseHandle(tem_requisicao);
    if (pipe != INVALID_HANDLE_VALUE) {
        if (sucesso)
            responder_e_fechar(pipe, "OK: servidor encerrado");
        else
            CloseHandle(pipe);
    }
    return sucesso ? 0 : 1;
}
