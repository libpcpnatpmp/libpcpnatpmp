#ifndef TEST_PROCESS_HELPER_H_
#define TEST_PROCESS_HELPER_H_

typedef struct test_process_result {
    int exit_code;
    char *stdout_data;
    char *stderr_data;
} test_process_result_t;

typedef struct test_process {
#ifdef WIN32
    void *process_handle;
    void *thread_handle;
    void *stdout_read_handle;
    void *stderr_read_handle;
#else
    int pid;
    int stdout_fd;
    int stderr_fd;
#endif
    int has_exit_code;
    int exit_code;
} test_process_t;

int test_process_start(test_process_t *process, const char *path, int argc,
                       char *argv[]);
int test_process_try_wait(test_process_t *process, int *exited, int *exit_code);
int test_process_finish(test_process_t *process, test_process_result_t *result);
int test_process_run(const char *path, int argc, char *argv[],
                     test_process_result_t *result);
void test_process_result_free(test_process_result_t *result);

#endif /* TEST_PROCESS_HELPER_H_ */
