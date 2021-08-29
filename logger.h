#ifndef MY_AWESOME_BGP_LOGGER_H
#define MY_AWESOME_BGP_LOGGER_H

#include <cstdio>

enum log_level{
    ERROR,
    WARNING,
    NOTICE,
    INFO,
    DEBUG
};

template <typename ... Args>
void log(log_level level, const char *format, Args const & ... args){
    if(level > DEBUG){
        return;
    }
    if(level == log_level::ERROR){
        fprintf(stderr, "[ERROR] ");
        fprintf(stderr, format, args...);
        fprintf(stderr, "\n");
        return;
    }
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
    }
    printf(format, args ...);
    printf("\n");
}

#endif //MY_AWESOME_BGP_LOGGER_H
