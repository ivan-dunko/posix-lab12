#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define ERROR_CODE -1
#define SUCCESS_CODE 0
#define PRINT_CNT 10
#define THREAD_MSG "routine\n"
#define MAIN_MSG "main\n"

#define lock pthread_mutex_lock
#define unlock pthread_mutex_unlock

typedef struct Context{
    pthread_cond_t *cond_var;
    pthread_mutex_t *cond_mtx;
    size_t thread_cnt;
    size_t thread_id;
    size_t *signal_thread;
    int return_code;
} Context;

void exitWithFailure(const char *msg, int err){
    errno = err;
    fprintf(stderr, "%.256s : %.256s", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

void assertSuccess(const char *msg, int errcode){
    if (errcode != SUCCESS_CODE)
        exitWithFailure(msg, errcode);
}

void assertInThreadSuccess(int errcode, Context *cntx){
    if (errcode != SUCCESS_CODE){
        cntx->return_code = errcode;
        pthread_exit((void*)ERROR_CODE);
    }
}

void lockSuccessAssertion(pthread_mutex_t *mtx, const char *msg){
    if (mtx == NULL)
        return;

    int err = lock(mtx);
    assertSuccess(msg, err);
}

void unlockSuccessAssertion(pthread_mutex_t *mtx, const char *msg){
    if (mtx == NULL)
        return;

    int err = unlock(mtx);
    assertSuccess(msg, err);
}

void condWaitSuccessAssertion(
    pthread_cond_t *cond, 
    pthread_mutex_t *cond_mtx,
    const char *err_msg){
    if (cond == NULL)
        return;
    
    int err = pthread_cond_wait(cond, cond_mtx);
    assertSuccess(err_msg, err);
}

void condSignalSuccessAssertion(
    pthread_cond_t *cond,
    const char *err_msg){
    if (cond == NULL)
        return;

    int err = pthread_cond_signal(cond);
    assertSuccess(err_msg, err);
}

void initMutexSuccessAssertion(
    pthread_mutex_t *mtx, 
    pthread_mutexattr_t *mtx_attr,
    const char *msg){
    if (mtx == NULL)
        return;

    int err = pthread_mutex_init(mtx, mtx_attr);
    assertSuccess(msg, err);
}

void destroyMutexSuccessAssertion(pthread_mutex_t *mtx, const char *msg){
    if (mtx == NULL)
        return;

    int err = pthread_mutex_destroy(mtx);
    assertSuccess(msg, err);
}

void initCondVarSuccessAssertion(
    pthread_cond_t *cond, 
    pthread_condattr_t *cond_attr,
    const char *err_msg){

    if (cond == NULL)
        return;

    int err = pthread_cond_init(cond, cond_attr);
    assertSuccess(err_msg, err);
}

void destroyCondVarSuccessAssertion(
    pthread_cond_t *cond,
    const char *err_msg){
    if (cond == NULL)
        return;

    int err = pthread_cond_destroy(cond);
    assertSuccess(err_msg, err);
}

void releaseResources(
    pthread_cond_t *cond,
    pthread_mutex_t *cond_mtx){

    destroyCondVarSuccessAssertion(cond, "release");
    destroyMutexSuccessAssertion(cond_mtx, "release");
}

int initContext(
    Context *cntx,
    pthread_cond_t *cond,
    pthread_mutex_t *cond_mtx,
    size_t thread_cnt,
    size_t thread_id,
    size_t *signal_thread){
    
    if (cntx == NULL || cond_mtx == NULL
        || cond == NULL || signal_thread == NULL)
        return EINVAL;

    cntx->cond_var = cond;
    cntx->cond_mtx = cond_mtx;
    cntx->return_code = SUCCESS_CODE;
    cntx->thread_cnt = thread_cnt;
    cntx->thread_id = thread_id;
    cntx->signal_thread = signal_thread;

    return SUCCESS_CODE;
}

void iteration(
    Context *cntx, 
    const char *msg, 
    const char *err_msg){
    
    printf(msg);
    condSignalSuccessAssertion(cntx->cond_var, err_msg);
    *cntx->signal_thread = (cntx->thread_id + 1) % cntx->thread_cnt;
    while (*cntx->signal_thread != cntx->thread_id)
        condWaitSuccessAssertion(cntx->cond_var, cntx->cond_mtx, err_msg);
}

#define THREAD_CNT 2

void *routine(void *data){
    if (data == NULL)
        exitWithFailure("routine", EINVAL);

    Context *cntx = (Context*)data;
    lockSuccessAssertion(cntx->cond_mtx, "routine");
    for (int i = 0; i < PRINT_CNT; ++i)
        iteration(cntx, THREAD_MSG,"routine");

    unlockSuccessAssertion(cntx->cond_mtx, "routine");
    pthread_exit((void*)SUCCESS_CODE);
}

int main(int argc, char **argv){
    pthread_t pid;

    pthread_mutex_t cond_mtx;
    pthread_cond_t cond;

    pthread_mutexattr_t mtx_attr;
    int err = pthread_mutexattr_init(&mtx_attr);
    assertSuccess("main", err);

    err = pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
    assertSuccess("main", err);
    initMutexSuccessAssertion(&cond_mtx, &mtx_attr, "main");
    initCondVarSuccessAssertion(&cond, NULL, "main");
    err = pthread_mutexattr_destroy(&mtx_attr);
    assertSuccess("main", err);

    //thread that is going to make print and signal
    size_t signal_thread = 0;

    Context cntx[THREAD_CNT];
    for (size_t i = 0; i < THREAD_CNT; ++i)
        initContext(&cntx[i], &cond, &cond_mtx, THREAD_CNT, i, &signal_thread);

    lockSuccessAssertion(cntx[0].cond_mtx, "main");

    err = pthread_create(&pid, NULL, routine, (void*)(&cntx[1]));
    if (err != SUCCESS_CODE)
        exitWithFailure("main", err);

    for (int i = 0; i < PRINT_CNT; ++i)
        iteration(&cntx[0], MAIN_MSG, "main");
    
    signal_thread = THREAD_CNT - 1;
    condSignalSuccessAssertion(cntx[0].cond_var, "main");
    unlockSuccessAssertion(cntx[0].cond_mtx, "main");
    err = pthread_join(pid, NULL);
    assertSuccess("main", err);

    releaseResources(&cond, &cond_mtx);

    pthread_exit((void*)(EXIT_SUCCESS));
}
