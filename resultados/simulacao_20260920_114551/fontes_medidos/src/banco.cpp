#include "sistema.hpp"
#include <sstream>
#include <vector>

static std::vector<Registro> tabela;
static Mutex mutex_banco;

// Esta funcao so e chamada com o mutex do banco adquirido.
static std::string executar(const std::string& texto) {
    std::istringstream entrada(texto);
    std::string operacao, nome, sobra;
    int id;
    std::string token_id;
    if (!(entrada >> operacao >> token_id))
        return "ERRO: informe operacao e ID inteiro nao negativo";
    std::istringstream numero(token_id);
    char caractere;
    if (!(numero >> id) || (numero >> caractere) || id < 0)
        return "ERRO: informe operacao e ID inteiro nao negativo";
    if (operacao != "INSERT" && operacao != "SELECT" &&
        operacao != "UPDATE" && operacao != "DELETE")
        return "ERRO: operacao desconhecida";
    if (operacao == "INSERT" || operacao == "UPDATE") {
        std::getline(entrada >> std::ws, nome);
        if (nome.empty() || nome.size() > 49)
            return "ERRO: nome deve ter de 1 a 49 bytes";
    } else if (entrada >> sobra) {
        return "ERRO: use apenas operacao e ID";
    }
    auto registro = tabela.begin();
    while (registro != tabela.end() && registro->id != id) ++registro;
    if (operacao == "INSERT") {
        if (registro != tabela.end()) return "ERRO: ID ja existe";
        tabela.push_back({id, nome});
        return "OK: inserido";
    }
    if (registro == tabela.end()) return "ERRO: ID nao encontrado";
    if (operacao == "SELECT")
        return "OK: id=" + std::to_string(id) + " nome=" + registro->nome;
    if (operacao == "UPDATE") {
        registro->nome = nome;
        return "OK: atualizado";
    }
    tabela.erase(registro);
    return "OK: removido";
}

std::string executar_requisicao(const std::string& texto) {
    // Leituras tambem precisam de protecao contra escritas simultaneas.
    GuardaMutex guarda(mutex_banco);
    return executar(texto);
}
