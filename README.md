# Projeto M1 - Windows nativo

Versao basica em C++ para Windows, sem Linux ou WSL. Cliente e servidor sao
executaveis distintos e usam IPC real por named pipe do Windows. O servidor
cria quatro threads com CreateThread e protege o vetor do banco com CreateMutexA.
Um semaforo acorda as threads quando chegam requisicoes. Todo o pool fica em
src/servidor.cpp. As respostas aparecem no cliente, no servidor e em servidor.log.

## Compilar

Com g++ (MinGW) no PATH, abra o PowerShell dentro desta pasta:

```powershell
.\compilar.bat
```

O script cria cliente.exe e servidor.exe com bibliotecas do compilador
incluidas estaticamente. Se tiver mingw32-make, tambem pode usar o Makefile.

Por padrao sao quatro threads. Para variar o pool, use, por exemplo,
`.\servidor.exe 8`. Sao aceitos valores de 1 a 64.
Os executaveis incluidos no ZIP permitem executar sem recompilar.

## Executar

Abra um PowerShell na pasta do projeto e execute:

```powershell
.\servidor.exe
```

Abra outro PowerShell na mesma pasta e execute:

```powershell
.\cliente.exe
```

Digite um comando por linha. O resultado aparece no cliente antes de ele enviar o proximo comando:

```text
INSERT 1 Ana Silva
SELECT 1
UPDATE 1 Maria Silva
SELECT 1
DELETE 1
SELECT 1
PARAR
```

INSERT e UPDATE recebem ID e nome sem aspas; SELECT e DELETE recebem apenas ID.
IDs sao inteiros nao negativos. Nomes aceitam espacos e ate 49 bytes.
Comandos usam letras maiusculas. SAIR fecha somente o cliente; PARAR encerra
o servidor depois de concluir os pedidos recebidos. Evite novos envios durante
o encerramento. INSERT rejeita ID repetido; as demais operacoes informam quando
o registro nao existe. Para uma demonstracao simples, use nomes sem acentos,
evitando diferencas de codificacao entre terminais do Windows.

Para enviar insercoes independentes de um arquivo, no PowerShell:

```powershell
Get-Content .\testes\insercoes.txt | .\cliente.exe
```

## Testes

Encerre qualquer servidor manual antes do teste (o nome do pipe e fixo).
Com Python 3 instalado, execute:

```powershell
python .\testes\testar.py
```

O teste inicia e encerra seu proprio servidor, usa um log em pasta temporaria,
verifica CRUD, entradas invalidas, disputa de 12 clientes pelo mesmo ID,
conclusao de pedidos recebidos e falha de conexao com o servidor desligado.

## Arquivos
 
Para repetir as simulacoes de desempenho (Python 3, sem servidor aberto):

```powershell
python .\testes\simular.py
```

Os dados e logs sao salvos em uma nova subpasta de resultados. Sao comparados
1, 2, 4 e 8 threads com quatro clientes, em cinco repeticoes de cada carga.

- include/sistema.hpp: constantes, registro, declaracoes e auxiliares de mutex.
- src/cliente.cpp: leitura e envio dos comandos.
- src/ipc.cpp: criacao do named pipe e envio das mensagens.
- src/servidor.cpp: recepcao, fila, pool, log e encerramento.
- src/banco.cpp: CRUD e protecao do banco.
- docs/fluxo.md: explicacao do funcionamento.

## Relacao com o enunciado

Esta adaptacao usa APIs nativas equivalentes em conceito: CreateThread no lugar
de Pthreads, CreateMutexA no lugar de pthread_mutex_t e named pipe Win32 no
lugar de FIFO POSIX. O enunciado cita bibliotecas equivalentes a Pthreads, mas
tambem lista primitivas POSIX nos requisitos tecnicos. Confirme com o professor
a aceitacao destas APIs do Windows; esta versao nao usa literalmente POSIX.

## Limites didaticos

O banco existe somente na memoria e se perde ao encerrar. O log e sobrescrito
a cada inicio. O pool nao garante ordem de conclusao e o mutex serializa o banco,
inclusive SELECT. A fila nao possui limite; o protocolo pressupoe clientes locais
usando o executavel fornecido. O servidor recebe uma conexao por vez e processa
pedidos pelo pool. Somente uma instancia pode usar o nome fixo do pipe.
O cliente recebe e imprime o resultado completo pelo named pipe.
Use PARAR para encerrar normalmente; fechamento forcado perde pedidos pendentes.

As simulacoes foram executadas e analisadas. Consulte
[resultados e graficos](resultados/simulacao_20260920_114551/RESULTADOS.md).
A rodada final teve 60 medicoes e 122.000 insercoes verificadas, sem erros.
O relatorio editavel esta em [docs/Relatorio_M1.docx](docs/Relatorio_M1.docx), com copia em PDF. Preencha os campos de identificacao pendentes e o link do repositorio publico antes da entrega.


## Guia para a apresentacao

Consulte o [guia de explicacao e testes](GUIA_EXPLICACAO_E_TESTES.md) para estudar o codigo, executar os testes e preparar a apresentacao ao professor.


Digite cls ou CLS no cliente para limpar a tela. O banco e o servidor.log permanecem intactos.


## Atualizacao do retorno (20/09/2026)

Cliente e servidor devem ser atualizados juntos. Veja [o complemento da documentacao](docs/ATUALIZACAO_RETORNO_CLIENTE.md). O Word/PDF foi atualizado com as medicoes de 20/09. Resultados de 19/09 sao historicos e nao entram na analise atual.
