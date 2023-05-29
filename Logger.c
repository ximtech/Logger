#include "Logger.h"

#define DEFAULT_FILE_SIZE 1048576L // 1 MB
#define FILE_TIMESTAMP_LENGTH (sizeof("_yyyy-MM-dd") + 5)    // For example: _2023-05-03 + additional designators for files with same name

static const char *LEVEL_STRINGS[] = {
        [LOG_LEVEL_UNKNOWN] = "UNKNOWN",
        [LOG_LEVEL_TRACE] = "TRACE",
        [LOG_LEVEL_DEBUG] = "DEBUG",
        [LOG_LEVEL_INFO] = "INFO",
        [LOG_LEVEL_WARN] = "WARN",
        [LOG_LEVEL_ERROR] = "ERROR",
        [LOG_LEVEL_FATAL] = "FATAL"};

#ifdef USE_LOGGER_COLOR
static const char *LEVEL_COLORS[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
static size_t formatColoredTagLevel(char *buffer, const char *tag, LogLevel severity, size_t timestampLength);
#endif

static LoggerEvent loggerSubscriberArray[LOGGER_MAX_SUBSCRIBERS] = {0};
static char messageBuffer[LOGGER_BUFFER_SIZE] = {0};

static LoggerEvent ERROR_EVENT = {.buffer = messageBuffer};

static bool isLockInitialized = false;
#if defined(_WIN32) || defined(_WIN64)
static CRITICAL_SECTION threadMutex;
#else
static pthread_mutex_t threadMutex;
#endif

static LoggerEvent *loggerSubscribe(LoggerEvent *event);
static void consoleCallback(LoggerEvent *event, LogLevel severity);
static void fileCallback(LoggerEvent *event, LogLevel severity);
static void customCallback(LoggerEvent *event, LogLevel severity);

static void initThreadLock();
static void lockThread();
static void unlockThread();

static bool isNeedToBeLogged(LogLevel level);
static uint32_t getLogFileSize(const char *fileName);
static bool rotateLogFiles(LoggerEvent *event);
static void formatBackupFileName(const char *fileBaseName, char *backupFileName, size_t length);
static bool isLogFileExist(const char *fileName);
static void shiftBackupFilesLeft(LogFile *files, uint8_t length);

static size_t formatTimestamp(char *buffer);
static size_t formatTagLevel(char *buffer, const char *tag, LogLevel severity, size_t timestampLength);
static size_t formatLogMessage(char *buffer, const char *format, va_list list, size_t prefixLength);
int strCompareICase(const char *one, const char *two);


LoggerEvent *subscribeFileLogger(LogLevel threshold, const char *fileName, uint32_t maxFileSize, uint8_t maxBackupFiles) {
    if (fileName == NULL) {
        snprintf(ERROR_EVENT.buffer, LOGGER_BUFFER_SIZE, "ERROR: Mandatory parameter [fileName] is NULL");
        return &ERROR_EVENT;
    }

    if (strstr(fileName, ".log") == NULL) {
        char *message = "ERROR: Invalid file: [%s] extension. Only files with [.log] extension allowed";
        snprintf(ERROR_EVENT.buffer, LOGGER_BUFFER_SIZE, message, fileName);
        return &ERROR_EVENT;
    }

    size_t fileNameLength = strlen(fileName) + 1;   // including line terminator
    if (fileNameLength > LOGGER_FILE_NAME_MAX_SIZE) {
        char *message = "ERROR: [fileName] exceeds the maximum number of characters. Allowed: [%d], actual: [%d]";
        snprintf(ERROR_EVENT.buffer, LOGGER_BUFFER_SIZE, message, LOGGER_FILE_NAME_MAX_SIZE, fileNameLength);
        return &ERROR_EVENT;
    }
    initThreadLock();
    lockThread();

    LoggerEvent fileEvent = {.buffer = messageBuffer, .level = threshold, .function = fileCallback};
    fileEvent.file = malloc(sizeof(struct LogFile));
    if (fileEvent.file == NULL) {
        snprintf(ERROR_EVENT.buffer, LOGGER_BUFFER_SIZE, "ERROR: Memory allocation fail while creating log file: [%s]", fileName);
        unlockThread();
        return &ERROR_EVENT;
    }

    fileEvent.file->id = 0;
    fileEvent.file->out = fopen(fileName, "ab+"); // open file for read/write or create if not exist
    if (fileEvent.file->out == NULL) {
        snprintf(ERROR_EVENT.buffer, LOGGER_BUFFER_SIZE, "ERROR: Failed to open/create file: [%s]", fileName);
        unlockThread();
        return &ERROR_EVENT;
    }

    fileEvent.file->size = getLogFileSize(fileName);
    fileEvent.file->name = calloc(fileNameLength, sizeof(char));
    fileEvent.backupFiles = calloc(maxBackupFiles, sizeof(struct LogFile));
    if (fileEvent.file->name == NULL || fileEvent.backupFiles == NULL) {
        snprintf(ERROR_EVENT.buffer, LOGGER_BUFFER_SIZE, "ERROR: Memory allocation fail: [%s]", fileName);
        loggerUnsubscribe(&fileEvent);
        unlockThread();
        return &ERROR_EVENT;
    }

    for (uint16_t i = 0; i < maxBackupFiles; i++) {
        fileEvent.backupFiles[i].id = i + 1;
        fileEvent.backupFiles[i].name = calloc(fileNameLength + FILE_TIMESTAMP_LENGTH, sizeof(char));
        if (fileEvent.backupFiles[i].name == NULL) {
            snprintf(ERROR_EVENT.buffer, LOGGER_BUFFER_SIZE, "ERROR: Backup memory allocation fail: [%s]", fileName);
            loggerUnsubscribe(&fileEvent);
            unlockThread();
            return &ERROR_EVENT;
        }
    }

    strncpy(fileEvent.file->name, fileName, fileNameLength);
    fileEvent.file->maxSize = maxFileSize > 0 ? maxFileSize : DEFAULT_FILE_SIZE;
    fileEvent.maxBackupFiles = maxBackupFiles;
    LoggerEvent *logEvent = loggerSubscribe(&fileEvent);
    memset(messageBuffer, 0, LOGGER_BUFFER_SIZE);
    unlockThread();
    return logEvent;
}

LoggerEvent *subscribeConsoleLogger(LogLevel threshold) {
    initThreadLock();
    lockThread();
    LoggerEvent consoleEvent = {
            .level = threshold,
            .buffer = messageBuffer,
            .function = consoleCallback
    };
    LoggerEvent *event = loggerSubscribe(&consoleEvent);
    unlockThread();
    return event;
}

LoggerEvent *subscribeCustomLogger(LogLevel threshold, LoggerCallback callback) {
    initThreadLock();
    lockThread();
    LoggerEvent customEvent = {
            .level = threshold,
            .buffer = messageBuffer,
            .callback = callback,
            .function = customCallback
    };
    LoggerEvent *event = loggerSubscribe(&customEvent);
    unlockThread();
    return event;
}

void loggerUnsubscribe(LoggerEvent *subscriber) {
    if (subscriber == NULL || subscriber == &ERROR_EVENT) return;

    if (subscriber->file != NULL) {
        if (subscriber->file->out != NULL) {
            fclose(subscriber->file->out);
        }

        if (subscriber->file->name != NULL) {
            free(subscriber->file->name);
            subscriber->file->name = NULL;
        }
    }

    for (uint8_t i = 0; i < subscriber->maxBackupFiles; i++) {
        if (subscriber->backupFiles[i].name != NULL) {
            free(subscriber->backupFiles[i].name);
            subscriber->backupFiles[i].name = NULL;
        }

        if (subscriber->backupFiles[i].out != NULL) {
            fclose(subscriber->backupFiles[i].out);
        }
    }

    if (subscriber->file != NULL) {
        free(subscriber->file);
        subscriber->file = NULL;
    }

    if (subscriber->backupFiles != NULL) {
        free(subscriber->backupFiles);
        subscriber->backupFiles = NULL;
    }

    subscriber->maxBackupFiles = 0;
    subscriber->isSubscribed = false;

    for (uint8_t i = 0; i < LOGGER_MAX_SUBSCRIBERS; i++) {  // Reorganize subscribers, active should be first
        LoggerEvent *event = &loggerSubscriberArray[i];
        if (!event->isSubscribed && (i + 1) < LOGGER_MAX_SUBSCRIBERS) {
            LoggerEvent *nextEvent = &loggerSubscriberArray[i + 1];
            if (nextEvent->isSubscribed) {
                loggerSubscriberArray[i] = *nextEvent;
                loggerSubscriberArray[i + 1] = (LoggerEvent){0};
            }
        }
    }
}

void loggerUnsubscribeAll() {
    initThreadLock();
    lockThread();
    for (uint8_t i = 0; i < LOGGER_MAX_SUBSCRIBERS; i++) {
        LoggerEvent *subscriber = &loggerSubscriberArray[i];
        if (!subscriber->isSubscribed) {
            break;
        }
        loggerUnsubscribe(subscriber);
    }
    unlockThread();
}

const char *logLevelToString(LogLevel severity) {
    return severity <= LOG_LEVEL_FATAL ? LEVEL_STRINGS[severity] : LEVEL_STRINGS[LOG_LEVEL_UNKNOWN];
}

LogLevel stringToLogLevel(const char *severity) {
    if (strCompareICase(severity, LEVEL_STRINGS[LOG_LEVEL_TRACE]) == 0) {
        return LOG_LEVEL_TRACE;

    } else if (strCompareICase(severity, LEVEL_STRINGS[LOG_LEVEL_DEBUG]) == 0) {
        return LOG_LEVEL_DEBUG;

    } else if (strCompareICase(severity, LEVEL_STRINGS[LOG_LEVEL_INFO]) == 0) {
        return LOG_LEVEL_INFO;

    } else if (strCompareICase(severity, LEVEL_STRINGS[LOG_LEVEL_WARN]) == 0) {
        return LOG_LEVEL_WARN;

    } else if (strCompareICase(severity, LEVEL_STRINGS[LOG_LEVEL_ERROR]) == 0) {
        return LOG_LEVEL_ERROR;

    } else if (strCompareICase(severity, LEVEL_STRINGS[LOG_LEVEL_FATAL]) == 0) {
        return LOG_LEVEL_FATAL;

    } else {
        return LOG_LEVEL_UNKNOWN;
    }
}

void logMessage(const char *tag, LogLevel severity, const char *format, ...) {
    lockThread();
    if (!isNeedToBeLogged(severity)) {
        unlockThread();
        return;
    }

    for (uint8_t i = 0; i < LOGGER_MAX_SUBSCRIBERS; i++) {
        LoggerEvent *subscriber = &loggerSubscriberArray[i];
        if (!subscriber->isSubscribed) {
            break;
        }

        if (severity >= subscriber->level) {
            va_start(subscriber->list, format);
            subscriber->tag = tag;
            subscriber->format = format;
            subscriber->function(subscriber, severity);
            va_end(subscriber->list);
        }
    }

    memset(messageBuffer, 0, strlen(messageBuffer));
    unlockThread();
}

static LoggerEvent *loggerSubscribe(LoggerEvent *event) {
    for (uint8_t i = 0; i < LOGGER_MAX_SUBSCRIBERS; i++) {
        LoggerEvent *subscriber = &loggerSubscriberArray[i];
        if (!subscriber->isSubscribed) {
            *subscriber = *event;
            subscriber->isSubscribed = true;
            return subscriber;
        }
    }
    return NULL;
}

static void consoleCallback(LoggerEvent *event, LogLevel severity) {
    size_t timestampLength = formatTimestamp(event->buffer);
#ifdef USE_LOGGER_COLOR
    size_t tagLevelLength = formatColoredTagLevel(event->buffer, event->tag, severity, timestampLength);
#else
    size_t tagLevelLength = formatTagLevel(event->buffer, event->tag, severity, timestampLength);
#endif

    size_t totalMessageLength = formatLogMessage(event->buffer, event->format, event->list, (timestampLength + tagLevelLength));
    printf("%s", event->buffer);
    memset(event->buffer, 0, totalMessageLength);
}

static void fileCallback(LoggerEvent *event, LogLevel severity) {
    if (rotateLogFiles(event)) {
        size_t timestampLength = formatTimestamp(event->buffer);
        size_t tagLevelLength = formatTagLevel(event->buffer, event->tag, severity, timestampLength);
        size_t totalMessageLength = formatLogMessage(event->buffer, event->format, event->list, (timestampLength + tagLevelLength));

        fwrite(event->buffer, sizeof(char), totalMessageLength, event->file->out);
    #if defined(_WIN32) || defined(_WIN64)
        fflush(event->file->out);
    #else
        fflush(event->file->out);
        fsync(fileno(event->file->out));
    #endif /* defined(_WIN32) || defined(_WIN64) */
        event->file->size += totalMessageLength;
        memset(event->buffer, 0, totalMessageLength);
    }
}

static void customCallback(LoggerEvent *event, LogLevel severity) {
    size_t timestampLength = formatTimestamp(event->buffer);
    size_t tagLevelLength = formatTagLevel(event->buffer, event->tag, severity, timestampLength);
    size_t totalMessageLength = formatLogMessage(event->buffer, event->format, event->list, (timestampLength + tagLevelLength));
    event->callback(severity, event->buffer, totalMessageLength);
    memset(event->buffer, 0, totalMessageLength);
}

static void initThreadLock() {
    if (isLockInitialized) return;
#if defined(_WIN32) || defined(_WIN64)
    InitializeCriticalSection(&threadMutex);
#else
    pthread_mutex_init(&threadMutex, NULL);
#endif
    isLockInitialized = true;
}

static void lockThread() {
#if defined(_WIN32) || defined(_WIN64)
    EnterCriticalSection(&threadMutex);
#else
    pthread_mutex_lock(&threadMutex);
#endif /* defined(_WIN32) || defined(_WIN64) */
}

static void unlockThread() {
#if defined(_WIN32) || defined(_WIN64)
    LeaveCriticalSection(&threadMutex);
#else
    pthread_mutex_unlock(&threadMutex);
#endif /* defined(_WIN32) || defined(_WIN64) */
}

static bool isNeedToBeLogged(LogLevel level) {
    for(uint32_t i = 0; i < LOGGER_MAX_SUBSCRIBERS; i++) {
        LoggerEvent *subscriber = &loggerSubscriberArray[i];
        if (!subscriber->isSubscribed) {
            return false;
        }

        if (level >= subscriber->level) {
            return true;
        }
    }
    return false;
}

static uint32_t getLogFileSize(const char *fileName) {
    FILE *logFile;
    if ((logFile = fopen(fileName, "rb")) == NULL) {
        return 0;
    }
    fseek(logFile, 0, SEEK_END);
    uint32_t fileSize = ftell(logFile);
    fclose(logFile);
    return fileSize;
}

static bool rotateLogFiles(LoggerEvent *event) {
    if (event->file->size <= event->file->maxSize) {
        return event->file->out != NULL;
    }

    if (event->maxBackupFiles == 0) {
        fprintf(stderr, "ERROR: Log file is full: [%s]\n", event->file->name);
        return false;
    }
    fclose(event->file->out);

    LogFile backupFile = event->backupFiles[0];  // first is the NULL or oldest backup file
    if (backupFile.out != NULL && remove(backupFile.name) != 0) {   // remove already existing file
        fprintf(stderr, "ERROR: Failed to remove backup log file: [%s]\n", backupFile.name);
    }

    size_t length = strlen(event->file->name) + FILE_TIMESTAMP_LENGTH;
    formatBackupFileName(event->file->name, backupFile.name, length);
    if (isLogFileExist(backupFile.name)) {
        sprintf(backupFile.name + strlen(backupFile.name), ".%d", backupFile.id);  // log file with same timestamp already exist, so add unique id
        if (isLogFileExist(backupFile.name) && remove(backupFile.name) != 0) {  // if it still exists, then remove it
            fprintf(stderr, "ERROR: Failed to remove backup log file: [%s]\n", backupFile.name);
        }
    }

    if (rename(event->file->name, backupFile.name) != 0) {
        fprintf(stderr, "ERROR: Failed to rename log file. From [%s] to [%s]\n", event->file->name, backupFile.name);
    }
    backupFile.size = event->file->size;
    backupFile.maxSize = event->file->maxSize;
    backupFile.out = fopen(backupFile.name, "r");
    if (backupFile.out == NULL) {
        fprintf(stderr, "ERROR: Failed to open backup log file: [%s]\n", backupFile.name);
    }
    fclose(backupFile.out);

    shiftBackupFilesLeft(event->backupFiles, event->maxBackupFiles);    // move backups
    event->backupFiles[event->maxBackupFiles - 1] = backupFile;  // put the newest backup file to the end of array
    event->file->out = fopen(event->file->name, "a"); // create empty file with original log file name
    if (event->file->out == NULL) {
        fprintf(stderr, "ERROR: Failed to open log file: [%s]\n", event->file->name);
        return false;
    }
    event->file->size = getLogFileSize(event->file->name);
    return true;
}

static void formatBackupFileName(const char *fileBaseName, char *backupFileName, size_t length) {   // Example: "dir1/dir2/fileName.log" -> "dir1/dir2/fileName_2023-05-02.log"
    char *nameEnd = strstr(fileBaseName, ".log");
    int pathLen = nameEnd - fileBaseName;
    strncpy(backupFileName, fileBaseName, pathLen);

    time_t logTime = time(NULL);
    struct tm *logLocalTime = localtime(&logTime);
    strftime(backupFileName + pathLen, length - pathLen, "_%Y-%m-%d.log", logLocalTime);
}

static bool isLogFileExist(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        return false;
    }

    fclose(file);
    return true;
}

static void shiftBackupFilesLeft(LogFile *files, uint8_t length) {
    for (uint8_t i = 0; i < length - 1; i++) {
        files[i] = files[i + 1];
    }
}

static size_t formatTimestamp(char *buffer) {
    time_t logTime;
    time(&logTime);
    struct tm *logLocalTime = localtime(&logTime);
    return strftime(buffer, LOGGER_BUFFER_SIZE, "%d %b %Y %H:%M:%S", logLocalTime);
}

#ifdef USE_LOGGER_COLOR
static size_t formatColoredTagLevel(char *buffer, const char *tag, LogLevel severity, size_t timestampLength) {
    return snprintf(buffer + timestampLength, LOGGER_BUFFER_SIZE - timestampLength, " | %s%-5s\x1b[0m | %s - ", LEVEL_COLORS[severity], logLevelToString(severity), tag);
}
#endif

static size_t formatTagLevel(char *buffer, const char *tag, LogLevel severity, size_t timestampLength) {
    return snprintf(buffer + timestampLength, LOGGER_BUFFER_SIZE - timestampLength, " | %s | %s - ", logLevelToString(severity), tag);
}

static size_t formatLogMessage(char *buffer, const char *format, va_list list, size_t prefixLength) {
    size_t messageLength = vsnprintf(buffer + prefixLength, LOGGER_BUFFER_SIZE - prefixLength - 1, format, list);
    size_t totalMessageLength = prefixLength + messageLength;
    buffer[totalMessageLength] = '\n';
    return totalMessageLength + 1;
}

int strCompareICase(const char *one, const char *two) {
    int charOfOne;
    int charOfTwo;
    do {
        charOfOne = (int) *one++;
        charOfTwo = (int) *two++;
        charOfOne = tolower((int) charOfOne);
        charOfTwo = tolower((int) charOfTwo);
    } while (charOfOne == charOfTwo && charOfOne != '\0');

    return charOfOne - charOfTwo;
}