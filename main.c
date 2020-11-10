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

int printLine(size_t print_cnt, const char *str){
    for (size_t i = 0; i < print_cnt; ++i){
        int len = strlen(str);
        if (len == ERROR_CODE)
            return ERROR_CODE;
    
        int err = write(STDIN_FILENO, str, len);
        if (err == ERROR_CODE)
            return ERROR_CODE;
    }

    return SUCCESS_CODE;
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
    pthread_mutex_t *cond_mtx){
    
    if (cntx == NULL || cond_mtx == NULL
        || cond == NULL)
        return EINVAL;

    cntx->cond_var = cond;
    cntx->cond_mtx = cond_mtx;
    cntx->return_code = SUCCESS_CODE;

    return SUCCESS_CODE;
}

void *routine(void *data){
    if (data == NULL)
        exitWithFailure("routine", EINVAL);

    Context *cntx = (Context*)data;
    lockSuccessAssertion(cntx->cond_mtx, "routine");
    for (int i = 0; i < PRINT_CNT; ++i){
        printf(THREAD_MSG);
        condSignalSuccessAssertion(cntx->cond_var, "routine");
        condWaitSuccessAssertion(cntx->cond_var, cntx->cond_mtx, "routine");
    }

    unlockSuccessAssertion(cntx->cond_mtx, "routine");
    pthread_exit((void*)SUCCESS_CODE);
}

int main(int argc, char **argv){
    pthread_t pid;

    pthread_mutex_t cond_mtx, check_mtx;
    pthread_cond_t cond;

    initMutexSuccessAssertion(&cond_mtx, NULL, "main");
    initMutexSuccessAssertion(&check_mtx, NULL, "main");
    initCondVarSuccessAssertion(&cond, NULL, "main");
    
    Context cntx;
    initContext(&cntx, &cond, &cond_mtx);

    lockSuccessAssertion(cntx.cond_mtx, "main");

    int err = pthread_create(&pid, NULL, routine, (void*)(&cntx));
    if (err != SUCCESS_CODE)
        exitWithFailure("main", err);

    for (int i = 0; i < PRINT_CNT; ++i){
        printf(MAIN_MSG);
        condSignalSuccessAssertion(cntx.cond_var, "main");
        condWaitSuccessAssertion(cntx.cond_var, cntx.cond_mtx, "main");
    }
    
    condSignalSuccessAssertion(cntx.cond_var, "main");
    unlockSuccessAssertion(cntx.cond_mtx, "main");
    err = pthread_join(pid, NULL);
    assertSuccess("main", err);

    releaseResources(&cond, &cond_mtx);

    pthread_exit((void*)(EXIT_SUCCESS));
}
