==== Observações ====

Meu computador É um Ryzen 5 3600, com 6 nÚcleos e 12 threads.

Meus resultados foram obtidos com os seguintes comandos no ambiente Linux Ubuntu 18.04:

gcc -Wall -pedantic -Werror -o main main.c -lpthread -DMULTIPLE_TESTS=10 -DTHREADS_SKIP=2 -DWRITE_RESULT_LOG -DCALC_SERIAL
./main 200000 4

Isso irá sobrescrever o arquivo results.txt com novos resultados.

==== Compilação do código e algumas explicações ====

=== Gerando o executável simples ===
Para simplesmente compilar o código basta rodar o comando:
gcc -Wall -pedantic -Werror -o main main.c -lpthread
Com este comando o executável será gerado da forma mais simples possível, sem funcionalidades adicionais.

Para executar o executável é da seguinte forma:
./main <tamanho da array> <quantidade de threads>
ou em caso de windows
./main.exe <tamanho da array> <quantidade de threads>

Esse comando irá rodar um teste com o tamanho de vetor e a quantidade de threads especificadas, apenas uma vez, retornando uma linha com o resultado.

A forma de execuçÃo do executÁvel É a mesma em todos os casos

=== Gerando executável que pode rodar múltiplos testes ===
Para compilar esse executável basta adicionar -DMULTIPLE_TESTS=<quantidade de testes> ao comando do gcc. Com essa informação adicional, o executável irá gerar a quantidade de testes especificadas.
Para cada teste adicional, serão adicionados 3 threads por padrão. Essa variável pode ser definida no gcc tbm através da variável THREADS_SKIP. -DTHREADS_SKIP=5 irá adicionar 5 threads para cada teste adicional.

exemplo de compilação:
gcc -Wall -pedantic -Werror -o main main.c -lpthread -DMULTIPLE_TESTS=5 -DTHREADS_SKIP=5

Com essa configuraçÃo o executÁvel irÁ fazer 5 testes, adicionado 5 threads para cada teste adicional.

=== Gerando executÁvel que gera arquivo com tabela de resultados ===
Para compilar esse executÁvel basta adicionar -DWRITE_RESULT_LOG ao comando do gcc. Ao final da execuçÃo dos testes, serÁ gerado um arquivo chamado results.txt com uma tabela de relaçÃo de threads, tempo e tamanho do vetor.

exemplo de compilação:
gcc -Wall -pedantic -Werror -o main main.c -lpthread -DMULTIPLE_TESTS=5 -DTHREADS_SKIP=5 -DWRITE_RESULT_LOG

=== Gerando executÁvel que mostra o resultado do tempo serial ===
Basta adicionar -DCALC_SERIAL ao gcc

exemplo de compilação:
gcc -Wall -pedantic -Werror -o main main.c -lpthread -DMULTIPLE_TESTS=5 -DTHREADS_SKIP=5 -DWRITE_RESULT_LOG -DCALC_SERIAL