# Atualização — resposta no cliente (20/09/2026)

Agora INSERT, SELECT, UPDATE e DELETE devolvem o resultado completo ao cliente.
Por exemplo, SELECT 1 mostra OK: id=1 nome=Ana ou ERRO: ID nao encontrado.
O log continua existindo. CLS/cls continua limpando apenas a tela do cliente.

## Implementação

A fila armazena Pedido { texto, pipe }. A thread que executa a operação escreve
sua resposta no pipe associado, fora dos mutexes, e fecha esse handle. A thread
principal mantém outra instância disponível para aceitar clientes. Apenas a
primeira instância usa FILE_FLAG_FIRST_PIPE_INSTANCE, impedindo outro servidor.
O cliente aguarda ReadFile e imprime a resposta antes de enviar outro comando.
PARAR recebe confirmação após o pool terminar. SAIR não é enviado ao servidor.

## Executar e testar

Encerre normalmente a versão antiga com PARAR antes de abrir a nova.
Atualize cliente.exe e servidor.exe juntos; os protocolos são diferentes.
Compile com .\compilar.bat; abra .\servidor.exe e .\cliente.exe em terminais separados.
Execute python testes/testar.py com o servidor manual fechado.
O teste verifica respostas CRUD no cliente, uma sequência dependente de comandos,
erros, concorrência e encerramento. O canal de teste pode ser isolado alterando
somente CAMINHO_PIPE numa cópia dos fontes antes de compilar.

## Relatório e medições atualizados

Word e PDF agora descrevem o retorno ao cliente e o log, com tabelas, gráfico e
análise da rodada simulacao_20260920_114551: 60 medições e 122.000 inserções.
O teste funcional também confere as respostas no cliente. Consulte METODO.md na
pasta de resultados para o canal IPC isolado e as limitações das medições.
Os resultados de 19/09 permanecem apenas como histórico, fora da análise atual.

### Instrucoes fixas no cliente

No terminal interativo do Windows, as instrucoes ficam nas seis primeiras linhas e os comandos/resultados rolam abaixo. CLS limpa a area de uso e redesenha as instrucoes. Use uma janela de pelo menos 40 colunas e 12 linhas. Ao redimensionar, a tela e reorganizada no proximo comando. Em testes com saida redirecionada, a saida permanece texto simples.

Referencia da API de terminal: https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences#scrolling-margins

