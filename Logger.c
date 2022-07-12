#include "Logger.h"

#ifdef LOGGING_ENABLED
typedef struct LoggerSubscriber {
    LoggerFunction function;
    LogLevel threshold;
} LoggerSubscriber;


static LoggerSubscriber loggerSubscriberArray[LOGGER_MAX_SUBSCRIBERS] = {0};
static char messageBuffer[LOGGER_MAX_MESSAGE_LENGTH] = {0};

LoggerStatus loggerSubscribe(LoggerFunction function, LogLevel threshold) {
    for (uint32_t i = 0; i < LOGGER_MAX_SUBSCRIBERS; i++) {
        LoggerSubscriber *subscriber = &loggerSubscriberArray[i];
        if (subscriber->function == function) {  // already subscribed: update threshold and return immediately.
            subscriber->threshold = threshold;
            return LOGGER_OK;

        } else if (subscriber->function == NULL) {
            subscriber->function = function;
            subscriber->threshold = threshold;
            return LOGGER_OK;
        }
    }
    return LOGGER_ERROR_SUBSCRIBERS_EXCEEDED_MAX;
}

LoggerStatus loggerUnsubscribe(LoggerFunction function) {
    for (uint32_t i = 0; i < LOGGER_MAX_SUBSCRIBERS; i++) {
        LoggerSubscriber *subscriber = &loggerSubscriberArray[i];
        if (subscriber->function == function) {
            subscriber->function = NULL;
            return LOGGER_OK;
        }
    }
    return LOGGER_ERROR_NOT_UNSUBSCRIBED;
}

const char *logLevelToString(LogLevel severity) {
    switch (severity) {
        case LOG_DEBUG_LEVEL:
            return "DEBUG";
        case LOG_INFO_LEVEL:
            return "INFO";
        case LOG_WARN_LEVEL:
            return "WARNING";
        case LOG_ERROR_LEVEL:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

void logMessage(LogLevel severity, const char *format, ...) {
    va_list list;
    va_start(list, format);
    vsnprintf(messageBuffer, LOGGER_MAX_MESSAGE_LENGTH, format, list);
    va_end(list);

    for (uint32_t i = 0; i < LOGGER_MAX_SUBSCRIBERS; i++) {
        LoggerSubscriber *subscriber = &loggerSubscriberArray[i];
        if (subscriber->function != NULL && severity >= subscriber->threshold) {
            subscriber->function(severity, messageBuffer);
        }
    }
}

#else

LoggerStatus loggerSubscribe(LoggerFunction function, LogLevel threshold) { return LOGGER_OK; }
LoggerStatus loggerUnsubscribe(LoggerFunction function) { return LOGGER_OK; }
const char *logLevelToString(LogLevel severity) { return ""; }
void logMessage(LogLevel severity, const char *format, ...) {}

#endif