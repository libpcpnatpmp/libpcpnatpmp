#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include "test_process_helper.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static int append_bytes(char **buffer, size_t *length, size_t *capacity,
                        const char *data, size_t data_len) {
    char *new_buffer;
    size_t new_capacity = *capacity;

    if (*length + data_len + 1 <= *capacity) {
        memcpy(*buffer + *length, data, data_len);
        *length += data_len;
        (*buffer)[*length] = '\0';
        return 0;
    }

    if (new_capacity == 0) {
        new_capacity = 256;
    }
    while (*length + data_len + 1 > new_capacity) {
        new_capacity *= 2;
    }

    new_buffer = (char *)realloc(*buffer, new_capacity);
    if (new_buffer == NULL) {
        free(*buffer);
        *buffer = NULL;
        *length = 0;
        *capacity = 0;
        return -1;
    }

    *buffer = new_buffer;
    *capacity = new_capacity;
    memcpy(*buffer + *length, data, data_len);
    *length += data_len;
    (*buffer)[*length] = '\0';
    return 0;
}

#ifdef WIN32
static char *build_command_line(int argc, char *argv[]) {
    size_t i;
    size_t length = 0;
    size_t capacity = 0;
    char *command_line = NULL;

    for (i = 0; i < (size_t)argc; i++) {
        const char *arg = argv[i];
        const char *p;
        int needs_quotes = (arg[0] == '\0');

        for (p = arg; *p != '\0'; p++) {
            if (*p == ' ' || *p == '\t' || *p == '"') {
                needs_quotes = 1;
                break;
            }
        }

        if (i > 0 && append_bytes(&command_line, &length, &capacity, " ", 1)) {
            return NULL;
        }

        if (!needs_quotes) {
            if (append_bytes(&command_line, &length, &capacity, arg,
                             strlen(arg))) {
                return NULL;
            }
            continue;
        }

        if (append_bytes(&command_line, &length, &capacity, "\"", 1)) {
            return NULL;
        }

        for (p = arg; *p != '\0';) {
            size_t backslash_count = 0;

            while (*p == '\\') {
                backslash_count++;
                p++;
            }

            if (*p == '"') {
                while (backslash_count-- > 0) {
                    if (append_bytes(&command_line, &length, &capacity, "\\\\",
                                     2)) {
                        return NULL;
                    }
                }
                if (append_bytes(&command_line, &length, &capacity, "\\\"",
                                 2)) {
                    return NULL;
                }
                p++;
            } else if (*p == '\0') {
                while (backslash_count-- > 0) {
                    if (append_bytes(&command_line, &length, &capacity, "\\\\",
                                     2)) {
                        return NULL;
                    }
                }
            } else {
                while (backslash_count-- > 0) {
                    if (append_bytes(&command_line, &length, &capacity, "\\",
                                     1)) {
                        return NULL;
                    }
                }
                if (append_bytes(&command_line, &length, &capacity, p, 1)) {
                    return NULL;
                }
                p++;
            }
        }

        if (append_bytes(&command_line, &length, &capacity, "\"", 1)) {
            return NULL;
        }
    }

    if (command_line == NULL) {
        command_line = (char *)malloc(1);
        if (command_line == NULL) {
            return NULL;
        }
        command_line[0] = '\0';
    }

    return command_line;
}

static char *read_handle_all(HANDLE handle) {
    char chunk[256];
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    for (;;) {
        DWORD bytes_read = 0;
        BOOL ok = ReadFile(handle, chunk, sizeof(chunk), &bytes_read, NULL);

        if (!ok || bytes_read == 0) {
            break;
        }
        if (append_bytes(&buffer, &length, &capacity, chunk,
                         (size_t)bytes_read) != 0) {
            return NULL;
        }
    }

    if (buffer == NULL) {
        buffer = (char *)malloc(1);
        if (buffer == NULL) {
            return NULL;
        }
        buffer[0] = '\0';
    }

    return buffer;
}
#else
static char *read_fd_all(int fd) {
    char chunk[256];
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    for (;;) {
        ssize_t bytes_read = read(fd, chunk, sizeof(chunk));

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            free(buffer);
            return NULL;
        }
        if (bytes_read == 0) {
            break;
        }
        if (append_bytes(&buffer, &length, &capacity, chunk,
                         (size_t)bytes_read) != 0) {
            return NULL;
        }
    }

    if (buffer == NULL) {
        buffer = (char *)malloc(1);
        if (buffer == NULL) {
            return NULL;
        }
        buffer[0] = '\0';
    }

    return buffer;
}
#endif

int test_process_start(test_process_t *process, const char *path, int argc,
                       char *argv[]) {
    memset(process, 0, sizeof(*process));

#ifdef WIN32
    HANDLE stdout_read = NULL;
    HANDLE stdout_write = NULL;
    HANDLE stderr_read = NULL;
    HANDLE stderr_write = NULL;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char *command_line;

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        goto fail;
    }
    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0)) {
        goto fail;
    }

    command_line = build_command_line(argc, argv);
    if (command_line == NULL) {
        goto fail;
    }

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;

    if (!CreateProcessA(path, command_line, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        free(command_line);
        goto fail;
    }
    free(command_line);

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    process->process_handle = pi.hProcess;
    process->thread_handle = pi.hThread;
    process->stdout_read_handle = stdout_read;
    process->stderr_read_handle = stderr_read;
    return 0;

fail:
    if (stdout_read != NULL) {
        CloseHandle(stdout_read);
    }
    if (stdout_write != NULL) {
        CloseHandle(stdout_write);
    }
    if (stderr_read != NULL) {
        CloseHandle(stderr_read);
    }
    if (stderr_write != NULL) {
        CloseHandle(stderr_write);
    }
    return -1;
#else
    int stdout_pipe[2];
    int stderr_pipe[2];
    char **child_argv;
    pid_t pid;

    if (pipe(stdout_pipe) != 0) {
        return -1;
    }
    if (pipe(stderr_pipe) != 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return -1;
    }

    child_argv = (char **)calloc((size_t)argc + 1, sizeof(*child_argv));
    if (child_argv == NULL) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return -1;
    }
    memcpy(child_argv, argv, (size_t)argc * sizeof(*child_argv));

    pid = fork();
    if (pid < 0) {
        free(child_argv);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        execv(path, child_argv);
        perror("execv");
        _exit(127);
    }

    free(child_argv);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    process->pid = (int)pid;
    process->stdout_fd = stdout_pipe[0];
    process->stderr_fd = stderr_pipe[0];
    return 0;
#endif
}

int test_process_try_wait(test_process_t *process, int *exited,
                          int *exit_code) {
    if (process->has_exit_code) {
        *exited = 1;
        *exit_code = process->exit_code;
        return 0;
    }

#ifdef WIN32
    DWORD wait_rc = WaitForSingleObject((HANDLE)process->process_handle, 0);

    if (wait_rc == WAIT_TIMEOUT) {
        *exited = 0;
        return 0;
    }
    if (wait_rc != WAIT_OBJECT_0) {
        return -1;
    }

    {
        DWORD child_exit_code = 0;

        if (!GetExitCodeProcess((HANDLE)process->process_handle,
                                &child_exit_code)) {
            return -1;
        }
        process->has_exit_code = 1;
        process->exit_code = (int)child_exit_code;
    }
#else
    int status = 0;
    pid_t wait_rc = waitpid((pid_t)process->pid, &status, WNOHANG);

    if (wait_rc == 0) {
        *exited = 0;
        return 0;
    }
    if (wait_rc < 0) {
        return -1;
    }

    process->has_exit_code = 1;
    if (WIFEXITED(status)) {
        process->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        process->exit_code = 128 + WTERMSIG(status);
    } else {
        process->exit_code = -1;
    }
#endif

    *exited = 1;
    *exit_code = process->exit_code;
    return 0;
}

int test_process_finish(test_process_t *process,
                        test_process_result_t *result) {
    memset(result, 0, sizeof(*result));

    if (!process->has_exit_code) {
#ifdef WIN32
        DWORD child_exit_code = 0;

        if (WaitForSingleObject((HANDLE)process->process_handle, INFINITE) !=
                WAIT_OBJECT_0 ||
            !GetExitCodeProcess((HANDLE)process->process_handle,
                                &child_exit_code)) {
            return -1;
        }
        process->has_exit_code = 1;
        process->exit_code = (int)child_exit_code;
#else
        int status = 0;

        if (waitpid((pid_t)process->pid, &status, 0) < 0) {
            return -1;
        }
        process->has_exit_code = 1;
        if (WIFEXITED(status)) {
            process->exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            process->exit_code = 128 + WTERMSIG(status);
        } else {
            process->exit_code = -1;
        }
#endif
    }

    result->exit_code = process->exit_code;

#ifdef WIN32
    result->stdout_data = read_handle_all((HANDLE)process->stdout_read_handle);
    result->stderr_data = read_handle_all((HANDLE)process->stderr_read_handle);
    CloseHandle((HANDLE)process->stdout_read_handle);
    CloseHandle((HANDLE)process->stderr_read_handle);
    CloseHandle((HANDLE)process->thread_handle);
    CloseHandle((HANDLE)process->process_handle);
#else
    result->stdout_data = read_fd_all(process->stdout_fd);
    result->stderr_data = read_fd_all(process->stderr_fd);
    close(process->stdout_fd);
    close(process->stderr_fd);
#endif

    if (result->stdout_data == NULL || result->stderr_data == NULL) {
        test_process_result_free(result);
        return -1;
    }

    memset(process, 0, sizeof(*process));
    process->has_exit_code = 1;
    return 0;
}

int test_process_run(const char *path, int argc, char *argv[],
                     test_process_result_t *result) {
    test_process_t process;

    if (test_process_start(&process, path, argc, argv) != 0) {
        return -1;
    }
    return test_process_finish(&process, result);
}

void test_process_result_free(test_process_result_t *result) {
    free(result->stdout_data);
    free(result->stderr_data);
    result->stdout_data = NULL;
    result->stderr_data = NULL;
}
