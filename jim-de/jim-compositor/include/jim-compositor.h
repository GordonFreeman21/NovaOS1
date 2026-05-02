/*
 * JimCompositor Header File
 * 
 * Public API and type definitions for the JimDe compositor
 * 
 * Copyright (C) 2024 JimOS Project
 * Licensed under GPL-3.0-or-later
 */

#ifndef NOVA_COMPOSITOR_H
#define NOVA_COMPOSITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <wayland-server-core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * Rectangle structure
 */
typedef struct {
    int32_t x, y;
    int32_t width, height;
} jim_rect_t;

/**
 * Color with alpha
 */
typedef struct {
    float r, g, b, a;
} jim_color_t;

/**
 * Forward declarations
 */
struct jim_server;
struct jim_output;
struct jim_seat;
struct jim_view;
struct jim_wm;

/* ============================================================================
 * Core Server API
 * ============================================================================ */

/**
 * Create and initialize the compositor server
 */
struct jim_server *jim_server_create(void);

/**
 * Destroy the compositor server
 */
void jim_server_destroy(struct jim_server *server);

/**
 * Run the compositor event loop
 */
int jim_server_run(struct jim_server *server);

/**
 * Request compositor to quit
 */
void jim_server_quit(struct jim_server *server);

/* ============================================================================
 * View Management API
 * ============================================================================ */

/**
 * Focus a specific view
 */
void jim_server_focus_view(struct jim_server *server, struct jim_view *view);

/**
 * Find view at screen coordinates
 */
struct jim_view *jim_server_view_at(struct jim_server *server,
                                       double lx, double ly,
                                       struct wlr_surface **surface,
                                       double *sx, double *sy);

/**
 * Close a view
 */
void jim_server_close_view(struct jim_server *server, struct jim_view *view);

/**
 * Move view to coordinates
 */
void jim_server_move_view(struct jim_server *server, 
                           struct jim_view *view,
                           double lx, double ly);

/**
 * Resize view
 */
void jim_server_resize_view(struct jim_server *server,
                             struct jim_view *view,
                             int32_t width, int32_t height);

/* ============================================================================
 * Output Management API
 * ============================================================================ */

/**
 * Get primary output
 */
struct jim_output *jim_server_get_primary_output(struct jim_server *server);

/**
 * Get output count
 */
int jim_server_get_output_count(struct jim_server *server);

/* ============================================================================
 * Input Management API
 * ============================================================================ */

/**
 * Get current seat
 */
struct jim_seat *jim_server_get_current_seat(struct jim_server *server);

/**
 * Warp cursor to coordinates
 */
void jim_server_warp_cursor(struct jim_server *server,
                             double lx, double ly);

/* ============================================================================
 * Utility Functions (Assembly Optimized)
 * ============================================================================ */

/**
 * Fast memory copy (SIMD optimized)
 */
void jim_memcpy_aligned(void *dest, const void *src, size_t n);

/**
 * Fast memory set (AVX2 optimized)
 */
void jim_memset_aligned(void *ptr, int value, size_t n);

/**
 * Fast pixel blending (SSE4 optimized)
 */
void jim_blend_pixels(uint32_t *dest, uint32_t src, float alpha);

/**
 * Fast matrix multiplication (SSE optimized)
 */
void jim_matrix_multiply(float *result, const float *a, const float *b);

/**
 * Fast integer square root
 */
uint32_t jim_isqrt(uint32_t n);

/**
 * Get CPU feature flags
 */
uint32_t jim_get_cpu_features(void);

/**
 * Get current time in milliseconds
 */
uint64_t jim_get_time_ms(void);

/* ============================================================================
 * Window Manager API
 * ============================================================================ */

/**
 * Create window manager instance
 */
struct jim_wm *jim_wm_create(struct jim_server *server);

/**
 * Destroy window manager
 */
void jim_wm_destroy(struct jim_wm *wm);

/**
 * Update window animations
 */
void jim_wm_update_animations(struct jim_wm *wm);

/**
 * Switch to workspace
 */
void jim_wm_switch_workspace(struct jim_wm *wm, int workspace_id);

/**
 * Snap view to layout
 */
void jim_wm_snap_view(struct jim_wm *wm, struct jim_view *view, int snap_type);

/**
 * Check snap zone at coordinates
 */
void *jim_wm_check_snap_zone(struct jim_wm *wm, double x, double y);

/* ============================================================================
 * Constants
 * ============================================================================ */

/* CPU Feature Flags */
#define NOVA_CPU_SSE      (1 << 0)
#define NOVA_CPU_SSE2     (1 << 1)
#define NOVA_CPU_SSE3     (1 << 2)
#define NOVA_CPU_SSSE3    (1 << 3)
#define NOVA_CPU_SSE41    (1 << 4)
#define NOVA_CPU_SSE42    (1 << 5)
#define NOVA_CPU_AVX      (1 << 6)
#define NOVA_CPU_AVX2     (1 << 7)

/* Snap Types */
#define NOVA_SNAP_NONE              0
#define NOVA_SNAP_LEFT_HALF         1
#define NOVA_SNAP_RIGHT_HALF        2
#define NOVA_SNAP_TOP_HALF          3
#define NOVA_SNAP_BOTTOM_HALF       4
#define NOVA_SNAP_TOP_LEFT          5
#define NOVA_SNAP_TOP_RIGHT         6
#define NOVA_SNAP_BOTTOM_LEFT       7
#define NOVA_SNAP_BOTTOM_RIGHT      8
#define NOVA_SNAP_LEFT_THIRD        9
#define NOVA_SNAP_CENTER_THIRD      10
#define NOVA_SNAP_RIGHT_THIRD       11
#define NOVA_SNAP_MAXIMIZE          12

/* Modifier Keys */
#define NOVA_MODIFIER_SHIFT    (1 << 0)
#define NOVA_MODIFIER_CTRL     (1 << 1)
#define NOVA_MODIFIER_ALT      (1 << 2)
#define NOVA_MODIFIER_LOGO     (1 << 3)

/* Version Information */
#define NOVA_COMPOSITOR_VERSION_MAJOR 1
#define NOVA_COMPOSITOR_VERSION_MINOR 0
#define NOVA_COMPOSITOR_VERSION_PATCH 0
#define NOVA_COMPOSITOR_VERSION_STRING "1.0.0"

#ifdef __cplusplus
}
#endif

#endif /* NOVA_COMPOSITOR_H */
