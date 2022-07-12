#pragma once

#include "BaseTestTemplate.h"
#include "Logger.h"

static int logFunOneCounter = 0;
static int logFunTwoCounter = 0;

void logFunOne(LogLevel level, const char *msg) {
    logFunOneCounter++;
}

void logFunTwo(LogLevel level, const char *msg) {
    logFunTwoCounter++;
}

static MunitResult testLogger(const MunitParameter params[], void *testString) {
    loggerSubscribe(logFunOne, LOG_DEBUG_LEVEL);
    loggerSubscribe(logFunTwo, LOG_INFO_LEVEL);

    LOG_INFO("Test");
    assert_int(logFunOneCounter, ==, 1);
    assert_int(logFunTwoCounter, ==, 1);

    logFunOneCounter = 0;
    logFunTwoCounter = 0;
    LOG_DEBUG("Test");
    assert_int(logFunOneCounter, ==, 1);
    assert_int(logFunTwoCounter, ==, 0);

    logFunOneCounter = 0;
    logFunTwoCounter = 0;
    LOG_WARN("Test");
    LOG_ERROR("Test");
    assert_int(logFunOneCounter, ==, 2);
    assert_int(logFunTwoCounter, ==, 2);

    logFunOneCounter = 0;
    logFunTwoCounter = 0;
    loggerUnsubscribe(logFunOne);
    LOG_INFO("Test");
    LOG_WARN("Test");
    LOG_ERROR("Test");
    assert_int(logFunOneCounter, ==, 0);
    assert_int(logFunTwoCounter, ==, 3);

    return MUNIT_OK;
}

static MunitResult testLogLevelToString(const MunitParameter params[], void *testString) {
    munit_assert_string_equal("DEBUG", logLevelToString(LOG_DEBUG_LEVEL));
    munit_assert_string_equal("INFO", logLevelToString(LOG_INFO_LEVEL));
    munit_assert_string_equal("WARNING", logLevelToString(LOG_WARN_LEVEL));
    munit_assert_string_equal("ERROR", logLevelToString(LOG_ERROR_LEVEL));
    munit_assert_string_equal("UNKNOWN", logLevelToString(56));

    return MUNIT_OK;
}

static MunitTest loggerTests[] = {
        {.name =  "Test logger - should correctly log messages", .test = testLogger},
        {.name =  "Test logLevelToString() - should correctly convert level to string", .test = testLogLevelToString},
        END_OF_TESTS
};

static const MunitSuite loggerTestSuite = {
        .prefix = "Logger: ",
        .tests = loggerTests,
        .suites = NULL,
        .iterations = 1,
        .options = MUNIT_SUITE_OPTION_NONE
};