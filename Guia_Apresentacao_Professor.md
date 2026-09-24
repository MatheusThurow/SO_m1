# Guia para apresentar o projeto M1

## 1. O que falar no começo

> Nosso projeto simula um banco de dados com ID e nome. O cliente envia comandos por um named pipe do Windows. O servidor usa um pool de threads para atender aos pedidos, protege os dados com mutex e registra os resultados em um log. O resultado também volta ao cliente.

O objetivo é demonstrar **comunicação entre processos, threads e sincronização**, além de analisar o desempenho. Mais threads não precisam ser mais rápidas em todos os cenários.

## 2. As três janelas da apresentação

**São três janelas, não três logs.** O sistema tem dois executáveis e um arquivo de log.

| Janela | O que faz | É necessária? |
|---|---|---|
| Cliente | Recebe os comandos digitados e mostra as respostas | Sim, para enviar pedidos |
| Servidor | Recebe os pedidos, processa com as threads e registra os resultados | Sim |
| Acompanhamento do log | Mostra o conteúdo de `servidor.log` enquanto ele é atualizado | Não; serve para a demonstração |

### Janela 1 — servidor

Abra um PowerShell e execute:

```powershell
cd C:\faculdade\SO\m1\projeto_m1\projeto_m1
.\servidor.exe 4
```

> Aqui iniciei o servidor com quatro threads de atendimento. A thread principal recebe os pedidos, e as threads do pool processam esses pedidos.

Sem número, o padrão é quatro. Para usar oito, execute ` .\servidor.exe 8` (sem o espaço inicial). O programa aceita de 1 a 64 threads no pool, além da thread principal. Para mudar a quantidade, encerre com PARAR e inicie novamente; os registros da memória serão perdidos.

### Janela 2 — cliente

Abra outro PowerShell e execute:

```powershell
cd C:\faculdade\SO\m1\projeto_m1\projeto_m1
.\cliente.exe
```

> Aqui digito os comandos. O cliente não acessa o banco diretamente: envia o pedido e espera a resposta pelo pipe.

### Janela 3 — acompanhar o arquivo de log

Abra outro PowerShell e execute:

```powershell
cd C:\faculdade\SO\m1\projeto_m1\projeto_m1
Get-Content .\servidor.log -Wait
```

> Esta janela apenas acompanha o arquivo. O servidor grava o log automaticamente, mesmo que eu não abra esta terceira janela.

`Ctrl+C` nessa terceira janela para de acompanhar o arquivo; não encerra o servidor.

## 3. Onde ficam os dados e o que fica no log

- **Banco:** um vetor na memória do servidor, contendo os registros atuais.
- **Log:** histórico textual dos pedidos processados e seus resultados.
- **Cliente:** apresenta a resposta recebida.

Exemplo:

```text
Cliente envia: INSERT 10 Matheus
Servidor: insere ID 10 e nome Matheus no vetor
Log registra: [thread 4] INSERT 10 Matheus => OK: inserido
Cliente recebe: OK: inserido
```

O número da thread pode ser diferente em cada execução.

O log reúne as operações dos clientes atendidos por aquele servidor: INSERT, SELECT, UPDATE e DELETE, incluindo erros de operação ou de entrada processados pelo banco. Não é um registro de todos os eventos possíveis: CLS e SAIR são locais ao cliente; PARAR também não gera uma linha de operação nesta versão.

### Como o código salva o log

```cpp
arquivo_log.open("servidor.log");
```

Abre ou cria o arquivo na pasta de trabalho em que o servidor foi iniciado. **Ao iniciar novamente, sobrescreve o log anterior.** Para preservar uma execução, copie o arquivo antes de reiniciar.

```cpp
{
    GuardaMutex guarda(mutex_log);
    arquivo_log << "[thread " << numero << "] "
                << pedido.texto << " => " << resposta << endl;
}
```

O mutex deixa apenas uma thread por vez escrever nesse bloco, evitando misturar mensagens. `endl` termina a linha e descarrega o buffer de escrita para o sistema operacional. Ao sair do bloco, `GuardaMutex` libera o mutex automaticamente.

**O log não é o banco:** fechar o servidor perde os registros da memória. O programa não lê o log para reconstruí-los.

## 4. Demonstração das operações

Digite no cliente, uma linha de cada vez:

| Comando | O que explicar |
|---|---|
| `INSERT 10 Matheus` | Insere um registro com ID 10 |
| `SELECT 10` | Retorna o ID e o nome armazenados |
| `UPDATE 10 Matheus Thurow` | Altera o nome do registro |
| `SELECT 10` | Confirma a alteração |
| `INSERT 10 Outro Nome` | Deve rejeitar o ID duplicado |
| `DELETE 10` | Remove o registro |
| `SELECT 10` | Deve informar que o registro não existe |

Use comandos em maiúsculas. O ID precisa ser um inteiro não negativo. INSERT e UPDATE exigem um nome de até 49 bytes.

Aponte nas três janelas: **pedido e resposta no cliente; atendimento no servidor; histórico no arquivo acompanhado.**

## 5. Inserção em massa para mostrar devagar

Execute no **PowerShell**, onde aparece `PS C:\...>`, e não dentro da tela de comandos do cliente. Mantenha o servidor aberto. Use outro terminal ou digite SAIR no cliente para voltar ao PowerShell.

```powershell
101..120 | ForEach-Object {
    "INSERT $_ TESTE EM MASSA"
    Start-Sleep -Milliseconds 500
} | .\cliente.exe
```

Insere 20 registros, de ID 101 a 120, com o nome TESTE EM MASSA e uma pausa de meio segundo entre pedidos. Use IDs livres para evitar duplicatas.

> O PowerShell gera as linhas e as entrega ao cliente como entrada. O cliente envia os pedidos ao servidor. A pausa existe apenas para acompanhar a demonstração; não representa o tempo de processamento do sistema.

Para excluir esses registros devagar:

```powershell
101..120 | ForEach-Object {
    "DELETE $_"
    Start-Sleep -Milliseconds 500
} | .\cliente.exe
```

Para inserir rapidamente, sem pausa:

```powershell
1001..1100 | ForEach-Object { "INSERT $_ Pessoa$_" } | .\cliente.exe
```

`$_` é o número atual da sequência. Esse último comando gera nomes Pessoa1001 até Pessoa1100.

**Esses lotes usam um cliente em sequência.** Não demonstram, sozinhos, vários pedidos simultâneos. O número de uma thread no log mostra quem participou, não prova execução simultânea em núcleos diferentes.

## 6. O que explicar de cada arquivo

| Arquivo | Explicação para o professor |
|---|---|
| `src/cliente.cpp` | Lê comandos, controla a tela e mostra o resultado enviado pelo servidor |
| `src/ipc.cpp` | Implementa o named pipe: conexão, envio, leitura da resposta e fechamento |
| `src/servidor.cpp` | Cria o pool, recebe pedidos, controla a fila, registra o log e encerra as threads |
| `src/banco.cpp` | Valida comandos e executa CRUD no vetor protegido pelo mutex |
| `include/sistema.hpp` | Define Registro, constantes, funções compartilhadas e auxiliares de mutex |
| `compilar.bat` / `Makefile` | Automatizam a compilação dos dois executáveis |
| `testes/testar.py` | Confere operações, erros, respostas, concorrência e encerramento |
| `testes/simular.py` | Mede lotes com diferentes quantidades de threads |
| `testes/analisar.py` | Calcula estatísticas e gera tabelas e gráficos |

As threads ficam em `servidor.cpp`; não há um arquivo `threads.cpp` separado nesta versão.

### Onde as threads são definidas e criadas no código

**Quantidade padrão — `include/sistema.hpp`, linha 19:**

```cpp
constexpr int NUM_THREADS = 4;
```

Essa constante define quatro threads de atendimento quando nenhum número é informado no terminal.

**Escolha da quantidade — `src/servidor.cpp`, a partir da linha 67:**

```cpp
int quantidade_threads = NUM_THREADS;
```

O servidor começa com o padrão. Se houver um argumento, lê `argv[1]` com `istringstream` e coloca o número em `quantidade_threads`. A validação aceita apenas valores inteiros de 1 a 64.

```powershell
.\servidor.exe 8
```

Nesse exemplo, `argv[1]` contém o texto `8`, que substitui o padrão. O número é escolhido ao iniciar o servidor, não durante a execução.

**Criação do pool — `src/servidor.cpp`, a partir da linha 102:**

```cpp
for (int i = 0; i < quantidade_threads; ++i) {
    numeros[i] = i + 1;
    threads[i] = CreateThread(nullptr, 0, atender, &numeros[i], 0, nullptr);
    if (!threads[i])
        break;
    ++criadas;
}
```

- O `for` tenta criar a quantidade solicitada.
- `CreateThread` cria uma thread real do Windows.
- `atender` é a função que cada thread executa.
- `&numeros[i]` passa o número usado para identificar a thread no log.
- `threads[i]` guarda o handle, usado para aguardar e liberar os recursos da thread.
- `criadas` conta as threads criadas com sucesso; o servidor verifica se o pool ficou completo.

**Trabalho das threads — função `atender()`, na linha 31 de `src/servidor.cpp`:**

Cada thread espera o semáforo, retira um pedido da fila, executa a operação no banco, registra o log e devolve a resposta ao cliente. Depois volta a esperar outro pedido. Por isso é um pool reutilizado, e não uma thread nova para cada comando.

O número escolhido é o tamanho do pool. A thread principal do servidor, responsável pela recepção, existe além dessas threads.

> A quantidade padrão fica no cabeçalho, pode ser alterada pelo argumento do terminal, e o servidor cria o pool com CreateThread. Todas as threads do pool executam a função atender.

As linhas indicadas correspondem à versão consultada; podem mudar se forem acrescentados comentários.

## 7. Conceitos que o professor pode perguntar

**O que é IPC?** Comunicação entre processos. Aqui ocorre por named pipe real do Windows, usando WriteFile e ReadFile.

**Processo e thread são iguais?** Não. Cliente e servidor são processos separados. As threads pertencem ao servidor e compartilham sua memória.

**Por que um pool?** Para criar as threads uma vez e reutilizá-las no atendimento, em vez de criar uma nova para cada pedido.

**O que faz a fila?** Guarda o comando e a conexão do cliente até uma thread retirar o pedido.

**Para que serve o semáforo?** Sinaliza trabalho disponível. Uma thread sem trabalho fica bloqueada, sem consultar a fila continuamente.

**Para que serve o mutex?** Protege um recurso compartilhado. Há mutexes separados para fila, banco e log.

**Por que SELECT usa mutex?** Porque outra thread poderia alterar ou remover registros enquanto a consulta lê o vetor.

**Onde está a proteção contra IDs duplicados?** A busca pelo ID e a inserção acontecem dentro da mesma seção protegida do banco.

**As operações do banco acontecem todas ao mesmo tempo?** Não. O pool atende pedidos concorrentemente, mas apenas uma thread por vez entra na seção protegida do banco.

**Por que o cliente recebe resposta e também existe log?** O log registra as operações, como permitido no enunciado. O retorno ao cliente facilita o uso e informa o resultado de cada pedido.

**As variáveis globais fazem o IPC?** Não. As variáveis do servidor são compartilhadas entre suas threads. Cliente e servidor trocam mensagens pelo pipe.

## 8. Como usar testar.py e simular.py

Os comandos desta seção devem ser executados no **PowerShell**, fora da tela interativa do cliente. É necessário ter Python instalado e os executáveis compilados.

Antes de começar, digite `PARAR` no cliente se houver um servidor manual aberto. Os scripts iniciam e encerram seus próprios servidores. Os dados em memória do servidor manual serão perdidos ao encerrá-lo.

Entre na pasta do projeto:

```powershell
cd C:\faculdade\SO\m1\projeto_m1\projeto_m1
```

### testar.py — verifica se o programa funciona

```powershell
python .\testes\testar.py
```

O script verifica:

- INSERT, SELECT, UPDATE e DELETE.
- Entradas inválidas, IDs duplicados e registros inexistentes.
- Respostas recebidas pelo cliente e registradas no log.
- Operações dependentes enviadas em sequência.
- Vários clientes concorrentes: na disputa de 12 clientes pelo mesmo ID, apenas uma inserção deve ser aceita.
- Encerramento do servidor e falha de conexão depois que ele encerra.

Se tudo passar, aparece:

```text
OK: respostas no cliente, CRUD, validacao, concorrencia e encerramento.
```

Se aparecer uma exceção ou erro, o teste não foi concluído com sucesso; leia a mensagem antes de apresentar o resultado como aprovado.

> Esse script verifica se o sistema funciona corretamente. Ele abre processos reais de cliente e servidor e confere os resultados esperados.

### simular.py — mede o desempenho

```powershell
python .\testes\simular.py
```

O script executa o teste funcional primeiro. Depois, compara:

| Configuração | Valores |
|---|---|
| Threads no servidor | 1, 2, 4 e 8 |
| Inserções por execução | 100, 1.000 e 5.000 |
| Clientes por execução | 4 |
| Repetições por cenário | 5 |

São **60 medições**, totalizando 122.000 inserções medidas, além de quatro aquecimentos. Cada execução começa com um servidor novo e banco vazio. Não é necessário abrir clientes manualmente.

Espere aparecer `CONCLUIDO`. O script salva uma nova pasta em `resultados`, com dados brutos e logs. Os aquecimentos ficam separados das medições usadas na análise.

> Esse script compara o tempo de atendimento com diferentes quantidades de threads. O tempo inclui comunicação, processamento, logs e encerramento; não é apenas o tempo do banco.

O `simular.py` salva os dados. **Quem gera tabelas e gráficos é o `analisar.py`:**

```powershell
python .\testes\analisar.py .\resultados\NOME_DA_PASTA\dados.json
```

Substitua `NOME_DA_PASTA` pela pasta informada pelo simulador. O analisador requer matplotlib; se necessário, instale com `python -m pip install matplotlib`.

Se `python` não for reconhecido, tente usar `py` no lugar de `python`.

### Por que esses scripts são importantes?

**testar.py:** ajuda a comprovar o funcionamento das operações e da concorrência. O professor não exigiu um arquivo com esse nome; ele é uma ferramenta de validação do projeto.

**simular.py:** produz as medições para as comparações. Essa parte é importante porque o trabalho pede simulações, resultados e análise. O professor não exige necessariamente Python, mas exige esses resultados no relatório.

Para a apresentação, mostre o teste funcional passando e explique os gráficos já gerados. Não é necessário repetir as 60 medições ao vivo, a menos que o professor peça. Esteja preparado para executar e explicar os scripts.

### Como explicar os resultados já registrados

As tabelas e os gráficos estão em `resultados/simulacao_20260920_114551`. Essa rodada usou uma cópia com nome do pipe isolado, conforme METODO.md; alterações posteriores de comentários e formatação não são novas medições.

Na rodada registrada, quatro threads tiveram mediana cerca de 18,9% menor que uma thread no lote de 5.000 inserções. Essa vantagem não se repetiu em todos os cenários. Há variação nas medições, e os resultados não estabelecem uma regra geral.

> Mais threads não garantem menor tempo, porque o banco continua protegido por um único mutex. A análise serve para explicar o comportamento observado, não para prometer aceleração em qualquer situação.

## 9. Como encerrar

- `CLS` ou `cls`: limpa a tela do cliente e mantém as instruções; não apaga banco ou log.
- `SAIR`: fecha apenas o cliente.
- `PARAR`: pede ao servidor que conclua os pedidos recebidos e encerre; o cliente sai após a resposta.

## 10. Frase para fechar a apresentação

> O cliente envia pelo pipe, o servidor coloca na fila, uma thread processa com o banco protegido, e o resultado vai para o log e volta ao cliente. Os testes verificam o funcionamento, e as simulações permitem analisar o efeito da quantidade de threads no desempenho.
