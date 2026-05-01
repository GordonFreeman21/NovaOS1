/*
 * NovaCompositor Header File
 * 
 * Public API and type definitions for the NovaDe compositor
 * 
 * Copyright (C) 2024 NovaOS Project
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
} nova_rect_t;

/**
 * Color with alpha
 */
typedef struct {
    float r, g, b, a;
} nova_color_t;

/**
 * Forward declarations
 */
struct nova_server;
struct nova_output;
struct nova_seat;
struct nova_view;
struct nova_wm;

/* ============================================================================
 * Core Server API
 * ============================================================================ */

/**
 * Create and initialize the compositor server
 */
struct nova_server *nova_server_create(void);

/**
 * Destroy the compositor server
 */
void nova_server_destroy(struct nova_server *server);

/**
 * Run the compositor event loop
 */
int nova_server_run(struct nova_server *server);

/**
 * Request compositor to quit
 */
void nova_server_quit(struct nova_server *server);

/* ============================================================================
 * View Management API
 * ============================================================================ */

/**
 * Focus a specific view
 */
void nova_server_focus_view(struct nova_server *server, struct nova_view *view);

/**
 * Find view at screen coordinates
 */
struct nova_view *nova_server_view_at(struct nova_server *server,
                                       double lx, double ly,
                                       struct wlr_surface **surface,
                                       double *sx, double *sy);

/**
 * Close a view
 */
void nova_server_close_view(struct nova_server *server, struct nova_view *view);

/**
 * Move view to coordinates
 */
void nova_server_move_view(struct nova_server *server, 
                           struct nova_view *view,
                           double lx, double ly);

/**
 * Resize view
 */
void nova_server_resize_view(struct nova_server *server,
                             struct nova_view *view,
                             int32_t width, int32_t height);

/* ============================================================================
 * Output Management API
 * ============================================================================ */

/**
 * Get primary output
 */
struct nova_output *nova_server_get_primary_output(struct nova_server *server);

/**
 * Get output count
 */
int nova_server_get_output_count(struct nova_server *server);

/* ============================================================================
 * Input Management API
 * ============================================================================ */

/**
 * Get current seat
 */
struct nova_seat *nova_server_get_current_seat(struct nova_server *server);

/**
 * Warp cursor to coordinates
 */
void nova_server_warp_cursor(struct nova_server *server,
                             double lx, double ly);

/* ============================================================================
 * Utility Functions (Assembly Optimized)
 * ============================================================================ */

/**
 * Fast memory copy (SIMD optimized)
 */
void nova_memcpy_aligned(void *dest, const void *src, size_t n);

/**
 * Fast memory set (AVX2 optimized)
 */
void nova_memset_aligned(void *ptr, int value, size_t n);

/**
 * Fast pixel blending (SSE4 optimized)
 */
void nova_blend_pixels(uint32_t *dest, uint32_t src, float alpha);

/**
 * Fast matrix multiplication (SSE optimized)
 */
void nova_matrix_multiply(float *result, const float *a, const float *b);

/**
 * Fast integer square root
 */
uint32_t nova_isqrt(uint32_t n);

/**
 * Get CPU feature flags
 */
uint32_t nova_get_cpu_features(void);

/**
 * Get current time in milliseconds
 */
uint64_t nova_get_time_ms(void);

/* ============================================================================
 * Window Manager API
 * ============================================================================ */

/**
 * Create window manager instance
 */
struct nova_wm *nova_wm_create(struct nova_server *server);

/**
 * Destroy window manager
 */
void nova_wm_destroy(struct nova_wm *wm);

/**
 * Update window animations
 */
void nova_wm_update_animations(struct nova_wm *wm);

/**
 * Switch to workspace
 */
void nova_wm_switch_workspace(struct nova_wm *wm, int workspace_id);

/**
 * Snap view to layout
 */
void nova_wm_snap_view(struct nova_wm *wm, struct nova_view *view, int snap_type);

/**
 * Check snap zone at coordinates
 */
void *nova_wm_check_snap_zone(struct nova_wm *wm, double x, double y);

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
