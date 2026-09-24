# Projeto M1 — Sistemas Operacionais

Banco de dados simulado em C++ para Windows. Cliente e servidor se comunicam por named pipe. O servidor usa threads, mutexes e semáforo para processar os pedidos.

## Compilar

Instale o g++ (MinGW) e deixe-o no PATH. Na pasta do projeto, execute:

```powershell
.\compilar.bat
```

## Executar

No primeiro PowerShell, inicie o servidor:

```powershell
.\servidor.exe 4
```

O número indica a quantidade de threads. O padrão é 4 e o limite é de 1 a 64.

Em outro PowerShell, na mesma pasta, abra o cliente:

```powershell
.\cliente.exe
```

## Comandos do cliente

```text
INSERT 1 Matheus
SELECT 1
UPDATE 1 Matheus Thurow
DELETE 1
```

Use comandos em maiúsculas. Cada registro tem um ID único e um nome.

- `CLS` ou `cls`: limpa a tela do cliente.
- `SAIR`: fecha apenas o cliente.
- `PARAR`: encerra o servidor após concluir os pedidos recebidos.

Os resultados aparecem no cliente e são registrados em `servidor.log`. O banco fica na memória e é perdido ao encerrar. O log é sobrescrito quando o servidor inicia novamente.

## Testes e simulações

Com Python 3 instalado e o servidor manual fechado:

```powershell
python .\testes\testar.py
```

Verifica operações, erros e concorrência. Para medir o desempenho:

```powershell
python .\testes\simular.py
```

Os scripts abrem seus próprios servidores. As medições ficam na pasta `resultados`.

## Material de apoio

- [Resultados e gráficos](resultados/simulacao_20260920_114551/RESULTADOS.md)

O relatório foi entregue separadamente.
