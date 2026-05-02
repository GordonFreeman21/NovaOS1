/*
 * Jim Output Management
 * 
 * Monitor/output handling for JimDe compositor including
 * multi-monitor support, resolution, and refresh rate management
 * 
 * Copyright (C) 2024 JimOS Project
 * Licensed under GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_configuration_v1.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <drm_fourcc.h>

#include "jim-compositor.h"

/* Internal structures */
struct jim_server {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_scene *scene;
    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_list views;
    struct wl_list seats;
    void *focused_view;
    void *current_seat;
    void *wm;
    volatile sig_atomic_t quit;
};

struct jim_output {
    struct wl_list link;
    struct jim_server *server;
    struct wlr_output *wlr_output;
    struct wlr_output_layout_output *layout_output;
    int current_workspace;
    int workspace_count;
    bool workspaces_updated;
    int32_t width, height;
    float scale;
    int refresh_mhz;
    char name[64];
    char description[256];
    bool enabled;
    bool powered_on;
    struct wl_listener frame;
    struct wl_listener request_state;
};

/**
 * Get preferred mode for output
 */
static struct wlr_output_mode *jim_output_preferred_mode(struct wlr_output *output) {
    struct wlr_output_mode *mode;
    struct wlr_output_mode *preferred = NULL;
    
    wl_list_for_each(mode, &output->modes, link) {
        if (mode->preferred) {
            preferred = mode;
            break;
        }
    }
    
    /* Fallback to highest resolution */
    if (!preferred) {
        wl_list_for_each(mode, &output->modes, link) {
            if (!preferred || mode->width > preferred->width ||
                (mode->width == preferred->width && mode->height > preferred->height)) {
                preferred = mode;
            }
        }
    }
    
    return preferred;
}

/**
 * Frame render handler
 */
static void handle_output_frame(struct wl_listener *listener, void *data) {
    struct jim_output *output = wl_container_of(listener, output, frame);
    struct wlr_output *wlr_output = output->wlr_output;
    
    /* Check if output is enabled */
    if (!wlr_output->enabled) {
        return;
    }
    
    /* Begin rendering */
    struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
        output->server->scene, wlr_output);
    
    if (!scene_output) {
        return;
    }
    
    /* Render frame */
    wlr_scene_output_commit(scene_output, NULL);
    
    /* Update statistics */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    /* Schedule next frame */
    struct wlr_render_pass *render_pass = wlr_renderer_begin_render_pass(
        wlr_output->renderer);
    if (render_pass) {
        wlr_render_pass_end(render_pass);
    }
}

/**
 * Output state request handler
 */
static void handle_output_request_state(struct wl_listener *listener, void *data) {
    struct jim_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    
    wlr_output_commit_state(output->wlr_output, event->state);
}

/**
 * Configure output with mode and settings
 */
static void jim_output_configure(struct jim_output *output,
                                   struct wlr_output_mode *mode,
                                   float scale,
                                   int32_t transform) {
    struct wlr_output *wlr_output = output->wlr_output;
    
    /* Set mode */
    if (mode) {
        wlr_output_set_mode(wlr_output, mode);
    } else {
        mode = jim_output_preferred_mode(wlr_output);
        if (mode) {
            wlr_output_set_mode(wlr_output, mode);
        }
    }
    
    /* Set scale */
    if (scale > 0) {
        wlr_output_set_scale(wlr_output, scale);
        output->scale = scale;
    }
    
    /* Set transform (rotation) */
    if (transform >= 0) {
        wlr_output_set_transform(wlr_output, transform);
    }
    
    /* Enable output */
    wlr_output_enable(wlr_output, true);
    
    /* Commit changes */
    if (!wlr_output_commit(wlr_output)) {
        fprintf(stderr, "Failed to commit output %s\n", wlr_output->name);
    }
}

/**
 * Create output from wlr_output
 */
static struct jim_output *jim_output_create(struct jim_server *server,
                                               struct wlr_output *wlr_output) {
    struct jim_output *output = calloc(1, sizeof(*output));
    if (!output) {
        fprintf(stderr, "Failed to allocate output\n");
        return NULL;
    }
    
    output->server = server;
    output->wlr_output = wlr_output;
    output->workspace_count = 10;
    output->current_workspace = 0;
    output->scale = 1.0f;
    output->enabled = true;
    output->powered_on = true;
    
    /* Copy name and description */
    strncpy(output->name, wlr_output->name, sizeof(output->name) - 1);
    output->name[sizeof(output->name) - 1] = '\0';
    
    if (wlr_output->description) {
        strncpy(output->description, wlr_output->description, 
                sizeof(output->description) - 1);
    } else {
        snprintf(output->description, sizeof(output->description), 
                 "Output %s", wlr_output->name);
    }
    
    /* Set up listeners */
    output->frame.notify = handle_output_frame;
    output->request_state.notify = handle_output_request_state;
    
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);
    
    /* Add to output layout */
    output->layout_output = wlr_output_layout_add_auto(server->output_layout, 
                                                        wlr_output);
    if (!output->layout_output) {
        fprintf(stderr, "Failed to add output to layout: %s\n", wlr_output->name);
        free(output);
        return NULL;
    }
    
    /* Configure output */
    jim_output_configure(output, NULL, 1.0f, -1);
    
    /* Get dimensions */
    output->width = wlr_output->width;
    output->height = wlr_output->height;
    output->refresh_mhz = wlr_output->refresh;
    
    /* Add to server's output list */
    wl_list_insert(&server->outputs, &output->link);
    
    printf("Output created: %s (%s) %dx%d@%.2fHz\n",
           output->name, output->description,
           output->width, output->height,
           output->refresh_mhz / 1000.0f);
    
    return output;
}

/**
 * Destroy output
 */
static void jim_output_destroy(struct jim_output *output) {
    if (!output) return;
    
    /* Remove listeners */
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    
    /* Remove from layout */
    if (output->layout_output) {
        wlr_output_layout_remove(output->server->output_layout, 
                                  output->wlr_output);
    }
    
    /* Remove from list */
    wl_list_remove(&output->link);
    
    printf("Output destroyed: %s\n", output->name);
    
    free(output);
}

/**
 * Handle new output connection
 */
static void handle_new_output(struct wl_listener *listener, void *data) {
    struct jim_server *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;
    
    /* Configure output for desktop use */
    wlr_output_init_render(wlr_output, server->allocator, server->renderer);
    
    /* Create nova output */
    jim_output_create(server, wlr_output);
}

/**
 * Initialize output subsystem
 */
int jim_output_init(struct jim_server *server) {
    wl_list_init(&server->outputs);
    
    /* Set up output listener */
    server->new_output.notify = handle_new_output;
    wl_signal_add(&server->backend->events.new_output, &server->new_output);
    
    printf("Output subsystem initialized\n");
    
    return 0;
}

/**
 * Cleanup output subsystem
 */
void jim_output_fini(struct jim_server *server) {
    struct jim_output *output, *tmp;
    
    wl_list_for_each_safe(output, tmp, &server->outputs, link) {
        jim_output_destroy(output);
    }
    
    printf("Output subsystem cleaned up\n");
}

/**
 * Get primary output (first enabled output)
 */
struct jim_output *jim_server_get_primary_output(struct jim_server *server) {
    if (!server) return NULL;
    
    struct jim_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        if (output->enabled && output->powered_on) {
            return output;
        }
    }
    
    return NULL;
}

/**
 * Get output count
 */
int jim_server_get_output_count(struct jim_server *server) {
    if (!server) return 0;
    
    int count = 0;
    struct jim_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        if (output->enabled) {
            count++;
        }
    }
    
    return count;
}

/**
 * Set output scale
 */
int jim_output_set_scale(struct jim_output *output, float scale) {
    if (!output || scale <= 0) return -1;
    
    wlr_output_set_scale(output->wlr_output, scale);
    output->scale = scale;
    
    return wlr_output_commit(output->wlr_output);
}

/**
 * Set output mode
 */
int jim_output_set_mode(struct jim_output *output, 
                         int32_t width, int32_t height, 
                         int32_t refresh_mhz) {
    if (!output) return -1;
    
    struct wlr_output_mode *mode, *best_mode = NULL;
    
    wl_list_for_each(mode, &output->wlr_output->modes, link) {
        if (mode->width == width && mode->height == height) {
            if (refresh_mhz <= 0 || mode->refresh == refresh_mhz) {
                best_mode = mode;
                break;
            }
            if (!best_mode || abs(mode->refresh - refresh_mhz) < 
                              abs(best_mode->refresh - refresh_mhz)) {
                best_mode = mode;
            }
        }
    }
    
    if (!best_mode) {
        fprintf(stderr, "No suitable mode found for %dx%d\n", width, height);
        return -1;
    }
    
    wlr_output_set_mode(output->wlr_output, best_mode);
    output->width = width;
    output->height = height;
    output->refresh_mhz = best_mode->refresh;
    
    return wlr_output_commit(output->wlr_output);
}

/**
 * Arrange outputs in layout
 */
int jim_output_arrange(struct jim_server *server, 
                        const char *output_name,
                        int32_t x, int32_t y) {
    struct jim_output *output;
    
    wl_list_for_each(output, &server->outputs, link) {
        if (strcmp(output->name, output_name) == 0) {
            wlr_output_layout_add(server->output_layout, 
                                  output->wlr_output, x, y);
            return 0;
        }
    }
    
    return -1;
}

/**
 * Enable/disable output
 */
int jim_output_set_enabled(struct jim_output *output, bool enabled) {
    if (!output) return -1;
    
    wlr_output_enable(output->wlr_output, enabled);
    output->enabled = enabled;
    
    if (!enabled) {
        output->powered_on = false;
    }
    
    return wlr_output_commit(output->wlr_output);
}
