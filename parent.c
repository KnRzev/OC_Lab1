#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>

extern char **environ;

static int write_all(int fd, const char *buffer, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = write(fd, buffer + sent, length - sent);
        if (n == -1 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static void report_error(const char *message)
{
    size_t length = 0;
    while (message[length] != '\0') {
        length++;
    }
    write_all(STDERR_FILENO, message, length);
}

int main(void){
    const char prompt[] = "Enter filename: ";
    if (write_all(STDERR_FILENO, prompt, sizeof(prompt) - 1) == -1) {
        return 1;
    }

    char filename[4096];
    size_t length = 0;

    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == -1 && errno == EINTR) {
            continue;
        }
        if (n == -1) {
            report_error("Failed to read filename\n");
            return 1;
        }
        if (n == 0 || c == '\n') {
            break;
        }
        if (length == sizeof(filename) - 1) {
            report_error("Filename is too long\n");
            return 1;
        }
        if (c == '\0') {
            report_error("Invalid filename\n");
            return 1;
        }
        filename[length++] = c;
    }
    if (length == 0) {
        report_error("Filename is empty\n");
        return 1;
    }
    filename[length] = '\0';

    int filefd = open(filename, O_RDONLY);
    if (filefd == -1) {
        report_error("Cannot open file\n");
        return 1;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        report_error("Cannot create pipe\n");
        close(filefd);
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        report_error("Cannot fork\n");
        close(filefd);
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (pid == 0) {
        if (close(pipefd[0]) == -1) {
            report_error("Cannot close child pipe read end\n");
            _exit(1);
        }

        if (dup2(filefd, STDIN_FILENO) == -1) {
            report_error("Cannot redirect stdin\n");
            _exit(1);
        }
        if (filefd != STDIN_FILENO && close(filefd) == -1) {
            report_error("Cannot close child input file\n");
            _exit(1);
        }

        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            report_error("Cannot redirect stdout\n");
            _exit(1);
        }
        if (pipefd[1] != STDOUT_FILENO && close(pipefd[1]) == -1) {
            report_error("Cannot close child pipe write end\n");
            _exit(1);
        }

        char *const args[] = {"./child", NULL};
        execve("./child", args, environ);
        report_error("Cannot execute child\n");
        _exit(127);
    }

    int failed = 0;
    if (close(filefd) == -1) {
        report_error("Cannot close input file\n");
        failed = 1;
    }
    if (close(pipefd[1]) == -1) {
        report_error("Cannot close pipe write end\n");
        failed = 1;
    }

    char buffer[4096];
    if (!failed) {
        while (1) {
            ssize_t n = read(pipefd[0], buffer, sizeof(buffer));
            if (n == -1 && errno == EINTR) {
                continue;
            }
            if (n == -1) {
                report_error("Cannot read pipe\n");
                failed = 1;
                break;
            }
            if (n == 0) {
                break;
            }
            if (write_all(STDOUT_FILENO, buffer, (size_t)n) == -1) {
                report_error("Cannot write output\n");
                failed = 1;
                break;
            }
        }
    }

    if (close(pipefd[0]) == -1) {
        report_error("Cannot close pipe read end\n");
        failed = 1;
    }

    int status;
    pid_t result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        report_error("Cannot wait for child\n");
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        report_error("Child failed\n");
        return 1;
    }
    if (failed) {
        return 1;
    }
    return 0;
}
