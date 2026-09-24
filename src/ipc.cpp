#include "sistema.hpp"
#include <iostream>

using std::cerr;
using std::string;

// Canal do Windows entre processos; duplex permite pedido e resposta.
HANDLE criar_pipe(bool primeira) {
    // Só a primeira instância impede outro servidor de usar o mesmo nome.
    // Instâncias seguintes permitem manter pedidos de clientes distintos em andamento.
    HANDLE pipe = CreateNamedPipeA(
        CAMINHO_PIPE, PIPE_ACCESS_DUPLEX | (primeira ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
        MAX_REQUISICAO + 1, MAX_REQUISICAO + 1, 5000, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
        cerr << "Falha ao criar pipe. Outro servidor esta aberto? Codigo: " << GetLastError()
                  << '\n';
    return pipe;
}

// Envia o resultado; FlushFileBuffers aguarda o cliente ler os dados do pipe.
void responder_e_fechar(HANDLE pipe, const string &resposta) {
    DWORD escritos = 0;
    if (WriteFile(pipe, resposta.data(), static_cast<DWORD>(resposta.size()), &escritos, nullptr))
        FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    // Libera o recurso do sistema operacional associado a esta conexão.
    CloseHandle(pipe);
}

// IPC: envia o pedido e recebe o resultado pelo named pipe.
bool enviar_requisicao(const string &texto, string &resposta) {
    if (texto.empty() || texto.size() > MAX_REQUISICAO || texto.find('\n') != string::npos)
        return false;
    HANDLE pipe = INVALID_HANDLE_VALUE;
    // Tenta conectar por até 5 segundos se o pipe estiver ocupado.
    // Esse limite não se aplica à leitura da resposta.
    DWORD inicio = GetTickCount();
    do {
        pipe = CreateFileA(CAMINHO_PIPE, GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0,
                           nullptr);
        if (pipe != INVALID_HANDLE_VALUE)
            break;
        DWORD erro = GetLastError();
        if (erro != ERROR_PIPE_BUSY) {
            cerr << "Servidor indisponivel. Codigo: " << erro << '\n';
            return false;
        }
        WaitNamedPipeA(CAMINHO_PIPE, 100);
    } while (GetTickCount() - inicio < 5000);
    if (pipe == INVALID_HANDLE_VALUE) {
        cerr << "Tempo limite esperando o servidor.\n";
        return false;
    }
    DWORD escritos = 0, lidos = 0;
    char dados[MAX_REQUISICAO + 1];
    resposta.clear();
    bool ok = WriteFile(pipe, texto.data(), static_cast<DWORD>(texto.size()), &escritos, nullptr) &&
              escritos == texto.size();
    // Aguarda o resultado completo antes de o cliente enviar outro comando.
    if (ok)
        ok = ReadFile(pipe, dados, sizeof(dados), &lidos, nullptr) && lidos > 0;
    if (ok)
        resposta.assign(dados, lidos);
    CloseHandle(pipe);
    if (!ok)
        cerr << "Falha ao enviar requisicao ou receber resposta.\n";
    return ok;
}
