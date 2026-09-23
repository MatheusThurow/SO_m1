"""Benchmark reproduzivel Windows. Execute apos compilar.bat, sem outro servidor."""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from collections import Counter
from datetime import datetime
import hashlib
import json
import os
import platform
import random
import re
import subprocess
import time

RAIZ = Path(__file__).resolve().parents[1]
CLIENTES = 4
THREADS = [1, 2, 4, 8]
VOLUMES = [100, 1000, 5000]
REPETICOES = 5
SEMENTE = 20260919


# Cada cenario inicia um servidor novo, com banco vazio e logs separados.
def executar_cenario(pasta, threads, volume):
    pasta.mkdir(parents=True)
    clientes = []
    servidor = None
    inicio = time.perf_counter()
    with (pasta / "console.log").open("w") as console:
        try:
            servidor = subprocess.Popen(
                [str(RAIZ / "servidor.exe"), str(threads)],
                cwd=pasta,
                stdout=console,
                stderr=console,
            )
            limite = time.monotonic() + 10
            caminho_log = pasta / "servidor.log"
            while True:
                if (
                    caminho_log.exists()
                    and "SERVIDOR PRONTO" in caminho_log.read_text()
                ):
                    break
                if servidor.poll() is not None or time.monotonic() > limite:
                    raise RuntimeError("Servidor nao iniciou; consulte console.log")
                time.sleep(0.005)
            # Inicia os clientes antes de medir o tempo.
            for _ in range(CLIENTES + 1):
                clientes.append(
                    subprocess.Popen(
                        [str(RAIZ / "cliente.exe")],
                        stdin=subprocess.PIPE,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.PIPE,
                        text=True,
                        cwd=pasta,
                    )
                )
            cargas = [
                "".join(f"INSERT {i} Pessoa{i}\n" for i in range(c, volume, CLIENTES))
                for c in range(CLIENTES)
            ]
            with ThreadPoolExecutor(max_workers=CLIENTES) as executor:
                # Mede o lote completo: envio, resposta, processamento, logs e encerramento.
                inicio = time.perf_counter()
                envios = [
                    executor.submit(cliente.communicate, carga, timeout=120)
                    for cliente, carga in zip(clientes, cargas)
                ]
                for cliente, envio in zip(clientes, envios):
                    _, erro = envio.result()
                    if cliente.returncode != 0:
                        raise RuntimeError("Cliente falhou: " + erro)
                _, erro = clientes[-1].communicate("PARAR\n", timeout=15)
                if clientes[-1].returncode != 0:
                    raise RuntimeError("Falha ao encerrar: " + erro)
                if servidor.wait(timeout=30) != 0:
                    raise RuntimeError("Servidor encerrou com erro")
                segundos = time.perf_counter() - inicio
            texto = caminho_log.read_text()
            linhas = texto.splitlines()[1:]
            padrao = r"\[thread (\d+)\] INSERT (\d+) Pessoa(\d+) => OK: inserido"
            distribuicao = Counter()
            ids = []
            for linha in linhas:
                resultado = re.fullmatch(padrao, linha)
                if resultado is None:
                    raise AssertionError("Resposta inesperada: " + linha)
                worker, registro, nome = map(int, resultado.groups())
                assert registro == nome and 1 <= worker <= threads
                ids.append(registro)
                distribuicao[worker] += 1
            assert len(ids) == volume and set(ids) == set(
                range(volume)
            ), "Perda ou duplicacao"
            return {
                "threads": threads,
                "requisicoes": volume,
                "clientes": CLIENTES,
                "segundos": segundos,
                "requisicoes_por_segundo": volume / segundos,
                "corretas": len(ids),
                "erros": 0,
                "distribuicao_threads": dict(sorted(distribuicao.items())),
                "evidencia": str(pasta.name),
                "status": "aprovado",
            }
        finally:
            for processo in clientes + ([servidor] if servidor else []):
                if processo.poll() is None:
                    processo.kill()
                    processo.wait(timeout=10)


def main():
    destino = RAIZ / "resultados" / datetime.now().strftime("simulacao_%Y%m%d_%H%M%S")
    destino.mkdir(parents=True)
    ambiente = {
        "data": datetime.now().astimezone().isoformat(),
        "sistema": platform.platform(),
        "processador": os.environ.get("PROCESSOR_IDENTIFIER"),
        "processadores_logicos": os.cpu_count(),
        "python": platform.python_version(),
        "compilador": subprocess.check_output(
            ["g++", "--version"], text=True
        ).splitlines()[0],
        "flags": "-std=c++11 -Wall -Wextra -Wpedantic -Iinclude -static (sem -O)",
        "hashes_sha256": {
            str(p.relative_to(RAIZ)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in list((RAIZ / "src").glob("*.cpp"))
            + list((RAIZ / "include").glob("*.hpp"))
            + [RAIZ / "cliente.exe", RAIZ / "servidor.exe", Path(__file__)]
        },
    }
    dados = {
        "ambiente": ambiente,
        "metodo": {
            "threads": THREADS,
            "volumes": VOLUMES,
            "clientes": CLIENTES,
            "repeticoes": REPETICOES,
            "semente": SEMENTE,
            "carga": "INSERT com IDs distintos; banco vazio a cada execucao",
            "tempo": "Envio pelos clientes ate saida do servidor apos PARAR; inclui IPC, CRUD, logs, coordenacao Python e encerramento; exclui lancamento inicial dos processos e leitura de validacao dos logs",
            "saida_servidor": "stdout redirecionado para arquivo, servidor.log ativo com flush por resposta",
        },
        "aquecimentos": [],
        "execucoes": [],
    }
    arquivo = destino / "dados.json"

    # Preserva as medicoes brutas para gerar as estatisticas depois.
    def salvar():
        arquivo.write_text(
            json.dumps(dados, indent=2, ensure_ascii=False), encoding="utf-8"
        )

    salvar()
    print("Resultados: " + str(destino), flush=True)
    funcional = subprocess.run(
        [os.sys.executable, str(RAIZ / "testes" / "testar.py")],
        capture_output=True,
        text=True,
        timeout=60,
    )
    (destino / "teste_funcional.txt").write_text(
        funcional.stdout + funcional.stderr, encoding="utf-8"
    )
    funcional.check_returncode()
    # Aquecimentos ficam registrados, mas nao entram nas estatisticas finais.
    for t in THREADS:
        dados["aquecimentos"].append(
            executar_cenario(destino / f"aquecimento_t{t}", t, 1000)
        )
        salvar()
        print(f"Aquecimento: {t} threads OK", flush=True)
    ordem = [
        (t, n, r) for t in THREADS for n in VOLUMES for r in range(1, REPETICOES + 1)
    ]
    # A semente permite repetir a mesma ordem embaralhada dos cenarios.
    random.Random(SEMENTE).shuffle(ordem)
    for indice, (t, n, r) in enumerate(ordem, 1):
        resultado = executar_cenario(
            destino / f"exec_{indice:02d}_t{t}_n{n}_r{r}", t, n
        )
        resultado.update({"ordem": indice, "repeticao": r})
        dados["execucoes"].append(resultado)
        salvar()
        print(
            f"{indice:02d}/{len(ordem)} | threads={t} | n={n} | {resultado['segundos']:.4f}s | OK",
            flush=True,
        )
    print("CONCLUIDO: " + str(arquivo), flush=True)


if __name__ == "__main__":
    main()
