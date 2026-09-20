# Validacao da versao Windows

Validacao realizada em 20/09/2026, no Windows, com MinGW GCC 6.3.0.

## Compilacao

Executado compilar.bat com -std=c++11 -Wall -Wextra -Wpedantic -static.
Resultado: cliente.exe e servidor.exe gerados sem avisos ou erros.

## Teste de integracao executado

Executado testes/testar.py com Python 3.12, iniciando cliente.exe e servidor.exe
como processos reais e usando o named pipe do Windows.

- INSERT, SELECT, UPDATE e DELETE: aprovados.
- Rejeicao de ID duplicado e consulta de ID inexistente: aprovadas.
- Entradas invalidas (ID negativo, ID com letras, nome ausente e argumentos extras): aprovadas.
- 12 clientes concorrentes inserindo o mesmo ID: uma insercao aceita e 11 rejeitadas.
- 20 insercoes independentes seguidas de PARAR: todas registradas antes da saida.
- Tentativa de conexao depois do encerramento: falha reportada pelo cliente.
- Processo servidor encerrado com codigo zero.

Saida do teste:

```text
OK: respostas no cliente, CRUD, validacao, concorrencia e encerramento.
```

O teste confirma a conclusao das mensagens enviadas antes de PARAR; nao mede
quantos pedidos estavam na fila exatamente no momento do encerramento.
Esta verificacao e funcional: nao constitui benchmark de paralelismo, garantia
de desempenho ou validacao da aceitacao das APIs Win32 pelo professor.

## Atualizacao: simulacoes de desempenho

O servidor agora aceita um argumento opcional de 1 a 64 threads (padrao: quatro).
A versao recompilada passou novamente pelo teste funcional e por 60 medicoes
com pools de 1, 2, 4 e 8 threads. As 122.000 insercoes da rodada final foram
verificadas sem perdas ou duplicacoes. Consulte
../resultados/simulacao_20260920_114551/RESULTADOS.md para metodo, dados e limitacoes.

Também foram verificadas respostas no cliente, comandos dependentes em sequência e 24 clientes com IDs distintos (até 12 simultâneos). A rodada utilizou cópia com nome do pipe isolado, conforme METODO.md. A compilação e os testes automatizados não substituem a avaliação visual do cabeçalho interativo.
