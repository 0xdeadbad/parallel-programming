#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> 
#include <unistd.h> 
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

/* 
 * A macro MULTIPLE_TESTS define em tempo de compilação quantos testes multi threaded seram executados.
 * Caso ela não esteja definida via parâmetro -DMULTIPLE_TESTS=<número de testes> o padrão de quantidade de testes
 * será 1.
 */
#ifndef MULTIPLE_TESTS
#define MULTIPLE_TESTS 1
/*
 * Caso seja definido que o programa rodará múltiplos testes, também poderá ser definido quantas threads irá aumentar 
 * de um teste para outro.
 * Caso esta macro não esteja definida via parâmetro -DTHREADS_SKIP=<número de threads> o padrão de quantidade de threads puladas
 * será 3.
 */
#ifndef THREADS_SKIP
#define THREADS_SKIP 3
#endif
#endif

/*
 * Por padrão o programa irá utilizar de semáforos para criar uma barreira que sincronizará as threads, 
 * as fazendo iniciar a tarefa todas ao mesmo tempo.
 */
#define USE_SEMAPHORES

/*
 * Caso seja desejado, também pode-se definir a macro DONT_USE_SEMAPHORES via parâmetro do compilador -DDONT_USE_SEMAPHORES.
 * Assim as threads não irão esperar a sincronização e poderam começar fora de ordem, porém não afeterá o resultado final da conta
 * mas provavelmente o tempo que as threads leverão para fazer os cálculos.
 */
#ifdef DONT_USE_SEMAPHORES
#undef USE_SEMAPHORES
#endif

/*
 * Não explicarei tudo daqui para baixo, pois trata-se apenas de linhas de códigos triviais.
 * De qualquer modo mais informações podem ser encontradas no arquivo README.txt
 */

typedef struct {
    uint32_t *array;
    uint64_t subarray_count;
#ifdef USE_SEMAPHORES
    uint64_t soma;
    uint32_t thread_count, *counter;
    sem_t *_sem_count, *_sem_stop;
#endif
} thread_producer_arg_t;

int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return EXIT_FAILURE;
}

int fail_errno(const char *msg) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    return EXIT_FAILURE;
}

void* producer(void *arg) {
#ifdef USE_SEMAPHORES
    thread_producer_arg_t *st = (thread_producer_arg_t*)arg;
    sem_wait(st->_sem_count);
    if(*(st->counter) == st->thread_count - 1) {
        sem_post(st->_sem_count);
        for(uint32_t i = 0; i < st->thread_count - 1; i++)
            sem_post(st->_sem_stop);
    } else {
        (*(st->counter))++;
        sem_post(st->_sem_count);
        sem_wait(st->_sem_stop);
    }
    
#else
    uint64_t s;
    s = 0;
#endif
    for(uint64_t i = 0; i < ((thread_producer_arg_t*)arg)->subarray_count; i++)
#ifdef USE_SEMAPHORES
        ((thread_producer_arg_t*)arg)->soma += ((thread_producer_arg_t*)arg)->array[i];
    pthread_exit(arg);
#else
        s += ((thread_producer_arg_t*)arg)->array[i];
    free(arg);
    pthread_exit((void*) s);
#endif
}

int main(int argc, char **argv) {
    uint64_t soma, array_size;
    uint32_t chunk_size, *array, threads_count;
    pthread_t *threads;
    sem_t _sem_count, _sem_stop;
    struct timeval begin, end;
    char *end_ptr;
    long seconds, microseconds;
    double elapsed;

    errno = 0;

    if(argc != 3)
        return fail_errno("Quantidade de argumentos inválido.");

    array_size = strtoll(argv[1], &end_ptr, 10);
    if(errno == ERANGE || argv[1] == end_ptr)
        return fail_errno("Valor para tamanho da array inválido.");

    threads_count = strtol(argv[2], &end_ptr, 10);
    if(errno == ERANGE || argv[2] == end_ptr)
        return fail_errno("Valor para quantidade de threads inválido.");

    if(threads_count > array_size)
        return fail("A quantidade de threads não pode ser maior que o tamanho do vetor.");

    if(sem_init(&_sem_count, 0, 1) != 0)
        return fail_errno("Falha ao iniciar semáforo");
    if(sem_init(&_sem_stop, 0, 0) != 0)
        return fail_errno("Falha ao iniciar semáforo");

    array = calloc(array_size, sizeof(uint64_t));
    threads = NULL;

    for(uint64_t i = 0; i < array_size; i++)
        array[i] = rand() % 100000;

#ifdef CALC_SERIAL
    soma = 0;
    gettimeofday(&begin, 0);
    for(uint64_t i = 0; i < array_size; i++)
        soma += array[i];
    gettimeofday(&end, 0);

    seconds = end.tv_sec - begin.tv_sec;
    microseconds = end.tv_usec - begin.tv_usec;
    elapsed = seconds + microseconds*1e-6;

    printf("Utilizando a forma serial o algorítmo levou %lf segundos, resultando no valor %lu\n", elapsed, soma); 
#endif

#ifdef WRITE_RESULT_LOG
    const char *header = "THREADS			TEMPO EM SEGUNDOS			TAMANHO VETOR\n";
    char record[strlen(header)+100];

    FILE *f = fopen("./results.txt", "w");
    fwrite(header, sizeof(char), strlen(header), f);
#endif

#ifdef CALC_SERIAL
    sprintf(record, "%s			%lf					%lu\n", "serial", elapsed, array_size);
    fwrite(record, sizeof(char), strlen(record), f);
#endif

    for(uint32_t m = 0; m < MULTIPLE_TESTS; m++) {
#ifdef USE_SEMAPHORES
        uint32_t counter = 0;
#endif
        soma = 0;

        if(m > 0)
            threads_count += THREADS_SKIP;

        threads = realloc((void*)threads, sizeof(pthread_t) * threads_count);

        chunk_size = array_size / threads_count;
        for(uint32_t i = 0; i < threads_count; i++) {
            thread_producer_arg_t *arg = calloc(1, sizeof(thread_producer_arg_t));

#ifdef USE_SEMAPHORES
            arg->soma = 0;
            arg->counter = &counter;
            arg->thread_count = threads_count;
            arg->_sem_count = &_sem_count;
            arg->_sem_stop = &_sem_stop;
#endif

            arg->array = array+(i*chunk_size);
            arg->subarray_count = chunk_size;

            if(i == threads_count-1)
                if(arg->array + arg->subarray_count < array + array_size)
                    arg->subarray_count += (uint64_t)((array + array_size) - (arg->array + arg->subarray_count));

            if(pthread_create(threads+i, NULL, producer, (void*)arg))
                return fail_errno("Erro ao criar threads");
            
        }

        gettimeofday(&begin, 0);
        soma = 0;
        for(uint64_t i = 0; i < threads_count; i++) {
            void *s;
            pthread_join(threads[i], &s);
#ifdef USE_SEMAPHORES
            soma += ((thread_producer_arg_t*)s)->soma;
            sem_destroy(((thread_producer_arg_t*)s)->_sem_count);
            sem_destroy(((thread_producer_arg_t*)s)->_sem_stop);
            free(s);
#else
            soma += (uint64_t) s;
#endif
        }
        gettimeofday(&end, 0);

        seconds = end.tv_sec - begin.tv_sec;
        microseconds = end.tv_usec - begin.tv_usec;
        elapsed = seconds + microseconds*1e-6;

        printf("Utilizando a forma multi-threaded com %u threads, o algorítmo levou %lf segundos, resultando no valor %lu\n", threads_count, elapsed, soma);

#ifdef WRITE_RESULT_LOG
        sprintf(record, "%u			%lf					%lu\n", threads_count, elapsed, array_size);
        fwrite(record, sizeof(char), strlen(record), f);
#endif
    }

#ifdef WRITE_RESULT_LOG
        fclose(f);
#endif

    free(array);
    free(threads);

    pthread_exit(NULL);
}