#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>

#else
    #include <pthread.h>
    #include <unistd.h>
#endif

// define the maximum number of concurrent subscribers
#ifndef LOGGER_MAX_SUBSCRIBERS
#define LOGGER_MAX_SUBSCRIBERS 8
#endif

// maximum length of formatted log message
#ifndef LOGGER_BUFFER_SIZE
#define LOGGER_BUFFER_SIZE 1024
#endif

#ifndef DISABLE_LOGGER_COLOR
#define USE_LOGGER_COLOR
#endif

#ifndef LOGGER_FILE_NAME_MAX_SIZE
#define LOGGER_FILE_NAME_MAX_SIZE 256  // with null character
#endif

typedef enum LogLevel {
    LOG_LEVEL_UNKNOWN,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
} LogLevel;

typedef struct LoggerEvent LoggerEvent;
typedef void (*LoggerFunction)(LoggerEvent *event, LogLevel severity);
typedef void (*LoggerCallback)(LogLevel severity, const char *message, uint32_t length);

typedef struct LogFile {
    uint8_t id;
    char *name;
    FILE *out;
    uint32_t size;
    uint32_t maxSize;
} LogFile;

struct LoggerEvent {
    LogFile *file;
    LogFile *backupFiles;
    uint8_t maxBackupFiles;

    LogLevel level;
    LoggerFunction function;
    LoggerCallback callback;
    const char *format;
    const char *tag;
    bool isSubscribed;
    char *buffer;
    va_list list;
};

#define LOG_TRACE(TAG, ...) logMessage(TAG, LOG_LEVEL_TRACE, __VA_ARGS__)
#define LOG_DEBUG(TAG, ...) logMessage(TAG, LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(TAG, ...) logMessage(TAG, LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARN(TAG, ...) logMessage(TAG, LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_ERROR(TAG, ...) logMessage(TAG, LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_FATAL(TAG, ...) logMessage(TAG, LOG_LEVEL_FATAL, __VA_ARGS__)

LoggerEvent *subscribeFileLogger(LogLevel threshold, const char *fileName, uint32_t maxFileSize, uint8_t maxBackupFiles);
LoggerEvent *subscribeConsoleLogger(LogLevel threshold);
LoggerEvent *subscribeCustomLogger(LogLevel threshold, LoggerCallback callback);

void loggerUnsubscribe(LoggerEvent *subscriber);
void loggerUnsubscribeAll();

const char *logLevelToString(LogLevel severity);
LogLevel stringToLogLevel(const char *severity);

void logMessage(const char *tag, LogLevel severity, const char *format, ...);
