#include "logger.h"

#include "asserts.h"
#include "defines.h"
#include "util.h"
#include "platform.h"
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <cstdio>

static FILE* logfile;

bool logger_init(const char* logfile_path) {
    logfile = fopen(logfile_path, "w");
    if (logfile == NULL) {
        log_error("Unable to open log file for writing.");
        return false;
    }

    return true;
}

void logger_quit() {
    fclose(logfile);
}

void logger_output(bool is_error, const char* message, ...) {
    const int MESSAGE_LENGTH = 32000;
    char out_message[MESSAGE_LENGTH];
    memset(out_message, 0, sizeof(out_message));

    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    char* out_ptr = out_message;
    while (*message != '\0') {
        if (*message != '%') {
            *out_ptr = *message;
            out_ptr++;
            message++;
            continue;
        }

        message++;
        if (*message == '\0') {
            break;
        }

        switch (*message) {
            case 'c': {
                out_ptr += sprintf(out_ptr, "%c", (char)va_arg(arg_ptr, int));
                break;
            }
            case 's': {
                out_ptr += sprintf(out_ptr, "%s", va_arg(arg_ptr, char*));
                break;
            }
            case 'i': {
                out_ptr += sprintf(out_ptr, "%i", va_arg(arg_ptr, int));
                break;
            }
            case 'u': {
                out_ptr += sprintf(out_ptr, "%u", va_arg(arg_ptr, unsigned int));
                break;
            }
            case 'f': {
                out_ptr += sprintf(out_ptr, "%f", va_arg(arg_ptr, double));
                break;
            }
            case 'd': {
                fixed fp = va_arg(arg_ptr, fixed);
                out_ptr += sprintf(out_ptr, "%i.%i", fp.integer_part(), fp.fractional_value());
                break;
            }
            case 'b': {
                uint8_t* data = va_arg(arg_ptr, uint8_t*);
                size_t length = va_arg(arg_ptr, size_t);

                static char str[1024];
                char* str_ptr = str;
                for (size_t i = 0; i < length; i++) {
                    sprintf(str_ptr, " %02x", data[i]);
                    str_ptr += 3;
                }
                str_ptr[0] = '\0';
                
                out_ptr += sprintf(out_ptr, "%s", str);
                break;
            }
            case 'r': {
                rect_t* r = va_arg(arg_ptr, rect_t*);
                out_ptr += sprintf(out_ptr, "<%i %i %i %i>", r->position.x, r->position.y, r->size.x, r->size.y);
                break;
            }
            case 'z': {
                out_ptr += sprintf(out_ptr, "%zu", va_arg(arg_ptr, size_t));
                break;
            }
        }

        message++;
    }
    va_end(arg_ptr);

    char log_message[MESSAGE_LENGTH];
    sprintf(log_message, "%s\n", out_message);

    if (is_error) {
        platform_console_write_error(log_message);
    } else {
        platform_console_write(log_message);
    }

    fprintf(logfile, "%s", log_message);
    fflush(logfile);
}

// Definition of function declared in asserts.h
void report_assertion_failure(const char* expression, const char* message, const char* file, int line) {
    logger_output(true, "Assertion failure: %s, message: '%s', in file: %s, line %d\n", expression, message, file, line);
}