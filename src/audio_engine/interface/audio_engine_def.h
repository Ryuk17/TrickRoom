/*
 * @Author: Ryuk
 * @Date: 2026-08-09 19:02:18
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 20:51:45
 * @Description: First create
 */



#ifndef AUDIO_ENGINE_DEF_H
#define AUDIO_ENGINE_DEF_H

#include <stdio.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/time.h>
    #include <time.h>
#endif

#if defined(AUDIO_ENGINE_STATIC)
    /* Static linking: no import/export attributes needed */
    #define AUDIO_ENGINE_API
#elif defined(_WIN32)
    #if defined(AUDIO_ENGINE_EXPORTS)
        #define AUDIO_ENGINE_API __declspec(dllexport)
    #else
        #define AUDIO_ENGINE_API __declspec(dllimport)
    #endif
#else
    #define AUDIO_ENGINE_API __attribute__((visibility("default")))
#endif


/* ============================================================================
 * 1. 日志级别定义
 * ==========================================================================*/
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_NONE  4

#ifndef LOG_LEVEL
    #define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

/* ============================================================================
 * 2. ANSI 颜色控制
 * ==========================================================================*/
#define LOG_COLOR_RESET "\033[0m"
#define LOG_COLOR_DEBUG "\033[36m"  // 青色
#define LOG_COLOR_INFO  "\033[32m"  // 绿色
#define LOG_COLOR_WARN  "\033[33m"  // 黄色
#define LOG_COLOR_ERROR "\033[31m"  // 红色

/* ============================================================================
 * 3. 跨平台毫秒级时间戳获取函数 (静态内联，避免重复定义)
 * 格式化输出为: YYYY-MM-DD HH:MM:SS.ms (例如 2026-08-09 14:30:05.123)
 * ==========================================================================*/
static inline void log_get_timestamp(char* buffer, size_t size) {
#if defined(_WIN32)
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // 转换为本地时间结构体
    struct tm* tm_info = localtime(&tv.tv_sec);
    
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // 拼接毫秒部分
    snprintf(buffer, size, "%s.%03d", time_str, (int)(tv.tv_usec / 1000));
#endif
}

/* ============================================================================
 * 4. 核心日志宏 (增加时间戳输出)
 * ==========================================================================*/
#define LOG_BASE(level_num, level_str, color, fmt, ...) \
    do { \
        if (level_num >= LOG_LEVEL) { \
            char _time_buf[32]; \
            log_get_timestamp(_time_buf, sizeof(_time_buf)); \
            fprintf(stderr, "%s[%s] [%-5s] [%s:%d | %s] " fmt "%s\n", \
                    color, _time_buf, level_str, __FILE__, __LINE__, __func__, \
                    ##__VA_ARGS__, LOG_COLOR_RESET); \
        } \
    } while(0)

/* ============================================================================
 * 5. 用户 API 接口
 * ==========================================================================*/
#define LOG_DEBUG(fmt, ...) LOG_BASE(LOG_LEVEL_DEBUG, "DEBUG", LOG_COLOR_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG_BASE(LOG_LEVEL_INFO,  "INFO",  LOG_COLOR_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG_BASE(LOG_LEVEL_WARN,  "WARN",  LOG_COLOR_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_BASE(LOG_LEVEL_ERROR, "ERROR", LOG_COLOR_ERROR, fmt, ##__VA_ARGS__)


typedef enum {
    AUDIO_ENGINE_SUCCESS = 0,
    AUDIO_ENGINE_ERR_INIT_FAILED = 1,
    AUDIO_ENGINE_ERR_SET_PARAM_FAILED = 2,
    AUDIO_ENGINE_ERR_INVALID_HANDLE = 3,
    AUDIO_ENGINE_ERR_NULL_POINTER = 4,
    AUDIO_ENGINE_ERR_NOT_INITIALIZED = 5,
    AUDIO_ENGINE_ERR_PROCESS_FAILED = 6,
    AUDIO_ENGINE_ERR_INVALID_PARAM = 7,
} AudioEngineStatus;

#endif