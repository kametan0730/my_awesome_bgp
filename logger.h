#ifndef MY_AWESOME_BGP_LOGGER_H
#define MY_AWESOME_BGP_LOGGER_H

#include <cstdio>

extern uint8_t log_id;
extern uint8_t console_mode;

enum class log_level{
    ERROR,
    WARNING,
    NOTICE,
    INFO,
    DEBUG,
    TRACE
};

template <typename ... Args>
void console(const char *format, Args const & ... args){
    if(console_mode == 0){
        return;
    }
    printf("\e[m");
    printf("[CONSOLE] ");
    printf(format, args ...);
    printf("\n");
}

template <typename ... Args>
void log(log_level level, const char *format, Args const & ... args){
    if(console_mode == 1 and level > log_level::NOTICE){
        return;
    }
    if(level > log_level::TRACE){
        return;
    }
    if(level == log_level::ERROR){
        printf("\e[31m");
        fprintf(stderr, "[ERROR][%d] ", log_id);
        fprintf(stderr, format, args...);
        fprintf(stderr, "\n");
        return;
    }
    switch(level){
        case log_level::WARNING:
            printf("\e[33m");
            printf("[WARNING]");
            break;
        case log_level::NOTICE:
            printf("\e[35m");
            printf("[NOTICE]");
            break;
        case log_level::INFO:
            printf("\e[36m");
            printf("[INFO]");
            break;
        case log_level::DEBUG:
            printf("\e[32m");
            printf("[DEBUG]");
            break;
        case log_level::TRACE:
            printf("\e[34m");
            printf("[TRACE]");

            break;
        case log_level::ERROR:
            break;
    }
    printf("[%d] ", log_id);
    printf(format, args ...);
    printf("\n");
}

#endif //MY_AWESOME_BGP_LOGGER_H
