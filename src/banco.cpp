#include "sistema.hpp"
#include <sstream>
#include <vector>

using std::getline;
using std::istringstream;
using std::string;
using std::to_string;
using std::vector;
using std::ws;

// Banco em memória: os registros são perdidos quando o servidor encerra.
// O log registra operações, mas não recarrega o banco automaticamente.
static vector<Registro> tabela;
static Mutex mutex_banco;

static string executar(const string &texto) {
    // Separa operação, ID e nome; os comandos devem estar em maiúsculas.
    istringstream entrada(texto);
    string operacao, nome, sobra;
    int id;
    string token_id;
    if (!(entrada >> operacao >> token_id))
        return "ERRO: informe operacao e ID inteiro nao negativo";
    // Rejeita IDs negativos ou incompletos, como 2abc.
    istringstream numero(token_id);
    char caractere;
    if (!(numero >> id) || (numero >> caractere) || id < 0)
        return "ERRO: informe operacao e ID inteiro nao negativo";
    if (operacao != "INSERT" && operacao != "SELECT" && operacao != "UPDATE" &&
        operacao != "DELETE")
        return "ERRO: operacao desconhecida";
    if (operacao == "INSERT" || operacao == "UPDATE") {
        // Lê o restante da linha: o nome pode conter espaços.
        getline(entrada >> ws, nome);
        if (nome.empty() || nome.size() > 49)
            return "ERRO: nome deve ter de 1 a 49 bytes";
    } else if (entrada >> sobra) {
        return "ERRO: use apenas operacao e ID";
    }
    // Busca sequencial pelo ID dentro do vetor.
    auto registro = tabela.begin();
    while (registro != tabela.end() && registro->id != id)
        ++registro;
    // A verificação de duplicidade e a inserção estão sob o mesmo mutex.
    if (operacao == "INSERT") {
        if (registro != tabela.end())
            return "ERRO: ID ja existe";
        tabela.push_back({id, nome});
        return "OK: inserido";
    }
    // SELECT, UPDATE e DELETE exigem que o ID já exista.
    if (registro == tabela.end())
        return "ERRO: ID nao encontrado";
    if (operacao == "SELECT")
        return "OK: id=" + to_string(id) + " nome=" + registro->nome;
    if (operacao == "UPDATE") {
        registro->nome = nome;
        return "OK: atualizado";
    }
    // Se chegou aqui, o comando validado é DELETE.
    tabela.erase(registro);
    return "OK: removido";
}

string executar_requisicao(const string &texto) {
    // Apenas uma operação acessa o banco por vez, inclusive SELECT.
    // Uma consulta também precisa de proteção contra alterações simultaneas.
    GuardaMutex guarda(mutex_banco);
    return executar(texto);
}
