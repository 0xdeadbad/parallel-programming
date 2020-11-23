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

#ifndef MULTIPLE_TESTS
#define MULTIPLE_TESTS 1
#ifndef THREADS_SKIP
#define THREADS_SKIP 3
#endif
#endif

#define USE_SEMAPHORES

#ifdef DONT_USE_SEMAPHORES
#undef USE_SEMAPHORES
#endif

typedef struct {
    uint64_t *array, count;
#ifdef USE_SEMAPHORES
    uint64_t *counter, soma;
    sem_t *_semaphore;
    pthread_mutex_t *_mutex1, *_mutex2;
    pthread_cond_t *_cond;
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
    pthread_mutex_lock(((thread_producer_arg_t*)arg)->_mutex1);
    --(*(((thread_producer_arg_t*)arg)->counter));
    pthread_mutex_unlock(((thread_producer_arg_t*)arg)->_mutex1);
    
    pthread_mutex_lock(((thread_producer_arg_t*)arg)->_mutex2);
    pthread_cond_wait(((thread_producer_arg_t*)arg)->_cond, ((thread_producer_arg_t*)arg)->_mutex2);
    pthread_mutex_unlock(((thread_producer_arg_t*)arg)->_mutex2);
#else
    uint64_t s;
    s = 0;
#endif
    for(uint64_t i = 0; i < ((thread_producer_arg_t*)arg)->count; i++)
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
    uint64_t array_size, soma, chunk_size, *array, threads_count;
    pthread_t *threads;
    struct timeval begin, end;
    char *end_ptr;
    long seconds, microseconds;
    double elapsed;

#ifdef USE_SEMAPHORES
    sem_t _semaphore;
    pthread_mutex_t _mutex1, _mutex2;
    pthread_cond_t _cond;

    if(sem_init(&_semaphore, 0, 0) != 0)
        return fail_errno("Falha ao iniciar semáforo");
    if(pthread_mutex_init(&_mutex1, NULL) != 0)
        return fail_errno("Falha ao inicializar mutex 1");
    if(pthread_mutex_init(&_mutex2, NULL) != 0)
        return fail_errno("Falha ao inicializar mutex 2");
    if(pthread_cond_init(&_cond, NULL) != 0)
        return fail_errno("Falha ao inicializar cond");
#endif

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

    for(uint64_t m = 0; m < MULTIPLE_TESTS; m++) {
#ifdef USE_SEMAPHORES
        uint64_t counter;
#endif
        soma = 0;

        if(m > 0)
            threads_count += THREADS_SKIP;
        
#ifdef USE_SEMAPHORES
        counter = threads_count;
#endif

        threads = realloc((void*)threads, sizeof(pthread_t) * threads_count);
        memset((void*)threads, 0, sizeof(pthread_t) * threads_count);

        chunk_size = array_size / threads_count ;
        for(uint64_t i = 0; i < threads_count; i++) {
            thread_producer_arg_t *arg = calloc(1, sizeof(thread_producer_arg_t));

            arg->array = array+(i*chunk_size);
            arg->count = chunk_size;

#ifdef USE_SEMAPHORES
            arg->soma = 0;
            arg->_semaphore = &_semaphore;
            arg->_mutex1 = &_mutex1;
            arg->_mutex2 = &_mutex2;
            arg->counter = &counter;
            arg->_cond = &_cond;
#endif

            if(i == threads_count-1)
                if(arg->array + arg->count < array + array_size)
                    arg->count += (uint64_t)((array + array_size) - (arg->array + arg->count));

            if(pthread_create(threads+i, NULL, producer, (void*)arg))
                return fail_errno("Erro ao criar threads");
            
        }


#ifdef USE_SEMAPHORES
        while(counter > 0){}
        pthread_cond_broadcast(&_cond);
#endif

        gettimeofday(&begin, 0);
        soma = 0;
        for(uint64_t i = 0; i < threads_count; i++) {
            void *s;
            pthread_join(threads[i], &s);
#ifdef USE_SEMAPHORES
            soma += ((thread_producer_arg_t*)s)->soma;
            free(s);
#else
            soma += (uint64_t) s;
#endif
        }
        gettimeofday(&end, 0);

        seconds = end.tv_sec - begin.tv_sec;
        microseconds = end.tv_usec - begin.tv_usec;
        elapsed = seconds + microseconds*1e-6;

        printf("Utilizando a forma multi-threaded com %lu threads, o algorítmo levou %lf segundos, resultando no valor %lu\n", threads_count, elapsed, soma);

#ifdef WRITE_RESULT_LOG
        sprintf(record, "%lu			%lf					%lu\n", threads_count, elapsed, array_size);
        fwrite(record, sizeof(char), strlen(record), f);
#endif
    }

#ifdef WRITE_RESULT_LOG
        fclose(f);
#endif

    free(array);
    free(threads);
#ifdef USE_SEMAPHORES
    sem_destroy(&_semaphore);
    pthread_mutex_destroy(&_mutex1);
    pthread_mutex_destroy(&_mutex2);
    pthread_cond_destroy(&_cond);
#endif

    pthread_exit(NULL);
}