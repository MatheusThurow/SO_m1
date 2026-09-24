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

using std::runtime_error;
using std::string;

constexpr const char *CAMINHO_PIPE = "\\\\.\\pipe\\projeto_m1_banco";
// Valor padrão, substituído pelo argumento informado ao iniciar o servidor.
constexpr int NUM_THREADS = 4;
constexpr int MAX_REQUISICAO = 400;

// Cada registro possui um identificador e um nome.
struct Registro {
    int id;
    string nome;
};

// Encapsula o mutex real do Windows para proteger recursos compartilhados.
class Mutex {
    HANDLE handle;

  public:
    Mutex() : handle(CreateMutexA(nullptr, FALSE, nullptr)) {
        if (!handle)
            throw runtime_error("Falha ao criar mutex");
    }
    ~Mutex() {
        CloseHandle(handle);
    }
    // Impede copiar o objeto e fechar o mesmo handle duas vezes.
    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;
    void bloquear() {
        DWORD resultado = WaitForSingleObject(handle, INFINITE);
        if (resultado != WAIT_OBJECT_0 && resultado != WAIT_ABANDONED)
            throw runtime_error("Falha ao adquirir mutex");
    }
    void liberar() {
        ReleaseMutex(handle);
    }
};

// Bloqueia ao entrar no bloco e libera automaticamente ao sair.
class GuardaMutex {
    Mutex &mutex;

  public:
    explicit GuardaMutex(Mutex &m) : mutex(m) {
        mutex.bloquear();
    }
    // O destrutor executa ao sair do bloco, inclusive em retornos antecipados.
    ~GuardaMutex() {
        mutex.liberar();
    }
    GuardaMutex(const GuardaMutex &) = delete;
    GuardaMutex &operator=(const GuardaMutex &) = delete;
};

HANDLE criar_pipe(bool primeira = true);
void responder_e_fechar(HANDLE pipe, const string &resposta);
bool enviar_requisicao(const string &texto, string &resposta);
string executar_requisicao(const string &texto);

#endif
