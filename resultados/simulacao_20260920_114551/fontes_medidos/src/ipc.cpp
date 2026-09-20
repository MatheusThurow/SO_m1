#include "sistema.hpp"
#include <iostream>

HANDLE criar_pipe(bool primeira) {
    HANDLE pipe = CreateNamedPipeA(
        CAMINHO_PIPE, PIPE_ACCESS_DUPLEX | (primeira ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES, MAX_REQUISICAO + 1, MAX_REQUISICAO + 1, 5000, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
        std::cerr << "Falha ao criar pipe. Outro servidor esta aberto? Codigo: "
                  << GetLastError() << '\n';
    return pipe;
}

void responder_e_fechar(HANDLE pipe, const std::string& resposta) {
    DWORD escritos = 0;
    if (WriteFile(pipe, resposta.data(), static_cast<DWORD>(resposta.size()),
                  &escritos, nullptr)) FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

bool enviar_requisicao(const std::string& texto, std::string& resposta) {
    if (texto.empty() || texto.size() > MAX_REQUISICAO ||
        texto.find('\n') != std::string::npos) return false;
    HANDLE pipe = INVALID_HANDLE_VALUE;
    DWORD inicio = GetTickCount();
    // Cada conexao envia uma mensagem. Clientes concorrentes aguardam sua vez.
    do {
        pipe = CreateFileA(CAMINHO_PIPE, GENERIC_WRITE | GENERIC_READ, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) break;
        DWORD erro = GetLastError();
        if (erro != ERROR_PIPE_BUSY) {
            std::cerr << "Servidor indisponivel. Codigo: " << erro << '\n';
            return false;
        }
        WaitNamedPipeA(CAMINHO_PIPE, 100);
    } while (GetTickCount() - inicio < 5000);
    if (pipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Tempo limite esperando o servidor.\n";
        return false;
    }
    DWORD escritos = 0, lidos = 0;
    char dados[MAX_REQUISICAO + 1];
    resposta.clear();
    bool ok = WriteFile(pipe, texto.data(), static_cast<DWORD>(texto.size()),
                        &escritos, nullptr) && escritos == texto.size();
    if (ok) ok = ReadFile(pipe, dados, sizeof(dados), &lidos, nullptr) && lidos > 0;
    if (ok) resposta.assign(dados, lidos);
    CloseHandle(pipe);
    if (!ok) std::cerr << "Falha ao enviar requisicao ou receber resposta.\n";
    return ok;
}
