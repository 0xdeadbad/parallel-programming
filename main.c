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
 * Por padrão o programa irá utilizar mutex cond para criar uma barreira que sincronizará as threads, 
 * as fazendo iniciar a tarefa todas ao mesmo tempo.
 */
#define USE_MUTEX_COND

/*
 * Caso seja desejado, também pode-se definir a macro DONT_USE_MUTEX_COND via parâmetro do compilador -DDONT_USE_MUTEX_COND.
 * Assim as threads não irão esperar a sincronização e poderam começar fora de ordem, porém não afeterá o resultado final da conta
 * mas provavelmente o tempo que as threads leverão para fazer os cálculos.
 */
#ifdef DONT_USE_MUTEX_COND
#undef USE_MUTEX_COND
#endif

/*
 * Não explicarei tudo daqui para baixo, pois trata-se apenas de linhas de códigos triviais.
 * De qualquer modo mais informações podem ser encontradas no arquivo README.txt
 */

typedef struct {
    uint32_t *array;
    uint64_t subarray_count, soma;
#ifdef USE_MUTEX_COND
    uint32_t thread_count, *counter;
    pthread_mutex_t *_barrier;
    pthread_cond_t *_cond;
    int id;
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
#ifdef USE_MUTEX_COND
    thread_producer_arg_t *st = (thread_producer_arg_t*)arg;
    pthread_mutex_lock(st->_barrier);
    (*(st->counter)) ++;
    if(*(st->counter) == st->thread_count) {
        st->counter = 0;
        pthread_cond_broadcast(st->_cond);
    } else 
        while(pthread_cond_wait(st->_cond, st->_barrier) != 0);
    pthread_mutex_unlock(st->_barrier);
#endif
    for(uint64_t i = 0; i < ((thread_producer_arg_t*)arg)->subarray_count; i++)
        ((thread_producer_arg_t*)arg)->soma += ((thread_producer_arg_t*)arg)->array[i];
    pthread_exit(arg);
}

int main(int argc, char **argv) {
    uint64_t soma, array_size;
    uint32_t chunk_size, *array, threads_count;
    pthread_t *threads;
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

    array = calloc(array_size, sizeof(uint32_t));
    threads = NULL;

    for(uint64_t i = 0; i < array_size; i++)
        array[i] = rand() % 100000;

#ifdef CALC_SERIAL
    soma = 0;
    gettimeofday(&begin, 0);
    for(uint32_t i = 0; i < array_size; i++)
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
#ifdef USE_MUTEX_COND
        uint32_t counter = 0;
        pthread_mutex_t _barrier;
        pthread_cond_t _cond;

        if(pthread_mutex_init(&_barrier, NULL) != 0)
            return fail_errno("Falha ao iniciar mutex _barrier");
        if(pthread_cond_init(&_cond, NULL) != 0)
            return fail_errno("Falha ao iniciar cond _cond");
#endif
        soma = 0;

        if(m > 0)
            threads_count += THREADS_SKIP;

        threads = realloc((void*)threads, sizeof(pthread_t) * threads_count);

        chunk_size = array_size / threads_count;
        for(uint32_t i = 0; i < threads_count; i++) {
            thread_producer_arg_t *arg = malloc(sizeof(thread_producer_arg_t));

#ifdef USE_MUTEX_COND
            arg->soma = 0;
            arg->counter = &counter;
            arg->thread_count = threads_count;
            arg->_barrier = &_barrier;
            arg->_cond = &_cond;
            arg->id = i;
#endif

            arg->array = array+(i*chunk_size);
            arg->subarray_count = chunk_size;

            if(i == threads_count-1)
                if(arg->array + arg->subarray_count < array + array_size)
                    arg->subarray_count += (uint64_t)((array + array_size) - (arg->array + arg->subarray_count));

            if(pthread_create(threads+i, NULL, producer, (void*)arg))
                return fail_errno("Erro ao criar threads");
            
        }

#ifdef USE_MUTEX_COND
        gettimeofday(&begin, 0);
        soma = 0;
        for(uint32_t i = 0; i < threads_count; i++) {
            void *s;
            pthread_join(threads[i], &s);
            soma += ((thread_producer_arg_t*)s)->soma;
            free(s);
        }
        gettimeofday(&end, 0);

        pthread_mutex_destroy(&_barrier);
        pthread_cond_destroy(&_cond);
#endif
    
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