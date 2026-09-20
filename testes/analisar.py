"""Gera tabelas e graficos a partir de dados.json. Requer matplotlib."""
from collections import Counter
from pathlib import Path
import json
import statistics as st
import sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

origem = Path(sys.argv[1]).resolve()
dados = json.loads(origem.read_text(encoding="utf-8"))
pasta = origem.parent
execucoes = dados["execucoes"]
threads = dados["metodo"]["threads"]
volumes = dados["metodo"]["volumes"]
assert len(execucoes) == len(threads) * len(volumes) * dados["metodo"]["repeticoes"]
assert all(x["status"] == "aprovado" and x["erros"] == 0 for x in execucoes)
resumo = []
for n in volumes:
    base = st.median(x["segundos"] for x in execucoes if x["requisicoes"] == n and x["threads"] == 1)
    for t in threads:
        grupo = [x for x in execucoes if x["requisicoes"] == n and x["threads"] == t]
        tempos = [x["segundos"] for x in grupo]
        mediana = st.median(tempos)
        resumo.append({"requisicoes": n, "threads": t, "repeticoes": len(tempos),
            "mediana_s": mediana, "media_s": st.mean(tempos), "desvio_padrao_s": st.stdev(tempos),
            "minimo_s": min(tempos), "maximo_s": max(tempos),
            "vazao_req_s": n / mediana, "speedup": base / mediana})
(pasta / "resumo.json").write_text(json.dumps(resumo, indent=2), encoding="utf-8")

plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10,
    "axes.spines.top": False, "axes.spines.right": False})
cores = ["#2563eb", "#0d9488", "#d97706"]
fig, eixos = plt.subplots(1, 3, figsize=(13, 4.8))
for eixo, n, cor in zip(eixos, volumes, cores):
    linhas = [x for x in resumo if x["requisicoes"] == n]
    medianas = [x["mediana_s"] * 1000 for x in linhas]
    erros = [[(x["mediana_s"] - x["minimo_s"]) * 1000 for x in linhas],
             [(x["maximo_s"] - x["mediana_s"]) * 1000 for x in linhas]]
    eixo.errorbar(range(len(threads)), medianas, yerr=erros, fmt="o-", color=cor,
                  linewidth=2, capsize=5, markersize=7, zorder=3)
    for i, t in enumerate(threads):
        pontos = [x["segundos"] * 1000 for x in execucoes if x["requisicoes"] == n and x["threads"] == t]
        eixo.scatter([i + (j - 2) * .045 for j in range(len(pontos))], pontos,
                     color=cor, alpha=.4, s=22, zorder=2)
    eixo.set_xticks(range(len(threads)), threads)
    eixo.set_title(f"{n:,} inserções".replace(",", "."), fontweight="bold")
    eixo.set_xlabel("Threads no servidor")
    eixo.set_ylabel("Tempo total (ms)")
    eixo.set_ylim(bottom=0)
    eixo.grid(axis="y", alpha=.18)
fig.suptitle("Tempos medidos por volume e tamanho do pool", fontsize=15, fontweight="bold", y=.98)
fig.text(.5, .035, "4 clientes · 5 repetições por cenário · pontos = execuções · linha = mediana · barras = mínimo–máximo\nEscalas verticais independentes. Tempo inclui IPC, logs e encerramento; menor é melhor.",
         ha="center", fontsize=10, color="#475569")
fig.tight_layout(rect=[0, .14, 1, .91])
fig.savefig(pasta / "tempos.png", dpi=180, facecolor="white")
fig.savefig(pasta / "tempos.svg", facecolor="white")
plt.close(fig)

def br(numero, casas=3):
    return f"{numero:,.{casas}f}".replace(",", "_").replace(".", ",").replace("_", ".")

tabela = ["| Requisições | Threads | Mediana (s) | Média ± DP (s) | Mín.–máx. (s) | Vazão (req/s) | Aceleração |",
          "|---:|---:|---:|---:|---:|---:|---:|"]
for x in resumo:
    tabela.append(f"| {x['requisicoes']} | {x['threads']} | {br(x['mediana_s'],4)} | {br(x['media_s'],4)} ± {br(x['desvio_padrao_s'],4)} | {br(x['minimo_s'],4)}–{br(x['maximo_s'],4)} | {br(x['vazao_req_s'],0)} | {br(x['speedup'],2)}× |")

distribuicao = ["| Pool | Operações concluídas por cada thread (cinco lotes de 5.000) | Total |",
               "|---:|---|---:|"]
for t in threads:
    contagem = Counter()
    for x in execucoes:
        if x["threads"] == t and x["requisicoes"] == max(volumes):
            contagem.update({int(k): v for k, v in x["distribuicao_threads"].items()})
    distribuicao.append(f"| {t} | " + "; ".join(f"T{k}: {v}" for k, v in sorted(contagem.items())) + f" | {sum(contagem.values())} |")

maior = {x["threads"]: x for x in resumo if x["requisicoes"] == max(volumes)}
aumento = (maior[4]["mediana_s"] / maior[1]["mediana_s"] - 1) * 100
ambiente = dados["ambiente"]
hardware_path = pasta / "hardware.json"
hardware = json.loads(hardware_path.read_text(encoding="utf-8-sig")) if hardware_path.exists() else None
maquina = (f"{hardware['cpu']['Name']}, {hardware['cpu']['NumberOfCores']} núcleos e "
           f"{hardware['cpu']['NumberOfLogicalProcessors']} processadores lógicos; "
           f"{br(hardware['sistema']['TotalVisibleMemorySize']/1024/1024,2)} GiB de RAM utilizável pelo Windows."
           if hardware else f"{ambiente['processador']}; {ambiente['processadores_logicos']} processadores lógicos.")

relatorio = f"""# Simulações e resultados — Projeto M1

## Resultado principal

A rodada final executou **60 medições e 122.000 inserções**, com **zero operações
perdidas, duplicadas ou com resposta inesperada**. Quatro aquecimentos adicionais
(4.000 inserções) foram excluídos das estatísticas. O teste funcional também passou.

Para 5.000 inserções, a mediana foi **{br(maior[1]['mediana_s'])} s com uma thread**
e **{br(maior[4]['mediana_s'])} s com quatro threads**. O tempo com quatro threads
variou {br(aumento,1)}% em relação a uma thread nessa amostra (valor negativo indica redução). Não foi observado ganho consistente
de desempenho ao ampliar o pool nesta carga e nesta máquina.

## Ambiente de execução

- Data da rodada: {ambiente['data']}.
- Sistema: {ambiente['sistema']}.
- Hardware: {maquina}
- Compilador: {ambiente['compilador']}.
- Flags: `{ambiente['flags']}`.
- Python de automação: {ambiente['python']}.
- Implementação: C++/Win32, named pipe, CreateThread, CreateMutexA e CreateSemaphoreA.
- Servidor com banco em vetor, mutex único do banco e mutex separado para log.

## Método

Foram combinados pools de **1, 2, 4 e 8 threads** com lotes de **100, 1.000 e
5.000 INSERTs**, sempre com **quatro processos clientes**. Cada cenário teve
cinco repetições. A ordem dos 60 ensaios foi embaralhada com semente 20260919.
Antes deles, houve um aquecimento de 1.000 inserções para cada tamanho de pool.

Cada execução iniciou um servidor novo e, portanto, um banco vazio. Os IDs foram
distintos, de 0 a N−1, distribuídos entre os quatro clientes. O tamanho do lote é
o total entre os clientes, não a quantidade enviada por cada cliente. Não foram
inseridos atrasos artificiais nem alterada a lógica do banco para favorecer threads.
A execução sem argumento usa quatro threads. Cada cliente espera a resposta
completa de uma operação antes de enviar a seguinte. Consulte METODO.md, quando
presente, para as condições específicas e a proveniência da rodada.

O cronômetro `time.perf_counter()` começou antes de despachar os envios e parou
após o servidor sair em resposta a PARAR. Assim, mede **tempo total do lote**,
incluindo IPC, sincronização, operações, saída em log, coordenação Python e
encerramento. As chamadas de criação dos processos ocorreram antes do cronômetro,
mas algum trabalho residual de inicialização dos clientes pode estar incluído.
A leitura dos logs para validação ocorreu depois da medição. Este valor não é
latência individual de uma requisição nem tempo exclusivo de processamento do banco.

O log do servidor permaneceu ativo, com flush por resposta. A saída de console
do servidor foi redirecionada para outro arquivo; a saída normal do cliente foi
descartada. Rodar com terminais visíveis pode produzir tempos diferentes.

Somente as 60 medições do arquivo indicado entraram nas estatísticas abaixo.
Rodadas de outras versões são históricas e não foram combinadas com esta.
O computador não foi isolado de toda atividade de fundo do Windows.

## Tabela de resultados

{chr(10).join(tabela)}

Cada linha resume cinco medições. DP é o desvio padrão amostral. Vazão = N dividido
pela mediana do tempo. Aceleração = mediana com uma thread / mediana do cenário,
para o mesmo volume. Valor abaixo de 1 indica tempo maior que o da referência.

## Gráfico

![Tempos por volume e tamanho do pool](tempos.png)

As barras mostram mínimo e máximo, **não intervalo de confiança**. Os pontos
mostram as cinco execuções. As escalas verticais são independentes entre painéis.

## Verificação de funcionamento

Ao final de cada lote, o programa de simulação verificou todas as respostas do
log, a quantidade de linhas, a correspondência ID/nome, o conjunto completo de
IDs e a identificação das threads. Também exigiu código de saída zero de todos
os clientes e do servidor. Todos os lotes foram aprovados.

Separadamente, `testes/testar.py` validou INSERT, SELECT, UPDATE e DELETE, entradas
inválidas, consulta de registro inexistente, rejeição de duplicatas e encerramento.
Também verificou as respostas no cliente, uma sequência dependente e 24 clientes
lógicos concorrentes com IDs distintos, usando até 12 processos de cada vez.
Na disputa de 12 clientes pelo mesmo ID, houve uma inserção aceita e 11 rejeitadas.
Esses resultados estão em `teste_funcional.txt`. **Os tempos da tabela são apenas
de INSERT; não são benchmarks de SELECT, UPDATE ou DELETE.**

{chr(10).join(distribuicao)}

Essas contagens mostram que as threads do pool realizaram trabalho; não demonstram
execução simultânea dentro do banco. A seção crítica do banco é exclusiva.

## Análise e discussão

O aumento do pool não reduziu os tempos de maneira consistente. No maior lote,
a configuração com {min(maior, key=lambda t: maior[t]['mediana_s'])} threads apresentou a menor mediana entre as avaliadas. Nos lotes
menores, a variação entre repetições é grande em relação às diferenças entre
configurações; pequenas vantagens isoladas não sustentam uma conclusão geral.

A arquitetura ajuda a explicar o resultado: INSERT precisa adquirir um mutex
único, e a busca de ID no vetor também ocorre dentro dessa seção crítica. Logo,
as operações sobre o banco ficam serializadas. Além disso, cada comando abre uma
conexão no named pipe; a recepção é sequencial, mas cada pedido mantém sua
conexão até a thread devolver o resultado. As respostas também passam por um log
sincronizado com flush. Acrescentar threads adiciona coordenação
sem remover essas etapas serializadas. São explicações compatíveis com o código;
o experimento não mediu separadamente o custo de cada etapa.

O projeto demonstrou comunicação entre processos, uso de um pool e proteção dos
dados sob concorrência. **Concorrência correta não implica aceleração**. Não se
deve afirmar, com esses dados, que quatro threads processam o banco quatro vezes
mais rápido, nem generalizar os resultados para outros bancos e cargas.

As principais limitações são: cinco repetições por cenário, lotes curtos, uma
única máquina, banco inicialmente vazio, carga somente de inserção, compilação
sem otimização explícita e custos de IPC/log incluídos. Não foram medidos CPU,
memória, percentis de latência ou intervalos de confiança. Uma avaliação futura
pode usar lotes maiores e mais repetições, além de comparar cargas de consulta
e atualização com preparação e barreiras que preservem a ordem das dependências.

## Reprodução e evidências

1. Na pasta do projeto, execute `compilar.bat`.
2. Feche qualquer servidor manual e execute `python testes/simular.py`.
3. Uma nova pasta será criada em `resultados`, com `dados.json` e logs por execução.
4. Para recriar tabelas e gráficos, instale matplotlib no seu ambiente Python e
   execute `python testes/analisar.py CAMINHO_DO_DADOS_JSON`.

`dados.json` contém tempos sem arredondamento, ordem dos ensaios, distribuição por
thread e hashes SHA-256 dos fontes, executáveis e simulador. `resumo.json` contém
as estatísticas derivadas. As pastas `exec_*` guardam os logs individuais e as
pastas `aquecimento_*` guardam as execuções excluídas da análise. A rodada usada
neste texto é **{pasta.name}**.

Este texto é uma seção de resultados pronta para adaptação ao relatório do trio;
não substitui identificação dos autores, enunciado e demais elementos acadêmicos.
"""
(pasta / "RESULTADOS.md").write_text(relatorio, encoding="utf-8")
print(json.dumps(resumo, indent=2))
print("Relatorio e graficos: " + str(pasta))
