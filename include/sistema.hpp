#ifndef SISTEMA_HPP
#define SISTEMA_HPP

#include <string>
#include <stdexcept>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

constexpr const char* CAMINHO_PIPE = "\\\\.\\pipe\\projeto_m1_banco";
constexpr int NUM_THREADS = 4;
constexpr int MAX_REQUISICAO = 400;

struct Registro {
    int id;
    std::string nome;
};

class Mutex {
    HANDLE handle;
public:
    Mutex() : handle(CreateMutexA(nullptr, FALSE, nullptr)) {
        if (!handle) throw std::runtime_error("Falha ao criar mutex");
    }
    ~Mutex() { CloseHandle(handle); }
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    void bloquear() {
        DWORD resultado = WaitForSingleObject(handle, INFINITE);
        if (resultado != WAIT_OBJECT_0 && resultado != WAIT_ABANDONED)
            throw std::runtime_error("Falha ao adquirir mutex");
    }
    void liberar() { ReleaseMutex(handle); }
};

// Libera o mutex automaticamente ao sair do bloco.
class GuardaMutex {
    Mutex& mutex;
public:
    explicit GuardaMutex(Mutex& m) : mutex(m) { mutex.bloquear(); }
    ~GuardaMutex() { mutex.liberar(); }
    GuardaMutex(const GuardaMutex&) = delete;
    GuardaMutex& operator=(const GuardaMutex&) = delete;
};

HANDLE criar_pipe(bool primeira = true);
void responder_e_fechar(HANDLE pipe, const std::string& resposta);
bool enviar_requisicao(const std::string& texto, std::string& resposta);
std::string executar_requisicao(const std::string& texto);

#endif
