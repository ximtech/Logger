#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

// Uncomment to enable logging
//#define LOGGING_ENABLED

// define the maximum number of concurrent subscribers
#define LOGGER_MAX_SUBSCRIBERS 2

// maximum length of formatted log message
#define LOGGER_MAX_MESSAGE_LENGTH 256

typedef enum LogLevel {
    LOG_DEBUG_LEVEL = 24,
    LOG_INFO_LEVEL = 25,
    LOG_WARN_LEVEL = 26,
    LOG_ERROR_LEVEL = 27,
} LogLevel;

typedef void (*LoggerFunction)(LogLevel severity, const char *message);

typedef enum LoggerStatus {
    LOGGER_OK,
    LOGGER_ERROR_SUBSCRIBERS_EXCEEDED_MAX,
    LOGGER_ERROR_NOT_UNSUBSCRIBED,
} LoggerStatus;

#define LOG_DEBUG(...) logMessage(LOG_DEBUG_LEVEL, __VA_ARGS__)
#define LOG_INFO(...) logMessage(LOG_INFO_LEVEL, __VA_ARGS__)
#define LOG_WARN(...) logMessage(LOG_WARN_LEVEL, __VA_ARGS__)
#define LOG_ERROR(...) logMessage(LOG_ERROR_LEVEL, __VA_ARGS__)

LoggerStatus loggerSubscribe(LoggerFunction function, LogLevel threshold);
LoggerStatus loggerUnsubscribe(LoggerFunction function);
const char *logLevelToString(LogLevel severity);
void logMessage(LogLevel severity, const char *format, ...);
