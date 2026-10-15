#include <unistd.h>
#include <errno.h>
#include <limits.h>

static int write_all(int fd, const char *buffer, size_t length){
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

static int print_number(long long value){
    char reversed[32];
    int length = 0;
    long long current = value;

    do {
        int digit = (int)(current % 10);
        if (digit < 0) {
            digit = -digit;
        }
        reversed[length] = (char)('0' + digit);
        length++;
        current /= 10;
    } while (current != 0);

    if (value < 0) {
        reversed[length] = '-';
        length++;
    }

    char output[32];

    for (int i = 0; i < length; i++) {
        output[i] = reversed[length - 1 - i];
    }

    output[length] = '\n';

    if (write_all(STDOUT_FILENO, output, (size_t)length + 1) == -1) {
        return 1;
    }
    return 0;
}

int main(void){

    char buffer[4096];
    long long sum = 0;
    long long number = 0;

    int negative = 0;
    int in_number = 0;
    int has_digit = 0;
    int has_values = 0;

    while (1) {
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (n == -1 && errno == EINTR) {
            continue;
        }
        if (n == -1) {
            return 1;
        }
        int end_of_file = 0;
        ssize_t count = n;

        if (n == 0) {
            end_of_file = 1;
            count = 1;
        }
        for (ssize_t i = 0; i < count; i++) {
            char c;

            if (end_of_file) {
                c = '\n';
            } else {
                c = buffer[i];
            }
            if (c >= '0' && c <= '9') {
                if (!in_number) {
                    in_number = 1;
                    number = 0;
                    negative = 0;
                    has_digit = 0;
                }

                int digit = c - '0';
                long long limit = INT_MAX;

                if (negative) {
                    limit = -(long long)INT_MIN;
                }
                if (number > (limit - digit) / 10) {
                    return 1;
                }

                number = number * 10 + digit;
                has_digit = 1;
            }
            else if (c == '-' || c == '+') {
                if (in_number) {
                    return 1;
                }

                in_number = 1;
                number = 0;
                has_digit = 0;
                negative = 0;

                if (c == '-') {
                    negative = 1;
                }
            }
            else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                if (in_number) {
                    if (!has_digit) {
                        return 1;
                    }
                    long long value = number;

                    if (negative) {
                        value = -number;
                    }
                    if (value > 0 && sum > LLONG_MAX - value) {
                        return 1;
                    }
                    if (value < 0 && sum < LLONG_MIN - value) {
                        return 1;
                    }

                    sum += value;
                    has_values = 1;
                    in_number = 0;
                    has_digit = 0;
                }
                if (c == '\n' && has_values) {
                    if (print_number(sum) != 0) {
                        return 1;
                    }
                    sum = 0;
                    has_values = 0;
                }
            }
            else {
                return 1;
            }
        }
        if (end_of_file) {
            break;
        }
    }
    return 0;
}
