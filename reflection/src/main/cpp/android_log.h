#pragma once
#include <jni.h>
#include <android/log.h>

#define LOG_TAG "reflection_log"

#define ALOGV(FORMAT, ...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, FORMAT,##__VA_ARGS__)
#define ALOGD(FORMAT, ...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, FORMAT,##__VA_ARGS__)
#define ALOGI(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, FORMAT,##__VA_ARGS__)
#define ALOGW(FORMAT, ...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, FORMAT,##__VA_ARGS__)
#define ALOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, FORMAT,##__VA_ARGS__)