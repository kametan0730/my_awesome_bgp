#ifndef MY_AWESOME_BGP_LOGGER_H
#define MY_AWESOME_BGP_LOGGER_H

#include <cstdio>

extern uint8_t log_id;

enum class log_level{
    ERROR,
    WARNING,
    NOTICE,
    INFO,
    DEBUG,
    TRACE
};

template <typename ... Args>
void log(log_level level, const char *format, Args const & ... args){
    if(level > log_level::TRACE){
        return;
    }
    if(level == log_level::ERROR){
        fprintf(stderr, "[%d][ERROR] ", log_id);
        fprintf(stderr, format, args...);
        fprintf(stderr, "\n");
        return;
    }
    printf("[%d]", log_id);
    switch(level){
        case log_level::WARNING:
            printf("[WARNING] ");
            break;
        case log_level::NOTICE:
            printf("[NOTICE] ");
            break;
        case log_level::INFO:
            printf("[INFO] ");
            break;
        case log_level::DEBUG:
            printf("[DEBUG] ");
            break;
        case log_level::TRACE:
            printf("[TRACE] ");
            break;
        case log_level::ERROR:
            break;
    }
    printf(format, args ...);
    printf("\n");
}

#endif //MY_AWESOME_BGP_LOGGER_H
