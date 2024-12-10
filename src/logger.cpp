#include "logger.h"

#include "asserts.h"
#include "platform.h"
#include "util.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>

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

void logger_output(LogLevel log_level, const char* message, ...) {
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
            case 'b': {
                uint8_t* data = va_arg(arg_ptr, uint8_t*);
                size_t length = va_arg(arg_ptr, size_t);
                size_t printed_length = length > 256 ? 256 : length;

                static char str[1024];
                char* str_ptr = str;
                for (size_t i = 0; i < printed_length; i++) {
                    sprintf(str_ptr, " %02x", data[i]);
                    str_ptr += 3;
                }
                str_ptr[0] = '\0';
                
                out_ptr += sprintf(out_ptr, "%s", str);
                if (printed_length < length) {
                    out_ptr += sprintf(out_ptr, " ... (printed %zu of %zu bytes)", printed_length, length);
                }
                break;
            }
            case 'd': {
                fixed fp = va_arg(arg_ptr, fixed);
                out_ptr += sprintf(out_ptr, "%i.%i", fp.integer_part(), fp.fractional_value());
                break;
            }
            case 'x': {
                message++;
                switch (*message) {
                    case 'i': {
                        xy* v = va_arg(arg_ptr, xy*);
                        out_ptr += sprintf(out_ptr, "<%i, %i>", v->x, v->y);
                        break;
                    }
                    case 'd': {
                        xy_fixed* v = va_arg(arg_ptr, xy_fixed*);
                        out_ptr += sprintf(out_ptr, "<%i.%i, %i.%i>", v->x.integer_part(), v->x.fractional_value(), v->y.integer_part(), v->y.fractional_value());
                        break;
                    }
                }
                break;
            }
        } // End switch message
        
        message++;
    }

    va_end(arg_ptr);

    char log_message[MESSAGE_LENGTH];
    const char* LOG_LEVEL_PREFIXES[4] = { "ERROR", "WARN", "INFO", "TRACE" };
    sprintf(log_message, "[%f - %s]: %s\n", platform_get_absolute_time(), LOG_LEVEL_PREFIXES[log_level], out_message);

#   ifdef GOLD_DEBUG
        platform_console_write(log_message, log_level);
#   endif

    fprintf(logfile, "%s", log_message);
    fflush(logfile);
}

// Definition of function declared in asserts.h
void report_assertion_failure(const char* expression, const char* message, const char* file, int line) {
    logger_output(LOG_LEVEL_ERROR, "Assertion failure: %s, message: '%s', in file: %s, line %d\n", expression, message, file, line);
}