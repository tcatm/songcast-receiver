#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#define log_lines 40
#define log_offset 10

FILE *log_file;

void log_init(void) {
    log_file = fopen("songcast-receiver.log", "a");

    if (log_file == NULL) {
        fprintf(stderr, "Could not open logfile\n");
        exit(1);
    }
}

void log_printf(const char* format, ...) {
    printf("\033[%d;%dr", log_offset, log_offset + log_lines);
    printf("\033[%d;0H", log_offset + log_lines);
    printf("\033[1S");

    va_list dup_args, args;
    va_start(args, format);
    va_copy(dup_args, args);
    vfprintf(stdout, format, args);
    printf("\033[K");
    fflush(stdout);
    va_end(args);

    vfprintf(log_file, format, dup_args);
    fprintf(log_file, "\n");
    fflush(log_file);
    va_end(dup_args);
}
