# Guia para explicar e testar o projeto M1

Este guia descreve a versão **Windows nativa** do projeto. Use-o junto com os
fontes: a ideia é conseguir explicar por que cada parte existe, não decorar frases.

> Atualização de 20/09/2026: respostas agora voltam ao cliente. O relatório Word/PDF e as simulações de 19/09 documentam a versão anterior, com confirmação de recebimento e resultados no log. Seus tempos não representam esta versão; consulte `docs/ATUALIZACAO_RETORNO_CLIENTE.md`.

## 1. O que o sistema faz

O programa simula um pequeno banco com registros formados por **ID e nome**.
O cliente recebe comandos digitados e os envia para outro processo, o servidor.
O servidor coloca os pedidos numa fila, e as threads do pool retiram e processam
esses pedidos. As respostas aparecem no próprio cliente, no terminal do servidor e em `servidor.log`.

Uma explicação inicial para a apresentação:

> Nosso projeto separa cliente e servidor em dois processos. Eles se comunicam
> por um named pipe do Windows. O servidor mantém uma fila de pedidos e um pool
> de threads. Usamos mutexes para proteger a fila, o banco e o log, e um semáforo
> para acordar as threads quando há trabalho. O banco é um vetor em memória.

**O banco não é um banco SQL real.** INSERT, SELECT, UPDATE e DELETE são nomes
de comandos de um protocolo simples implementado pelo próprio programa.

## 2. Como executar no Windows

Extraia o ZIP e abra um PowerShell na pasta `projeto_m1`. No Explorador de Arquivos,
você pode abrir a pasta, digitar `powershell` na barra de endereço e pressionar Enter.

Se os executáveis já estão na pasta, é possível usá-los diretamente. Para compilar
os fontes, o `g++` do MinGW deve estar instalado e disponível no PATH:

```powershell
g++ --version
.\compilar.bat
```

O script gera **dois executáveis**: `cliente.exe` e `servidor.exe`.

No primeiro terminal:

```powershell
.\servidor.exe
```

O servidor deve informar que está pronto com quatro threads. Mantenha esse terminal aberto.
Em outro PowerShell, na pasta do projeto:

```powershell
.\cliente.exe
```

Digite os comandos no cliente e acompanhe os resultados no servidor. Usar a mesma
pasta facilita localizar o log; o named pipe é identificado por um nome do Windows,
não por um arquivo nessa pasta.

Para mudar a quantidade de threads, encerre o servidor anterior e execute:

```powershell
.\servidor.exe 8
```

O programa aceita de **1 a 64 threads**; o padrão é quatro. Esse número é o tamanho
do pool e não inclui a thread principal de recepção do servidor.

## 3. Responsabilidade de cada arquivo

| Arquivo | O que explicar |
|---|---|
| `include/sistema.hpp` | Define constantes, `Registro`, declarações e as classes `Mutex` e `GuardaMutex`. |
| `src/cliente.cpp` | Contém o `main()` do cliente; lê linhas e chama `enviar_requisicao`. |
| `src/ipc.cpp` | Cria o named pipe e implementa conexão, envio e retorno do resultado. |
| `src/servidor.cpp` | Contém o `main()` do servidor, a fila, a função `atender`, o pool, o log e o encerramento. |
| `src/banco.cpp` | Mantém o vetor de registros e executa as quatro operações sob mutex. |
| `compilar.bat` | Compila os dois programas no Windows usando g++. |
| `Makefile` | Oferece uma alternativa de compilação para quem tem `mingw32-make`. |
| `testes/testar.py` | Verifica as operações, erros, concorrência e encerramento. |
| `testes/simular.py` | Executa os experimentos de desempenho e verifica os logs. |
| `testes/analisar.py` | Calcula estatísticas e gera gráficos a partir de `dados.json`. |

Não existe `threads.cpp` nesta versão: **o código do pool fica em `servidor.cpp`**.
Um arquivo `.cpp` é uma unidade de organização do código; ele não representa,
por si só, um processo ou uma thread.

## 4. Caminho de uma requisição

Exemplo: `INSERT 7 Joao`.

1. O cliente lê a linha com `std::getline`.
2. `enviar_requisicao` abre uma conexão com o named pipe usando `CreateFileA`.
3. `WriteFile` envia o comando ao servidor.
4. A thread principal do servidor recebe a mensagem com `ReadFile`.
5. O servidor adquire o mutex da fila e insere o pedido com `fila.push`.
6. `ReleaseSemaphore` sinaliza que há um pedido disponível.
7. O servidor prepara outra instância do pipe para receber novos clientes; o pedido guarda o handle da conexão original.
8. Uma thread do pool passa pela espera do semáforo e retira o pedido da fila.
9. Essa thread adquire o mutex do banco e executa a operação.
10. Depois de liberar o banco, adquire o mutex do log e escreve a resposta.
11. Fora dos mutexes, a thread envia a resposta pelo pipe do pedido e fecha essa conexão. O cliente imprime o resultado e pode enviar o próximo comando.

```text
cliente.exe
    |
    | named pipe do Windows
    v
servidor.exe: thread principal
    |
    v
fila compartilhada -- semáforo --> threads do pool
                                      |
                                      v
                              banco protegido por mutex
                                      |
                                      v
                               terminal e arquivo de log
```

**O cliente recebe o resultado da operação.** Cada chamada espera a resposta antes de continuar. Pedidos enviados por um mesmo cliente são concluídos em sequência; clientes diferentes podem ter pedidos atendidos pelo pool ao mesmo tempo.

## 5. Processos e threads

**Processo:** uma execução de um programa, com seu próprio espaço de memória.
`cliente.exe` e `servidor.exe` são processos diferentes; podem aparecer separadamente
no Gerenciador de Tarefas.

**Thread:** uma linha de execução dentro de um processo. As threads do servidor
compartilham a fila, o vetor do banco e o log.

**Pool:** um conjunto de threads criado na inicialização e reutilizado. Não é
criada uma thread nova para cada comando. Isso também limita a quantidade de
threads de atendimento ao valor escolhido.

O trecho de criação fica no `main()` de `servidor.cpp`:

```cpp
for (int i = 0; i < quantidade_threads; ++i) {
    numeros[i] = i + 1;
    threads[i] = CreateThread(nullptr, 0, atender,
                              &numeros[i], 0, nullptr);
    if (!threads[i]) break;
    ++criadas;
}
```

- `atender` é a função executada por cada thread.
- `&numeros[i]` é o endereço do número que identifica a thread no log.
- `threads[i]` armazena um **handle**, uma referência a um objeto do Windows.
- O vetor `numeros` permanece vivo no `main()` até todas as threads terminarem.
- Os números do log são identificadores definidos pelo programa, não IDs de threads do Windows.

## 6. Mutex e seção crítica

Uma **seção crítica** é um trecho que acessa um recurso compartilhado e precisa
de coordenação. Um **mutex** permite que somente uma thread por vez execute essa
parte usando aquele mesmo mutex.

Neste projeto há três proteções separadas:

| Mutex | Recurso protegido | Por que existe |
|---|---|---|
| `mutex_fila` | Fila de requisições e estado de encerramento | Evita alterações simultâneas sem coordenação. |
| `mutex_banco` | Vetor de registros | Protege buscas, inserções, atualizações e exclusões. |
| `mutex_log` | Arquivo e saída do servidor | Evita misturar respostas de threads diferentes. |

Exemplo do problema sem mutex: duas threads poderiam verificar que o ID 7 ainda
não existe e inserir esse mesmo ID. A **verificação e a inserção** precisam ficar
dentro da mesma proteção; proteger apenas `push_back` não resolveria a regra de unicidade.

Em `banco.cpp`:

```cpp
std::string executar_requisicao(const std::string& texto) {
    GuardaMutex guarda(mutex_banco);
    return executar(texto);
}
```

`GuardaMutex` bloqueia o mutex ao ser construída e o libera no destrutor,
quando o objeto sai do escopo. Isso inclui a saída por `return`. Esse padrão de
gerenciamento de recurso pelo tempo de vida do objeto é conhecido como **RAII**.

O mutex é real: a classe `Mutex` usa `CreateMutexA`, `WaitForSingleObject`,
`ReleaseMutex` e `CloseHandle` da API do Windows.

**SELECT também precisa da proteção:** enquanto uma thread lê o vetor, outra
poderia remover registros ou realocar sua memória ao inserir novos elementos.

O mutex da fila é liberado antes do processamento do banco. Assim, uma thread
não segura a fila inteira enquanto executa a operação. Após a operação, o mutex
do banco é liberado antes de adquirir o mutex do log.

## 7. O semáforo não tem o mesmo papel do mutex

O semáforo `tem_requisicao` começa em zero. Quando não há sinal disponível,
`WaitForSingleObject` bloqueia a thread até que possa continuar. Ao inserir um
pedido, a thread principal chama `ReleaseSemaphore` para disponibilizar um sinal.

```cpp
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
```

O semáforo sinaliza **trabalho disponível**; o mutex protege **o acesso à estrutura**.
O `while (true)` da função `atender` não fica consultando a fila continuamente:
quando faltam sinais, a chamada de espera bloqueia a thread, evitando espera ocupada.
Durante o encerramento, também são liberados sinais para acordar as threads e permitir a saída.

## 8. Como funciona o IPC no Windows

O nome usado é `\\.\pipe\projeto_m1_banco`. Trata-se de um **named pipe**:
um canal de comunicação mantido pelo sistema operacional.

| Função ou opção | Papel no projeto |
|---|---|
| `CreateNamedPipeA` | Cria o canal do lado do servidor. |
| `ConnectNamedPipe` | Aguarda uma conexão de cliente. |
| `CreateFileA` | Abre o pipe pelo nome do lado do cliente; não cria um arquivo comum de dados. |
| `WriteFile` / `ReadFile` | Escrevem e leem o pedido e a resposta. |
| `WaitNamedPipeA` | Aguarda uma oportunidade de conexão quando o canal está ocupado. |
| `DisconnectNamedPipe` | Desconecta o cliente para atender outra conexão. |
| `CloseHandle` | Libera a referência a um recurso do Windows. |
| `PIPE_TYPE_MESSAGE` | Mantém o canal em modo de mensagens. |
| `PIPE_ACCESS_DUPLEX` | Permite enviar o pedido e devolver o resultado. |
| `FILE_FLAG_FIRST_PIPE_INSTANCE` | Impede iniciar outro servidor com o mesmo nome de pipe. |

A thread principal recebe as conexões sequencialmente. Cada pedido leva sua própria instância do pipe até a thread do pool, enquanto outra instância fica disponível para novos clientes. Assim, esperar a resposta de um cliente não impede a recepção de outros.

`servidor.log` **não é o canal IPC de entrada**. Ele é a saída de resultados.
O cliente não lê arquivos repetidamente para se comunicar com o servidor.

## 9. Banco simulado e comandos

Cada registro tem um `int id` e uma `std::string nome`. Os registros ficam num
`std::vector<Registro>`. Uma busca percorre esse vetor procurando o ID.

| Operação | Regra |
|---|---|
| INSERT | Insere se o ID ainda não existir. |
| SELECT | Retorna ID e nome quando o registro existe. |
| UPDATE | Altera o nome de um registro existente. |
| DELETE | Remove um registro existente. |

O parser usa `std::istringstream` para separar os campos. O ID precisa ser inteiro
e não negativo; um token como `2abc` é rejeitado. INSERT e UPDATE exigem nome com
1 a 49 **bytes**, não necessariamente 49 caracteres em qualquer codificação.
SELECT e DELETE não aceitam argumentos adicionais. Cada comando tem limite de 400 bytes.

O vetor não é salvo como banco persistente. Ao fechar o servidor, seus registros
são perdidos. O log é sobrescrito quando um novo servidor inicia.

## 10. Demonstração manual das quatro operações

Com cliente e servidor abertos, envie **um comando por vez**. Espere a resposta
no próprio cliente. Ele espera automaticamente o resultado antes de enviar a próxima operação.

| Comando no cliente | Resultado esperado no cliente |
|---|---|
| `INSERT 1 Ana` | `OK: inserido` |
| `SELECT 1` | `OK: id=1 nome=Ana` |
| `UPDATE 1 Maria` | `OK: atualizado` |
| `SELECT 1` | `OK: id=1 nome=Maria` |
| `DELETE 1` | `OK: removido` |
| `SELECT 1` | `ERRO: ID nao encontrado` |

O servidor acrescenta o número da thread e o comando antes da resposta. Exemplo
ilustrativo de formato, cujo número da thread pode variar:

```text
[thread 2] INSERT 1 Ana => OK: inserido
```

Depois demonstre as validações, também esperando cada resultado:

```text
INSERT 7 Joao
INSERT 7 Outro
INSERT -1 Invalido
INSERT 2abc Nome
INSERT 2
SELECT 7 extra
```

A primeira linha insere o registro, se o ID 7 estiver livre. As demais devem
retornar erros: duplicação, ID inválido, ausência de nome ou argumento extra.
Esses erros são respostas esperadas da aplicação, não falhas do teste.

Para encerrar:

```text
PARAR
```

`SAIR` fecha somente o cliente. `PARAR` pede ao servidor que pare de receber,
conclua os pedidos já recebidos e aguarde todas as threads terminarem antes de
fechar os handles. Evite enviar novos pedidos enquanto o servidor encerra.

## 11. Teste automático funcional

Encerre o servidor manual e verifique se o Python está disponível:

```powershell
python --version
python .\testes\testar.py
```

O script cria uma pasta temporária e inicia o próprio servidor. Ele verifica:

1. As quatro operações e os resultados após inserir, atualizar e remover.
2. Rejeição de ID duplicado, registro inexistente e comandos inválidos.
3. Disputa de **12 clientes** que tentam inserir o mesmo ID: deve existir **uma
   aceitação e 11 rejeições**.
4. Conclusão de 20 inserções enviadas antes de PARAR.
5. Falha de conexão depois de o servidor encerrar.

Ao concluir com sucesso, o script imprime:

```text
OK: respostas no cliente, CRUD, validacao, concorrencia e encerramento.
```

O teste verifica que todas as mensagens anteriores a PARAR terminaram; não mede
quantas estavam efetivamente pendentes na fila no instante em que PARAR chegou.
Uma exceção ou código de saída diferente de zero indica que o teste não passou.

## 12. Simulação de desempenho

Com os programas compilados e sem servidor manual aberto:

```powershell
python .\testes\simular.py
```

O script compara:

- Pools de 1, 2, 4 e 8 threads.
- Lotes de 100, 1.000 e 5.000 INSERTs com IDs distintos.
- Quatro clientes por execução.
- Cinco repetições de cada combinação, em ordem embaralhada.

Isso produz **60 medições** e **122.000 inserções medidas**, além de quatro
aquecimentos que ficam fora das estatísticas. Cada execução começa com banco vazio.
Cada resposta é conferida no log para detectar operações ausentes, duplicadas ou inesperadas.

Os resultados são salvos em uma nova pasta em `resultados`. Os tempos incluem
IPC, sincronização, banco, logs, coordenação do script e encerramento do servidor.
Não representam o tempo de uma única requisição nem somente o custo do banco.

Para gerar tabelas e gráficos de uma nova rodada, o analisador precisa de matplotlib:

```powershell
python -m pip install matplotlib
python .\testes\analisar.py .\resultados\NOME_DA_NOVA_PASTA\dados.json
```

Substitua `NOME_DA_NOVA_PASTA` pelo nome informado pelo simulador. Os resultados
já entregues podem ser consultados sem instalar matplotlib nem repetir as medições.

### Como explicar os resultados já obtidos

A rodada usada no relatório é `simulacao_20260919_213111`. Na carga de 5.000 inserções:

| Threads | Mediana do lote |
|---:|---:|
| 1 | 0,2620 s |
| 2 | 0,2928 s |
| 4 | 0,3037 s |
| 8 | 0,2968 s |

**Não houve ganho consistente ao acrescentar threads.** O mutex permite apenas
uma operação por vez no banco; a recepção é serial e a escrita dos logs também
tem sincronização. Esses fatores são compatíveis com os resultados, mas não
foram medidos separadamente.

Quatro threads tiveram tempo mediano aproximadamente 15,9% maior que uma thread
nesse lote. Isso não significa que threads sejam sempre mais lentas: a conclusão
vale para esta implementação, esta carga e esta máquina. Os ensaios foram curtos,
com cinco repetições, sujeitos à variação do Windows.

**Mediana:** valor central das cinco medidas ordenadas. **Vazão:** quantidade de
pedidos dividida pelo tempo do lote. **Aceleração:** tempo mediano com uma thread
dividido pelo tempo mediano com o pool comparado; menor que 1 significa pior tempo.

## 13. Perguntas que o professor pode fazer

### Onde está o paralelismo?

Há várias threads reais no servidor, que o sistema operacional pode escalonar
em diferentes núcleos. Porém, o banco é uma seção crítica exclusiva. O projeto
mostra atendimento concorrente e compartilhamento protegido; não foi medida
a execução simultânea em núcleos nem demonstrada aceleração das operações do banco.

### Por que usar um pool?

Para reutilizar as threads e manter uma quantidade definida de trabalhadores,
em vez de criar uma thread para cada requisição.

### As variáveis globais fazem a comunicação entre cliente e servidor?

Não. As globais do servidor são compartilhadas apenas entre suas próprias threads.
Cliente e servidor têm espaços de memória separados e trocam mensagens pelo pipe.

### Por que mutex e semáforo juntos?

O mutex protege a integridade das estruturas. O semáforo sinaliza trabalho para
as threads que aguardam pedidos. Neste programa, eles têm funções diferentes.

### O log pode estar fora de ordem?

Sim. As threads podem terminar em ordem diferente. O mutex do log impede mistura
de linhas, mas não reordena as respostas pela ordem de chegada dos pedidos.

### O que aconteceria se retirássemos o mutex do banco?

O vetor poderia ser acessado e modificado sem coordenação, causando condições
de corrida, quebra da regra de IDs únicos e comportamento indefinido. Não é
correto afirmar que toda execução necessariamente falharia: problemas concorrentes
podem aparecer de maneira intermitente.

### Se só uma thread acessa o banco por vez, por que quatro threads não aceleram?

Porque o trabalho principal continua serializado. O pool não remove esse limite
e ainda envolve coordenação. As medições mostraram justamente essa limitação.

### Por que não usar diretamente Pthreads?

A versão foi desenvolvida para Windows nativo. Foram usadas APIs equivalentes
em finalidade: `CreateThread`, `CreateMutexA` e `CreateSemaphoreA`. Elas criam
threads e objetos reais do sistema operacional; não são a interface POSIX.
O enunciado permite bibliotecas equivalentes a Pthreads.

### O que prova que os testes passaram?

O teste automático confere respostas e códigos de saída. O simulador verifica
todas as linhas, IDs, nomes e quantidades; os dados brutos e logs ficam salvos.
Isso é evidência dos cenários executados, não uma prova de ausência de qualquer bug.

### O que melhorar depois?

Persistência do banco, fila com capacidade limitada, tratamento de falhas mais completo e estratégias de
leitura/escrita e log que reduzam a serialização. Toda mudança de desempenho
precisaria ser medida novamente.

## 14. Problemas comuns ao executar

| Sintoma | O que verificar |
|---|---|
| `g++` não reconhecido | MinGW instalado e pasta `bin` disponível no PATH. |
| `python` não reconhecido | Python instalado; dependendo da instalação, use `py` em vez de `python`. |
| `Servidor indisponivel` | Inicie primeiro `servidor.exe`. |
| Falha ao criar pipe | Verifique se outro servidor ou teste já está usando o mesmo nome. |
| Consulta não encontra registro recém-enviado | No mesmo cliente, a inserção termina antes da consulta. Entre clientes diferentes, coordene a ordem dos comandos. |
| Dados sumiram ao reiniciar | O banco fica somente na memória. |
| Resposta não aparece no cliente | O resultado é exibido no servidor e em `servidor.log`. |
| Teste falhou com servidor manual aberto | Encerre o servidor manual com PARAR e repita o teste. |

## 15. Roteiro curto de apresentação

1. **Objetivo:** explique o banco simulado e as quatro operações.
2. **Arquitetura:** mostre os dois executáveis, o named pipe, a fila e o pool.
3. **Demonstração:** execute INSERT, SELECT, UPDATE e DELETE, esperando cada resposta.
4. **Sincronização:** abra `atender` e `executar_requisicao`; explique semáforo e mutex.
5. **Teste:** encerre o servidor manual e rode `testes/testar.py`.
6. **Resultados:** mostre a tabela e explique por que mais threads não deram ganho consistente.
7. **Limites:** banco em memória, acesso serializado e protocolo simples.

Todos do grupo devem saber explicar o fluxo completo, pois o professor pode
escolher um representante. Uma divisão para ensaio é: Matheus apresenta objetivo
e cliente/IPC; Rodrigo explica servidor, fila e pool; Eduardo apresenta banco,
mutexes, testes e resultados. Essa é apenas uma sugestão de apresentação, não
uma declaração de quem implementou cada parte.

## Limpar a tela do cliente

Digite **cls** ou **CLS** e pressione Enter para limpar o terminal do cliente. Esse comando e local: nao envia requisicao ao servidor, nao apaga registros e nao limpa o arquivo servidor.log. Depois, continue digitando os comandos normalmente.


### Instrucoes fixas no cliente

No terminal interativo do Windows, as instrucoes ficam nas seis primeiras linhas e os comandos/resultados rolam abaixo. CLS limpa a area de uso e redesenha as instrucoes. Use uma janela de pelo menos 40 colunas e 12 linhas. Ao redimensionar, a tela e reorganizada no proximo comando. Em testes com saida redirecionada, a saida permanece texto simples.

Referencia da API de terminal: https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences#scrolling-margins
