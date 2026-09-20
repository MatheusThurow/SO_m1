# Fluxo no Windows

1. O cliente abre uma conexão e envia um comando.
2. O servidor prepara outra instância do named pipe para receber novos clientes.
3. O servidor enfileira um Pedido contendo texto e handle da conexão original.
4. O semáforo acorda uma thread do pool; o mutex protege a retirada da fila.
5. A thread executa a operação sob o mutex do banco.
6. Grava a resposta no console e no log sob o mutex do log.
7. Fora dos mutexes, devolve a resposta pela conexão desse pedido e fecha o handle.
8. O cliente imprime o resultado antes de ler o próximo comando.
9. PARAR aguarda o pool terminar e devolve OK: servidor encerrado.

Cada cliente aguarda sua própria resposta. Clientes diferentes podem trabalhar
concorrentemente, mas o mutex continua serializando o acesso ao banco.
CLS é local ao cliente; SAIR fecha somente o cliente.
