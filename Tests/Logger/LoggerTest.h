#pragma once

#include "BaseTestTemplate.h"
#include "Logger.h"


static void readFileContents(const char* name, char *buffer) {
    FILE *f = fopen(name, "rb");
    if (f == NULL) return;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    fsize = fsize > 1024 ? 1024 : fsize;

    fread(buffer, fsize, 1, f);
    fclose(f);
    buffer[fsize] = 0;
}

static bool checkLogEntry(const char *buffer, const char* lvl, const char* msg) {
    return strstr(buffer, lvl) != NULL && strstr(buffer, msg) != NULL;
}

static bool checkFileEntry(const char *buffer, const char* msg) {
    return strstr(buffer, msg) != NULL;
}

static void getBackupFileName(char buffer[64], const char *postfix) {
    time_t logTime = time(NULL);
    struct tm *logLocalTime = localtime(&logTime);
    strcpy(buffer, "test");
    strftime(buffer + 4, 64, "_%Y-%m-%d.log", logLocalTime);
    if (postfix != NULL) {
        strcat(buffer, ".");
        strcat(buffer, postfix);
    }
}


static MunitResult testConsoleLogger(const MunitParameter params[], void *testString) {
    subscribeConsoleLogger(LOG_LEVEL_TRACE);
    FILE * file = freopen("output.txt", "wab+", stdout);

    LOG_TRACE("TEST", "test some message: [%d]", 1);
    LOG_DEBUG("TEST", "test some message: [%d]", 2);
    LOG_INFO("TEST", "test some message: [%d]", 3);
    LOG_WARN("TEST", "test some message: [%d]", 4);
    LOG_ERROR("TEST", "test some message: [%d]", 5);
    LOG_FATAL("TEST", "test some message: [%d]", 6);
    fflush(file);

    char buffer[1024] = {0};
    readFileContents("output.txt", buffer);
    assert_true(checkLogEntry(buffer, "TRACE", "TEST - test some message: [1]\n"));
    assert_true(checkLogEntry(buffer, "DEBUG", "TEST - test some message: [2]\n"));
    assert_true(checkLogEntry(buffer, "INFO", "TEST - test some message: [3]\n"));
    assert_true(checkLogEntry(buffer, "WARN", "TEST - test some message: [4]\n"));
    assert_true(checkLogEntry(buffer, "ERROR", "TEST - test some message: [5]\n"));
    assert_true(checkLogEntry(buffer, "FATAL", "TEST - test some message: [6]\n"));
    loggerUnsubscribeAll();
    fclose(file);

    // level filtering test
    subscribeConsoleLogger(LOG_LEVEL_INFO);
    file = freopen("output.txt", "wab+", stdout);

    LOG_TRACE("TEST", "test some message: [%d]", 1);
    LOG_DEBUG("TEST", "test some message: [%d]", 2);
    LOG_INFO("TEST", "test some message: [%d]", 3);
    LOG_WARN("TEST", "test some message: [%d]", 4);
    LOG_ERROR("TEST", "test some message: [%d]", 5);
    LOG_FATAL("TEST", "test some message: [%d]", 6);
    fflush(file);

    memset(buffer, 0, 1024);
    readFileContents("output.txt", buffer);
    assert_false(checkLogEntry(buffer, "TRACE", "TEST - test some message: [1]\n"));
    assert_false(checkLogEntry(buffer, "DEBUG", "TEST - test some message: [2]\n"));
    assert_true(checkLogEntry(buffer, "INFO", "TEST - test some message: [3]\n"));
    assert_true(checkLogEntry(buffer, "WARN", "TEST - test some message: [4]\n"));
    assert_true(checkLogEntry(buffer, "ERROR", "TEST - test some message: [5]\n"));
    assert_true(checkLogEntry(buffer, "FATAL", "TEST - test some message: [6]\n"));
    loggerUnsubscribeAll();
    fclose(file);

    subscribeConsoleLogger(LOG_LEVEL_FATAL);
    file = freopen("output.txt", "wab+", stdout);

    LOG_TRACE("TEST", "test some message: [%d]", 1);
    LOG_DEBUG("TEST", "test some message: [%d]", 2);
    LOG_INFO("TEST", "test some message: [%d]", 3);
    LOG_WARN("TEST", "test some message: [%d]", 4);
    LOG_ERROR("TEST", "test some message: [%d]", 5);
    LOG_FATAL("TEST", "test some message: [%d]", 6);
    fflush(file);

    memset(buffer, 0, 1024);
    readFileContents("output.txt", buffer);
    assert_false(checkLogEntry(buffer, "TRACE", "TEST - test some message: [1]\n"));
    assert_false(checkLogEntry(buffer, "DEBUG", "TEST - test some message: [2]\n"));
    assert_false(checkLogEntry(buffer, "INFO", "TEST - test some message: [3]\n"));
    assert_false(checkLogEntry(buffer, "WARN", "TEST - test some message: [4]\n"));
    assert_false(checkLogEntry(buffer, "ERROR", "TEST - test some message: [5]\n"));
    assert_true(checkLogEntry(buffer, "FATAL", "TEST - test some message: [6]\n"));
    loggerUnsubscribeAll();
    fclose(file);
    remove("output.txt");

    // switch back stdout
    #if defined(_WIN32) || defined(_WIN64)
        freopen("CON", "w", stdout); /*Mingw C++; Windows*/
    #else
        freopen("/dev/tty", "w", stdout); /*for gcc, ubuntu*/
    #endif
    return MUNIT_OK;
}

static MunitResult testFileLogger(const MunitParameter params[], void *testString) {
    // Error params check
    LoggerEvent *errorEvent = subscribeFileLogger(LOG_LEVEL_TRACE, NULL, 1024, 0);
    assert_false(errorEvent->isSubscribed);
    assert_string_equal(errorEvent->buffer, "ERROR: Mandatory parameter [fileName] is NULL");

    errorEvent = subscribeFileLogger(LOG_LEVEL_TRACE, "test.txt", 1024, 0);
    assert_false(errorEvent->isSubscribed);
    assert_string_equal(errorEvent->buffer, "ERROR: Invalid file: [test.txt] extension. Only files with [.log] extension allowed");

    // All level check
    LoggerEvent *event = subscribeFileLogger(LOG_LEVEL_TRACE, "test.log", 1024, 0);
    assert_true(event->isSubscribed);

    LOG_INFO("TEST", "test some message: [%d]", 1);
    LOG_DEBUG("TEST", "test some message: [%d]", 2);
    LOG_DEBUG("TEST", "test some message: [%d]", 3);
    LOG_INFO("TEST", "test some message: [%d]", 4);
    LOG_WARN("TEST", "test some message: [%d]", 5);
    LOG_ERROR("TEST", "test some message: [%d]", 6);
    LOG_FATAL("TEST", "test some message: [%d]", 7);

    LOG_INFO("TEST", "test some message: [%d]", 8);
    LOG_DEBUG("TEST", "test some message: [%d]", 9);
    LOG_DEBUG("TEST", "test some message: [%d]", 10);
    LOG_INFO("TEST", "test some message: [%d]", 11);
    LOG_WARN("TEST", "test some message: [%d]", 12);
    LOG_ERROR("TEST", "test some message: [%d]", 13);
    LOG_FATAL("TEST", "test some message: [%d]", 14);

    char buffer[1024] = {0};
    readFileContents("test.log", buffer);
    assert_true(checkFileEntry(buffer, " | INFO | TEST - test some message: [1]\n"));
    assert_true(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [2]\n"));
    assert_true(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [3]\n"));
    assert_true(checkFileEntry(buffer, " | INFO | TEST - test some message: [4]\n"));
    assert_true(checkFileEntry(buffer, " | WARN | TEST - test some message: [5]\n"));
    assert_true(checkFileEntry(buffer, " | ERROR | TEST - test some message: [6]\n"));
    assert_true(checkFileEntry(buffer, " | FATAL | TEST - test some message: [7]\n"));
    assert_true(checkFileEntry(buffer, " | INFO | TEST - test some message: [8]\n"));
    assert_true(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [9]\n"));
    assert_true(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [10]\n"));
    assert_true(checkFileEntry(buffer, " | INFO | TEST - test some message: [11]\n"));
    assert_true(checkFileEntry(buffer, " | WARN | TEST - test some message: [12]\n"));
    assert_true(checkFileEntry(buffer, " | ERROR | TEST - test some message: [13]\n"));
    assert_true(checkFileEntry(buffer, " | FATAL | TEST - test some message: [14]\n"));
    loggerUnsubscribeAll();
    remove("test.log");

    // Test level filtering
    event = subscribeFileLogger(LOG_LEVEL_WARN, "test.log", 1024, 0);
    assert_true(event->isSubscribed);

    LOG_INFO("TEST", "test some message: [%d]", 1);
    LOG_DEBUG("TEST", "test some message: [%d]", 2);
    LOG_DEBUG("TEST", "test some message: [%d]", 3);
    LOG_INFO("TEST", "test some message: [%d]", 4);
    LOG_WARN("TEST", "test some message: [%d]", 5);
    LOG_ERROR("TEST", "test some message: [%d]", 6);
    LOG_FATAL("TEST", "test some message: [%d]", 7);

    LOG_INFO("TEST", "test some message: [%d]", 8);
    LOG_DEBUG("TEST", "test some message: [%d]", 9);
    LOG_DEBUG("TEST", "test some message: [%d]", 10);
    LOG_INFO("TEST", "test some message: [%d]", 11);
    LOG_WARN("TEST", "test some message: [%d]", 12);
    LOG_ERROR("TEST", "test some message: [%d]", 13);
    LOG_FATAL("TEST", "test some message: [%d]", 14);

    memset(buffer, 0, 1024);
    readFileContents("test.log", buffer);
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [1]\n"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [2]\n"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [3]\n"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [4]\n"));
    assert_true(checkFileEntry(buffer, " | WARN | TEST - test some message: [5]\n"));
    assert_true(checkFileEntry(buffer, " | ERROR | TEST - test some message: [6]\n"));
    assert_true(checkFileEntry(buffer, " | FATAL | TEST - test some message: [7]\n"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [8]\n"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [9]\n"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [10]\n"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [11]\n"));
    assert_true(checkFileEntry(buffer, " | WARN | TEST - test some message: [12]\n"));
    assert_true(checkFileEntry(buffer, " | ERROR | TEST - test some message: [13]\n"));
    assert_true(checkFileEntry(buffer, " | FATAL | TEST - test some message: [14]\n"));
    loggerUnsubscribeAll();
    remove("test.log");

    // Rolling files
    event = subscribeFileLogger(LOG_LEVEL_TRACE, "test.log", 128, 3);
    assert_true(event->isSubscribed);

    LOG_INFO("TEST", "test some message: [%d]", 1);
    LOG_DEBUG("TEST", "test some message: [%d]", 2);
    LOG_DEBUG("TEST", "test some message: [%d]", 3);
    LOG_INFO("TEST", "test some message: [%d]", 4);
    LOG_WARN("TEST", "test some message: [%d]", 5);
    LOG_ERROR("TEST", "test some message: [%d]", 6);
    LOG_FATAL("TEST", "test some message: [%d]", 7);

    LOG_INFO("TEST", "test some message: [%d]", 8);
    LOG_DEBUG("TEST", "test some message: [%d]", 9);
    LOG_DEBUG("TEST", "test some message: [%d]", 10);
    LOG_INFO("TEST", "test some message: [%d]", 11);
    LOG_WARN("TEST", "test some message: [%d]", 12);
    LOG_ERROR("TEST", "test some message: [%d]", 13);
    LOG_FATAL("TEST", "test some message: [%d]", 14);

    // check test.log
    memset(buffer, 0, 1024);
    readFileContents("test.log", buffer);
    assert_true(checkFileEntry(buffer, " | ERROR | TEST - test some message: [13]"));
    assert_true(checkFileEntry(buffer, " | FATAL | TEST - test some message: [14]"));

    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [1]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [2]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [3]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [4]"));
    assert_false(checkFileEntry(buffer, " | WARN | TEST - test some message: [5]"));
    assert_false(checkFileEntry(buffer, " | ERROR | TEST - test some message: [6]"));
    assert_false(checkFileEntry(buffer, " | FATAL | TEST - test some message: [7]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [8]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [9]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [10]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [11]"));
    assert_false(checkFileEntry(buffer, " | WARN | TEST - test some message: [12]"));
    remove("test.log");


    // check test-<timestamp>.log
    char nameBuffer[64] = {0};
    getBackupFileName(nameBuffer, NULL);
    memset(buffer, 0, 1024);
    readFileContents(nameBuffer, buffer);
    assert_true(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [10]"));
    assert_true(checkFileEntry(buffer, " | INFO | TEST - test some message: [11]"));
    assert_true(checkFileEntry(buffer, " | WARN | TEST - test some message: [12]"));

    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [1]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [2]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [3]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [4]"));
    assert_false(checkFileEntry(buffer, " | WARN | TEST - test some message: [5]"));
    assert_false(checkFileEntry(buffer, " | ERROR | TEST - test some message: [6]"));
    assert_false(checkFileEntry(buffer, " | FATAL | TEST - test some message: [7]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [8]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [9]"));
    assert_false(checkFileEntry(buffer, " | ERROR | TEST - test some message: [13]"));
    assert_false(checkFileEntry(buffer, " | FATAL | TEST - test some message: [14]"));
    remove(nameBuffer);

    // check test-<timestamp>.log.2
    memset(nameBuffer, 0, 64);
    getBackupFileName(nameBuffer, "2");
    memset(buffer, 0, 1024);
    readFileContents(nameBuffer, buffer);
    assert_true(checkFileEntry(buffer, " | INFO | TEST - test some message: [4]"));
    assert_true(checkFileEntry(buffer, " | WARN | TEST - test some message: [5]"));
    assert_true(checkFileEntry(buffer, " | ERROR | TEST - test some message: [6]"));

    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [1]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [2]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [3]"));
    assert_false(checkFileEntry(buffer, " | FATAL | TEST - test some message: [7]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [8]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [9]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [10]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [11]"));
    assert_false(checkFileEntry(buffer, " | WARN | TEST - test some message: [12]"));
    assert_false(checkFileEntry(buffer, " | ERROR | TEST - test some message: [13]"));
    assert_false(checkFileEntry(buffer, " | FATAL | TEST - test some message: [14]"));
    remove(nameBuffer);

    // check test-<timestamp>.log.3
    memset(nameBuffer, 0, 64);
    getBackupFileName(nameBuffer, "3");
    memset(buffer, 0, 1024);
    readFileContents(nameBuffer, buffer);
    assert_true(checkFileEntry(buffer, " | FATAL | TEST - test some message: [7]"));
    assert_true(checkFileEntry(buffer, " | INFO | TEST - test some message: [8]"));
    assert_true(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [9]"));

    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [1]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [2]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [3]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [4]"));
    assert_false(checkFileEntry(buffer, " | WARN | TEST - test some message: [5]"));
    assert_false(checkFileEntry(buffer, " | ERROR | TEST - test some message: [6]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [10]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [11]"));
    assert_false(checkFileEntry(buffer, " | WARN | TEST - test some message: [12]"));
    assert_false(checkFileEntry(buffer, " | ERROR | TEST - test some message: [13]"));
    assert_false(checkFileEntry(buffer, " | FATAL | TEST - test some message: [14]"));
    remove(nameBuffer);

    // check that no other files
    memset(nameBuffer, 0, 64);
    getBackupFileName(nameBuffer, "1");
    memset(buffer, 0, 1024);
    readFileContents(nameBuffer, buffer);
    assert_true(strlen(buffer) == 0);
    remove("test.log");
//    loggerUnsubscribeAll();

    return MUNIT_OK;
}

static MunitResult testLogToFileOverflow(const MunitParameter params[], void *testString) {
    // check for overflow
   LoggerEvent *event = subscribeFileLogger(LOG_LEVEL_TRACE, "test_2.log", 128, 0);
    assert_true(event->isSubscribed);

    LOG_INFO("TEST", "test some message: [%d]", 1);
    LOG_DEBUG("TEST", "test some message: [%d]", 2);
    LOG_DEBUG("TEST", "test some message: [%d]", 3);
    LOG_INFO("TEST", "test some message: [%d]", 4);
    LOG_WARN("TEST", "test some message: [%d]", 5);
    LOG_ERROR("TEST", "test some message: [%d]", 6);
    LOG_FATAL("TEST", "test some message: [%d]", 7);

    LOG_INFO("TEST", "test some message: [%d]", 8);
    LOG_DEBUG("TEST", "test some message: [%d]", 9);
    LOG_DEBUG("TEST", "test some message: [%d]", 10);
    LOG_INFO("TEST", "test some message: [%d]", 11);
    LOG_WARN("TEST", "test some message: [%d]", 12);
    LOG_ERROR("TEST", "test some message: [%d]", 13);
    LOG_FATAL("TEST", "test some message: [%d]", 14);

    char buffer[1024] = {0};
    readFileContents("test_2.log", buffer);
    assert_true(checkFileEntry(buffer, " | INFO | TEST - test some message: [1]"));
    assert_true(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [2]"));
    assert_true(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [3]"));

    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [4]"));
    assert_false(checkFileEntry(buffer, " | WARN | TEST - test some message: [5]"));
    assert_false(checkFileEntry(buffer, " | ERROR | TEST - test some message: [6]"));
    assert_false(checkFileEntry(buffer, " | FATAL | TEST - test some message: [7]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [8]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [9]"));
    assert_false(checkFileEntry(buffer, " | DEBUG | TEST - test some message: [10]"));
    assert_false(checkFileEntry(buffer, " | INFO | TEST - test some message: [11]"));
    assert_false(checkFileEntry(buffer, " | WARN | TEST - test some message: [12]"));
    assert_false(checkFileEntry(buffer, " | ERROR | TEST - test some message: [13]"));
    assert_false(checkFileEntry(buffer, " | FATAL | TEST - test some message: [14]"));
    loggerUnsubscribeAll();
    remove("test_2.log");

    return MUNIT_OK;
}

static void customLoggerCallbackFun(LogLevel severity, const char *message, uint32_t length) {
    assert_int(LOG_LEVEL_INFO, ==, severity);
    assert_true(checkFileEntry(message, " | INFO | TEST - test some message: [1]\n"));
    assert_true(length == strlen(message));
}

static MunitResult testLogCustomCallback(const MunitParameter params[], void *testString) {
    LoggerEvent *event = subscribeCustomLogger(LOG_LEVEL_INFO, customLoggerCallbackFun);
    assert_true(event->isSubscribed);
    LOG_DEBUG("TEST", "test some message: [%d]", 2);
    LOG_INFO("TEST", "test some message: [%d]", 1);
    loggerUnsubscribeAll();

    return MUNIT_OK;
}

static MunitResult testLogLevelToString(const MunitParameter params[], void *testString) {
    munit_assert_string_equal("TRACE", logLevelToString(LOG_LEVEL_TRACE));
    munit_assert_string_equal("DEBUG", logLevelToString(LOG_LEVEL_DEBUG));
    munit_assert_string_equal("INFO", logLevelToString(LOG_LEVEL_INFO));
    munit_assert_string_equal("WARN", logLevelToString(LOG_LEVEL_WARN));
    munit_assert_string_equal("ERROR", logLevelToString(LOG_LEVEL_ERROR));
    munit_assert_string_equal("FATAL", logLevelToString(LOG_LEVEL_FATAL));
    munit_assert_string_equal("UNKNOWN", logLevelToString(56));
    munit_assert_string_equal("UNKNOWN", logLevelToString(-23));

    return MUNIT_OK;
}

static MunitResult testStringToLogLevel(const MunitParameter params[], void *testString) {
    assert_int(LOG_LEVEL_TRACE, ==, stringToLogLevel("TRACE"));
    assert_int(LOG_LEVEL_DEBUG, ==, stringToLogLevel("debug"));
    assert_int(LOG_LEVEL_INFO, ==, stringToLogLevel("iNfO"));
    assert_int(LOG_LEVEL_WARN, ==, stringToLogLevel("WARN"));
    assert_int(LOG_LEVEL_ERROR, ==, stringToLogLevel("error"));
    assert_int(LOG_LEVEL_FATAL, ==, stringToLogLevel("FATAL"));
    assert_int(LOG_LEVEL_UNKNOWN, ==, stringToLogLevel("bar"));
    assert_int(LOG_LEVEL_UNKNOWN, ==, stringToLogLevel("FOO"));

    return MUNIT_OK;
}

static MunitResult testOverflowLogger(const MunitParameter params[], void *testString) {
    subscribeConsoleLogger(LOG_LEVEL_TRACE);
    FILE * file = freopen("output_ovf.txt", "wab+", stdout);

    char message[LOGGER_BUFFER_SIZE + 10] = {[0 ... LOGGER_BUFFER_SIZE + 10 - 1] = 'A'};
    LOG_TRACE("TEST", "very long message: [%s]", message);
    fflush(file);

    char buffer[2048] = {0};
    readFileContents("output_ovf.txt", buffer);
    assert_true(checkLogEntry(buffer, "TRACE", "TEST - very long message: [AAAAA"));
    assert_size(strlen(buffer), ==, LOGGER_BUFFER_SIZE - 1);

    loggerUnsubscribeAll();
    fclose(file);

    // switch back stdout
    #if defined(_WIN32) || defined(_WIN64)
    freopen("CON", "w", stdout); /*Mingw C++; Windows*/
    #else
    freopen("/dev/tty", "w", stdout); /*for gcc, ubuntu*/
    #endif
    return MUNIT_OK;
}

static MunitTest loggerTests[] = {
        {.name =  "Test console logger - should correctly log messages to console", .test = testConsoleLogger},
        {.name =  "Test file logger - should correctly log messages to file and rotate them", .test = testFileLogger},
        {.name =  "Test file logger - should correctly log messages when no space left", .test = testLogToFileOverflow},
        {.name =  "Test custom logger - should correctly format messages for custom logger", .test = testLogCustomCallback},
        {.name =  "Test logLevelToString() - should correctly convert level to string", .test = testLogLevelToString},
        {.name =  "Test stringToLogLevel() - should correctly convert string to level", .test = testStringToLogLevel},
        {.name =  "Test overflow - should correctly work with string overflow", .test = testOverflowLogger},
        END_OF_TESTS
};

static const MunitSuite loggerTestSuite = {
        .prefix = "Logger: ",
        .tests = loggerTests,
        .suites = NULL,
        .iterations = 1,
        .options = MUNIT_SUITE_OPTION_NONE
};
