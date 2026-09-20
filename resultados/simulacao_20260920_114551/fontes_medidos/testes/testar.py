"""Teste Windows: processos separados, named pipe e concorrencia real."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import subprocess
import tempfile
import time

raiz = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="m1-teste-") as pasta:
    pasta = Path(pasta)
    with (pasta / "console.log").open("w") as console:
        servidor = subprocess.Popen([str(raiz / "servidor.exe")], cwd=pasta,
                                    stdout=console, stderr=console)
        def esperar(condicao):
            limite = time.monotonic() + 5
            while time.monotonic() < limite:
                if condicao():
                    return
                if servidor.poll() is not None:
                    raise AssertionError("Servidor encerrou inesperadamente")
                time.sleep(0.02)
            raise AssertionError("Tempo limite excedido")

        def log():
            caminho = pasta / "servidor.log"
            return caminho.read_text() if caminho.exists() else ""

        def enviar(comando):
            return subprocess.run([str(raiz / "cliente.exe")], input=comando + "\n",
                           text=True, cwd=pasta, capture_output=True,
                           check=True, timeout=10)

        def verificar(comando, resposta):
            resultado = enviar(comando)
            assert resposta in resultado.stdout, resultado.stdout
            esperar(lambda: comando + " => " + resposta in log())

        try:
            esperar(lambda: "SERVIDOR PRONTO" in log())
            sequencia = enviar("INSERT 50 Antes\nSELECT 50\nUPDATE 50 Depois\nSELECT 50\nDELETE 50\nSELECT 50")
            assert sequencia.stdout.splitlines()[2:] == [
                "OK: inserido", "OK: id=50 nome=Antes", "OK: atualizado",
                "OK: id=50 nome=Depois", "OK: removido", "ERRO: ID nao encontrado"]
            verificar("INSERT 1 Ana", "OK: inserido")
            verificar("SELECT 1", "OK: id=1 nome=Ana")
            verificar("INSERT 1 Outra", "ERRO: ID ja existe")
            verificar("UPDATE 1 Maria", "OK: atualizado")
            verificar("SELECT 01", "OK: id=1 nome=Maria")
            verificar("DELETE 1", "OK: removido")
            verificar("SELECT 001", "ERRO: ID nao encontrado")
            verificar("INSERT -1 Invalido", "ERRO:")
            verificar("SELECT 2 extra", "ERRO:")
            verificar("INSERT 2", "ERRO:")
            verificar("INSERT 2abc Nome", "ERRO:")
            # Cada cliente deve receber sua propria resposta, sem troca de conexoes.
            def conferir_cliente(i):
                resultado = enviar(f"INSERT {i} Pessoa{i}\nSELECT {i}")
                assert resultado.stdout.splitlines()[2:] == [
                    "OK: inserido", f"OK: id={i} nome=Pessoa{i}"]
            with ThreadPoolExecutor(max_workers=12) as executor:
                list(executor.map(conferir_cliente, range(200, 224)))
            # Clientes concorrentes disputam o mesmo ID: apenas um pode inserir.
            clientes = [subprocess.Popen([str(raiz / "cliente.exe")], cwd=pasta,
                        stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                        stderr=subprocess.PIPE, text=True) for _ in range(12)]
            for cliente in clientes:
                cliente.stdin.write("INSERT 99 Concorrente\n")
                cliente.stdin.close()
            for cliente in clientes:
                assert cliente.wait(timeout=10) == 0
            esperar(lambda: log().count("INSERT 99 Concorrente =>") == 12)
            assert log().count("INSERT 99 Concorrente => OK: inserido") == 1
            assert log().count("INSERT 99 Concorrente => ERRO: ID ja existe") == 11
            parada = enviar("\n".join(f"INSERT {i} Pessoa" for i in range(100, 120)) + "\nPARAR")
            assert "OK: servidor encerrado" in parada.stdout
            assert servidor.wait(timeout=5) == 0
            assert log().count("Pessoa => OK: inserido") == 20
            desligado = subprocess.run([str(raiz / "cliente.exe")],
                input="SELECT 1\n", text=True, cwd=pasta, capture_output=True, timeout=10)
            assert desligado.returncode != 0
            print("OK: respostas no cliente, CRUD, validacao, concorrencia e encerramento.")
        finally:
            if servidor.poll() is None:
                servidor.terminate()
                servidor.wait(timeout=5)
