#pragma once

// Ariadnis module SDK
//
// This header is the complete binary contract between Ariadnis and a module.
// A module must not include editor-private headers or link against the editor.
// All strings are UTF-8 and all pointers supplied by the editor are only valid
// for the duration of the callback that receives them.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
    #ifdef __cplusplus
        #define ARIADNIS_MODULE_EXPORT extern "C" __declspec(dllexport)
    #else
        #define ARIADNIS_MODULE_EXPORT __declspec(dllexport)
    #endif
#else
    #ifdef __cplusplus
        #define ARIADNIS_MODULE_EXPORT extern "C" __attribute__((visibility("default")))
    #else
        #define ARIADNIS_MODULE_EXPORT __attribute__((visibility("default")))
    #endif
#endif

#define ARIADNIS_MODULE_ABI_VERSION 1u

typedef enum AriadnisModuleType {
    ARIADNIS_MODULE_EXPORTER = 0,
    ARIADNIS_MODULE_IMPORTER = 1,
    ARIADNIS_MODULE_BRUSH = 2,
    ARIADNIS_MODULE_RENDERER = 3,
    ARIADNIS_MODULE_PANEL = 4
} AriadnisModuleType;

typedef enum AriadnisLogLevel {
    ARIADNIS_LOG_DEBUG = 0,
    ARIADNIS_LOG_INFO = 1,
    ARIADNIS_LOG_WARNING = 2,
    ARIADNIS_LOG_ERROR = 3
} AriadnisLogLevel;

typedef enum AriadnisPolygonMode {
    ARIADNIS_POLYGON_FILL = 0,
    ARIADNIS_POLYGON_LINE = 1
} AriadnisPolygonMode;

typedef struct AriadnisTerrainView {
    int width;
    int depth;
    float cell_size;
    const float* vertices;
    size_t vertex_count;
    const int* indices;
    size_t index_count;
} AriadnisTerrainView;

typedef struct AriadnisMutableTerrainView {
    int width;
    int depth;
    float cell_size;
    float* vertices;
    size_t vertex_count;
    const int* indices;
    size_t index_count;
} AriadnisMutableTerrainView;

typedef struct AriadnisTerrainData {
    int width;
    int depth;
    float cell_size;
    const float* vertices;
    size_t vertex_count;
    const int* indices;
    size_t index_count;
} AriadnisTerrainData;

typedef struct AriadnisTerrainSink {
    void* user_data;
    bool (*replace_terrain)(void* user_data, const AriadnisTerrainData* terrain);
} AriadnisTerrainSink;

typedef struct AriadnisBrushInput {
    float hit_x;
    float hit_y;
    float hit_z;
    float delta_time;
    float radius;
    float strength;
    bool invert;
} AriadnisBrushInput;

typedef struct AriadnisRenderContext {
    void* user_data;
    void (*set_polygon_mode)(void* user_data, AriadnisPolygonMode mode);
    void (*set_color)(void* user_data, float red, float green, float blue, float alpha);
    void (*set_line_width)(void* user_data, float width);
    void (*draw_terrain)(void* user_data);
} AriadnisRenderContext;

typedef struct AriadnisHostApi {
    uint32_t abi_version;
    uint32_t struct_size;
    void* user_data;

    void (*log)(void* user_data, AriadnisLogLevel level, const char* message);

    // Immediate-mode UI helpers. They are valid only while render_panel runs.
    bool (*ui_begin_window)(void* user_data, const char* title, bool* visible);
    void (*ui_text)(void* user_data, const char* text);
    void (*ui_separator)(void* user_data);
    void (*ui_end_window)(void* user_data);
} AriadnisHostApi;

typedef struct AriadnisModuleApi {
    uint32_t abi_version;
    uint32_t struct_size;

    // id must be globally unique and use only ASCII letters, digits, '.', '_' or '-'.
    const char* id;
    const char* name;
    const char* version;
    const char* description;
    AriadnisModuleType type;

    // Load/unload run once per DLL load. Enable/disable run on every state transition.
    bool (*on_load)(const AriadnisHostApi* host);
    bool (*on_enable)(void);
    void (*on_disable)(void);
    void (*on_unload)(void);

    // Exporter and importer callbacks.
    const char* const* (*get_supported_extensions)(size_t* count);
    bool (*export_terrain)(const AriadnisTerrainView* terrain, const char* utf8_path);
    bool (*import_terrain)(const char* utf8_path, const AriadnisTerrainSink* sink);

    // Panel callback. Set *visible to false to close the panel for this session.
    void (*render_panel)(bool* visible);

    // Brush callbacks. apply_brush returns true when it changed vertex data.
    const char* (*get_brush_name)(void);
    float (*get_brush_radius)(void);
    float (*get_brush_strength)(void);
    bool (*apply_brush)(AriadnisMutableTerrainView* terrain, const AriadnisBrushInput* input);

    // Renderer callback. Use the supplied context instead of editor/OpenGL internals.
    const char* (*get_render_mode_name)(void);
    bool (*render_terrain)(const AriadnisTerrainView* terrain, const AriadnisRenderContext* context);
} AriadnisModuleApi;

typedef const AriadnisModuleApi* (*AriadnisGetModuleApiFn)(void);

// Every module DLL must export exactly this function.
ARIADNIS_MODULE_EXPORT const AriadnisModuleApi* Ariadnis_GetModuleApi(void);
