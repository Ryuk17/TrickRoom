/*
 * @Author: Ryuk
 * @Date: 2026-08-09 19:02:18
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-11
 * @Description: Public API definitions — symbol visibility macro and status codes
 */
#ifndef AUDIO_ENGINE_DEF_H
#define AUDIO_ENGINE_DEF_H

/* ============================================================================
 * Symbol visibility: AUDIO_ENGINE_API
 *   - Static link:   empty (no attributes)
 *   - Build lib:     __declspec(dllexport) / visibility("default")
 *   - Consume DLL:   __declspec(dllimport)
 * ==========================================================================*/
#if defined(AUDIO_ENGINE_STATIC)
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
 * Unified status codes returned by all libAE_xxx API functions
 * ==========================================================================*/
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

#endif /* AUDIO_ENGINE_DEF_H */
