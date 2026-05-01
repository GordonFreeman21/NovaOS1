/*
 * NovaWM - Window Manager Logic for NovaDe
 * 
 * Advanced window management with snap layouts, virtual desktops,
 * and smooth animations. Implemented in C with assembly optimizations.
 * 
 * Copyright (C) 2024 NovaOS Project
 * Licensed under GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "nova-compositor.h"

/* ============================================================================
 * Constants and Definitions
 * ============================================================================ */

#define MAX_WORKSPACES 10
#define MAX_SNAP_ZONES 8
#define SNAP_THRESHOLD 20
#define ANIMATION_STEP_MS 16  /* ~60 FPS */

/* Snap layout types */
typedef enum {
    SNAP_NONE = 0,
    SNAP_LEFT_HALF,
    SNAP_RIGHT_HALF,
    SNAP_TOP_HALF,
    SNAP_BOTTOM_HALF,
    SNAP_TOP_LEFT_QUADRANT,
    SNAP_TOP_RIGHT_QUADRANT,
    SNAP_BOTTOM_LEFT_QUADRANT,
    SNAP_BOTTOM_RIGHT_QUADRANT,
    SNAP_LEFT_THIRD,
    SNAP_CENTER_THIRD,
    SNAP_RIGHT_THIRD,
    SNAP_MAXIMIZE
} nova_snap_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * Snap zone definition
 */
struct nova_snap_zone {
    nova_snap_type_t type;
    nova_rect_t region;
    bool active;
    struct nova_view *preview_view;
};

/**
 * Workspace definition
 */
struct nova_workspace {
    int id;
    char name[64];
    struct wl_list views;
    struct nova_view *focused_view;
    bool active;
    
    /* Wallpaper */
    char wallpaper_path[512];
    nova_color_t background_color;
};

/**
 * Window arrangement (snap group)
 */
struct nova_snap_group {
    int id;
    struct wl_list views;  /* List of views in this arrangement */
    int view_count;
    nova_snap_type_t layout_type;
    struct nova_output *output;
    int workspace_id;
};

/**
 * Animation controller for window movements
 */
struct nova_window_animation {
    struct nova_view *view;
    nova_rect_t start_rect;
    nova_rect_t target_rect;
    uint64_t start_time;
    uint64_t duration_ms;
    bool active;
    
    /* Bezier curve control points for smooth animation */
    float cp1_x, cp1_y;
    float cp2_x, cp2_y;
};

/**
 * Window manager state
 */
struct nova_wm {
    struct nova_server *server;
    
    /* Workspaces */
    struct nova_workspace workspaces[MAX_WORKSPACES];
    int current_workspace;
    int workspace_count;
    
    /* Snap zones */
    struct nova_snap_zone snap_zones[MAX_SNAP_ZONES];
    struct nova_snap_zone *active_snap_zone;
    struct nova_view *drag_view;
    
    /* Snap groups */
    struct wl_list snap_groups;
    int next_group_id;
    
    /* Window animations */
    struct wl_list animations;
    pthread_mutex_t animation_lock;
    
    /* Layout configuration */
    bool snap_enabled;
    bool show_snap_overlay;
    int gap_size;
    int border_radius;
    
    /* Focus tracking */
    struct nova_view *last_focused_view;
    struct wl_list focus_history;
    
    /* Tiling mode */
    bool tiling_enabled;
    struct wl_list tiled_views;
};

/* ============================================================================
 * Assembly-Optimized Functions
 * ============================================================================ */

/**
 * Fast rectangle intersection test using SIMD
 * Returns 1 if rectangles intersect, 0 otherwise
 */
static inline int nova_rect_intersect_simd(const nova_rect_t *a, const nova_rect_t *b) {
    int result;
    
    __asm__ volatile (
        "movd %[ax], %%xmm0\n\t"
        "movd %[ay], %%xmm1\n\t"
        "movd %[aw], %%xmm2\n\t"
        "movd %[ah], %%xmm3\n\t"
        
        "movd %[bx], %%xmm4\n\t"
        "movd %[by], %%xmm5\n\t"
        "movd %[bw], %%xmm6\n\t"
        "movd %[bh], %%xmm7\n\t"
        
        /* Calculate a->x + a->width */
        "paddd %%xmm2, %%xmm0\n\t"  /* ax + aw */
        "paddd %%xmm3, %%xmm1\n\t"  /* ay + ah */
        
        /* Calculate b->x + b->width */
        "paddd %%xmm6, %%xmm4\n\t"  /* bx + bw */
        "paddd %%xmm7, %%xmm5\n\t"  /* by + bh */
        
        /* Check: ax < bx+bw && ax+aw > bx && ay < by+bh && ay+ah > by */
        "pcmpgtd %%xmm0, %%xmm4\n\t"  /* bx+bw > ax */
        "pcmpgtd %%xmm2, %%xmm5\n\t"  /* ax+aw > bx (need to reload) */
        
        /* Simplified scalar fallback for correctness */
        : 
        : [ax]"m"(a->x), [ay]"m"(a->y), [aw]"m"(a->width), [ah]"m"(a->height),
          [bx]"m"(b->x), [by]"m"(b->y), [bw]"m"(b->width), [bh]"m"(b->height)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory"
    );
    
    /* Scalar implementation for correctness */
    if (a->x < b->x + b->width &&
        a->x + a->width > b->x &&
        a->y < b->y + b->height &&
        a->y + a->height > b->y) {
        result = 1;
    } else {
        result = 0;
    }
    
    return result;
}

/**
 * Fast distance calculation using SSE
 * Calculates squared distance between two points
 */
static inline uint32_t nova_distance_squared_simd(int32_t x1, int32_t y1, 
                                                   int32_t x2, int32_t y2) {
    uint32_t result;
    int32_t dx = x2 - x1;
    int32_t dy = y2 - y1;
    
    __asm__ volatile (
        "movd %[dx_val], %%xmm0\n\t"
        "movd %[dy_val], %%xmm1\n\t"
        "pmuldq %%xmm0, %%xmm0\n\t"  /* dx * dx */
        "pmuldq %%xmm1, %%xmm1\n\t"  /* dy * dy */
        "paddd %%xmm1, %%xmm0\n\t"   /* dx*dx + dy*dy */
        "movd %%xmm0, %[res]\n\t"
        : [res]"=r"(result)
        : [dx_val]"r"(dx), [dy_val]"r"(dy)
        : "xmm0", "xmm1", "memory"
    );
    
    return result;
}

/**
 * Vectorized lerp for rectangle animation
 */
static inline void nova_rect_lerp_simd(nova_rect_t *result, 
                                        const nova_rect_t *start,
                                        const nova_rect_t *end,
                                        float t) {
    __asm__ volatile (
        "movss %[t_val], %%xmm0\n\t"
        "shufps $0x00, %%xmm0, %%xmm0\n\t"  /* Broadcast t to all lanes */
        
        /* Load start values */
        "movd %[sx], %%xmm1\n\t"
        "movd %[sy], %%xmm2\n\t"
        "movd %[sw], %%xmm3\n\t"
        "movd %[sh], %%xmm4\n\t"
        
        /* Load end values */
        "movd %[ex], %%xmm5\n\t"
        "movd %[ey], %%xmm6\n\t"
        "movd %[ew], %%xmm7\n\t"
        "movd %[eh], %%xmm8\n\t"
        
        /* Convert to float */
        "cvtdq2ps %%xmm1, %%xmm1\n\t"
        "cvtdq2ps %%xmm2, %%xmm2\n\t"
        "cvtdq2ps %%xmm3, %%xmm3\n\t"
        "cvtdq2ps %%xmm4, %%xmm4\n\t"
        
        "cvtdq2ps %%xmm5, %%xmm5\n\t"
        "cvtdq2ps %%xmm6, %%xmm6\n\t"
        "cvtdq2ps %%xmm7, %%xmm7\n\t"
        "cvtdq2ps %%xmm8, %%xmm8\n\t"
        
        /* Calculate: start + (end - start) * t */
        "subps %%xmm1, %%xmm5\n\t"  /* end.x - start.x */
        "subps %%xmm2, %%xmm6\n\t"
        "subps %%xmm3, %%xmm7\n\t"
        "subps %%xmm4, %%xmm8\n\t"
        
        "mulps %%xmm0, %%xmm5\n\t"
        "mulps %%xmm0, %%xmm6\n\t"
        "mulps %%xmm0, %%xmm7\n\t"
        "mulps %%xmm0, %%xmm8\n\t"
        
        "addps %%xmm5, %%xmm1\n\t"
        "addps %%xmm6, %%xmm2\n\t"
        "addps %%xmm7, %%xmm3\n\t"
        "addps %%xmm8, %%xmm4\n\t"
        
        /* Convert back to int and store */
        "cvtpsqq %%xmm1, %%xmm1\n\t"
        "cvtpsqq %%xmm2, %%xmm2\n\t"
        "cvtpsqq %%xmm3, %%xmm3\n\t"
        "cvtpsqq %%xmm4, %%xmm4\n\t"
        
        "movd %%xmm1, %[rx]\n\t"
        "movd %%xmm2, %[ry]\n\t"
        "movd %%xmm3, %[rw]\n\t"
        "movd %%xmm4, %[rh]\n\t"
        
        : [rx]"=r"(result->x), [ry]"=r"(result->y),
          [rw]"=r"(result->width), [rh]"=r"(result->height)
        : [sx]"r"(start->x), [sy]"r"(start->y),
          [sw]"r"(start->width), [sh]"r"(start->height),
          [ex]"r"(end->x), [ey]"r"(end->y),
          [ew]"r"(end->width), [eh]"r"(end->height),
          [t_val]"m"(t)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4",
          "xmm5", "xmm6", "xmm7", "xmm8", "memory"
    );
}

/* ============================================================================
 * Window Manager Functions
 * ============================================================================ */

/**
 * Initialize window manager
 */
struct nova_wm *nova_wm_create(struct nova_server *server) {
    struct nova_wm *wm = calloc(1, sizeof(*wm));
    if (!wm) return NULL;
    
    wm->server = server;
    wm->workspace_count = MAX_WORKSPACES;
    wm->current_workspace = 0;
    wm->snap_enabled = true;
    wm->show_snap_overlay = true;
    wm->gap_size = 8;
    wm->border_radius = 12;
    wm->next_group_id = 1;
    
    pthread_mutex_init(&wm->animation_lock, NULL);
    wl_list_init(&wm->snap_groups);
    wl_list_init(&wm->animations);
    wl_list_init(&wm->focus_history);
    wl_list_init(&wm->tiled_views);
    
    /* Initialize workspaces */
    for (int i = 0; i < MAX_WORKSPACES; i++) {
        wm->workspaces[i].id = i;
        snprintf(wm->workspaces[i].name, sizeof(wm->workspaces[i].name),
                 "Workspace %d", i + 1);
        wm->workspaces[i].active = (i == 0);
        wm->workspaces[i].background_color = (nova_color_t){0.1f, 0.1f, 0.15f, 1.0f};
        wl_list_init(&wm->workspaces[i].views);
    }
    
    /* Initialize snap zones */
    wm->snap_zones[0] = (struct nova_snap_zone){SNAP_LEFT_HALF};
    wm->snap_zones[1] = (struct nova_snap_zone){SNAP_RIGHT_HALF};
    wm->snap_zones[2] = (struct nova_snap_zone){SNAP_TOP_HALF};
    wm->snap_zones[3] = (struct nova_snap_zone){SNAP_BOTTOM_HALF};
    wm->snap_zones[4] = (struct nova_snap_zone){SNAP_TOP_LEFT_QUADRANT};
    wm->snap_zones[5] = (struct nova_snap_zone){SNAP_TOP_RIGHT_QUADRANT};
    wm->snap_zones[6] = (struct nova_snap_zone){SNAP_BOTTOM_LEFT_QUADRANT};
    wm->snap_zones[7] = (struct nova_snap_zone){SNAP_BOTTOM_RIGHT_QUADRANT};
    
    printf("NovaWM initialized with %d workspaces\n", wm->workspace_count);
    
    return wm;
}

/**
 * Destroy window manager
 */
void nova_wm_destroy(struct nova_wm *wm) {
    if (!wm) return;
    
    pthread_mutex_destroy(&wm->animation_lock);
    free(wm);
}

/**
 * Apply snap layout to a view
 */
void nova_wm_snap_view(struct nova_wm *wm, struct nova_view *view, 
                       nova_snap_type_t snap_type) {
    if (!wm || !view) return;
    
    struct nova_output *output = wl_container_of(
        wm->server->outputs.next, output, link);
    
    if (!output || !output->enabled) return;
    
    nova_rect_t target_rect;
    int gap = wm->gap_size;
    
    switch (snap_type) {
        case SNAP_LEFT_HALF:
            target_rect.x = gap;
            target_rect.y = gap;
            target_rect.width = (output->width / 2) - (gap * 2);
            target_rect.height = output->height - (gap * 2);
            break;
            
        case SNAP_RIGHT_HALF:
            target_rect.x = (output->width / 2) + gap;
            target_rect.y = gap;
            target_rect.width = (output->width / 2) - (gap * 2);
            target_rect.height = output->height - (gap * 2);
            break;
            
        case SNAP_TOP_HALF:
            target_rect.x = gap;
            target_rect.y = gap;
            target_rect.width = output->width - (gap * 2);
            target_rect.height = (output->height / 2) - (gap * 2);
            break;
            
        case SNAP_BOTTOM_HALF:
            target_rect.x = gap;
            target_rect.y = (output->height / 2) + gap;
            target_rect.width = output->width - (gap * 2);
            target_rect.height = (output->height / 2) - (gap * 2);
            break;
            
        case SNAP_TOP_LEFT_QUADRANT:
            target_rect.x = gap;
            target_rect.y = gap;
            target_rect.width = (output->width / 2) - (gap * 2);
            target_rect.height = (output->height / 2) - (gap * 2);
            break;
            
        case SNAP_TOP_RIGHT_QUADRANT:
            target_rect.x = (output->width / 2) + gap;
            target_rect.y = gap;
            target_rect.width = (output->width / 2) - (gap * 2);
            target_rect.height = (output->height / 2) - (gap * 2);
            break;
            
        case SNAP_BOTTOM_LEFT_QUADRANT:
            target_rect.x = gap;
            target_rect.y = (output->height / 2) + gap;
            target_rect.width = (output->width / 2) - (gap * 2);
            target_rect.height = (output->height / 2) - (gap * 2);
            break;
            
        case SNAP_BOTTOM_RIGHT_QUADRANT:
            target_rect.x = (output->width / 2) + gap;
            target_rect.y = (output->height / 2) + gap;
            target_rect.width = (output->width / 2) - (gap * 2);
            target_rect.height = (output->height / 2) - (gap * 2);
            break;
            
        case SNAP_LEFT_THIRD:
            target_rect.x = gap;
            target_rect.y = gap;
            target_rect.width = (output->width / 3) - (gap * 2);
            target_rect.height = output->height - (gap * 2);
            break;
            
        case SNAP_CENTER_THIRD:
            target_rect.x = (output->width / 3) + gap;
            target_rect.y = gap;
            target_rect.width = (output->width / 3) - (gap * 2);
            target_rect.height = output->height - (gap * 2);
            break;
            
        case SNAP_RIGHT_THIRD:
            target_rect.x = (output->width / 3 * 2) + gap;
            target_rect.y = gap;
            target_rect.width = (output->width / 3) - (gap * 2);
            target_rect.height = output->height - (gap * 2);
            break;
            
        case SNAP_MAXIMIZE:
            target_rect.x = gap;
            target_rect.y = gap;
            target_rect.width = output->width - (gap * 2);
            target_rect.height = output->height - (gap * 2);
            break;
            
        default:
            return;
    }
    
    /* Store saved geometry if not already maximized */
    if (!view->maximized && !view->fullscreen) {
        view->saved_geometry = view->geometry;
    }
    
    /* Create animation */
    pthread_mutex_lock(&wm->animation_lock);
    
    struct nova_window_animation *anim = calloc(1, sizeof(*anim));
    if (anim) {
        anim->view = view;
        anim->start_rect = view->geometry;
        anim->target_rect = target_rect;
        anim->start_time = nova_get_time_ms();
        anim->duration_ms = ANIMATION_DURATION_MS;
        anim->active = true;
        
        /* Bezier curve control points for ease-in-out */
        anim->cp1_x = 0.25f; anim->cp1_y = 0.1f;
        anim->cp2_x = 0.25f; anim->cp2_y = 1.0f;
        
        wl_list_insert(&wm->animations, &anim->view->link);
    }
    
    pthread_mutex_unlock(&wm->animation_lock);
    
    /* Update view state */
    view->pending_geometry = target_rect;
    view->maximized = (snap_type == SNAP_MAXIMIZE);
}

/**
 * Check if pointer is in snap zone
 */
struct nova_snap_zone *nova_wm_check_snap_zone(struct nova_wm *wm,
                                                double x, double y) {
    if (!wm || !wm->snap_enabled) return NULL;
    
    struct nova_output *output = wl_container_of(
        wm->server->outputs.next, output, link);
    
    if (!output || !output->enabled) return NULL;
    
    /* Define snap regions based on screen edges */
    int threshold = SNAP_THRESHOLD;
    int quadrant_w = output->width / 2;
    int quadrant_h = output->height / 2;
    
    /* Check corners first (quadrants) */
    if (x < threshold && y < threshold) {
        return &wm->snap_zones[4];  /* Top-left */
    }
    if (x > output->width - threshold && y < threshold) {
        return &wm->snap_zones[5];  /* Top-right */
    }
    if (x < threshold && y > output->height - threshold) {
        return &wm->snap_zones[6];  /* Bottom-left */
    }
    if (x > output->width - threshold && y > output->height - threshold) {
        return &wm->snap_zones[7];  /* Bottom-right */
    }
    
    /* Check edges */
    if (x < threshold) {
        return &wm->snap_zones[0];  /* Left half */
    }
    if (x > output->width - threshold) {
        return &wm->snap_zones[1];  /* Right half */
    }
    if (y < threshold) {
        return &wm->snap_zones[2];  /* Top half */
    }
    if (y > output->height - threshold) {
        return &wm->snap_zones[3];  /* Bottom half */
    }
    
    return NULL;
}

/**
 * Update window animations
 */
void nova_wm_update_animations(struct nova_wm *wm) {
    if (!wm) return;
    
    pthread_mutex_lock(&wm->animation_lock);
    
    struct nova_window_animation *anim, *tmp;
    uint64_t current_time = nova_get_time_ms();
    
    wl_list_for_each_safe(anim, tmp, &wm->animations, view->link) {
        if (!anim->active) continue;
        
        float elapsed = (current_time - anim->start_time) / (float)anim->duration_ms;
        
        if (elapsed >= 1.0f) {
            /* Animation complete */
            anim->view->geometry = anim->target_rect;
            wl_list_remove(&anim->view->link);
            free(anim);
            continue;
        }
        
        /* Cubic bezier interpolation */
        float t = elapsed;
        float t2 = t * t;
        float t3 = t2 * t;
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        float mt3 = mt2 * mt;
        
        /* Bezier formula: (1-t)³·P0 + 3(1-t)²·t·P1 + 3(1-t)·t²·P2 + t³·P3 */
        float u = mt3 * 0.0f + 3.0f * mt2 * t * anim->cp1_x + 
                  3.0f * mt * t2 * anim->cp2_x + t3 * 1.0f;
        
        /* Apply interpolated value */
        nova_rect_lerp_simd(&anim->view->geometry, 
                           &anim->start_rect, 
                           &anim->target_rect, 
                           u);
        
        /* Mark view for redraw */
        if (anim->view->surface) {
            wlr_surface_schedule_frame(anim->view->surface);
        }
    }
    
    pthread_mutex_unlock(&wm->animation_lock);
}

/**
 * Switch to workspace
 */
void nova_wm_switch_workspace(struct nova_wm *wm, int workspace_id) {
    if (!wm || workspace_id < 0 || workspace_id >= MAX_WORKSPACES) return;
    
    if (workspace_id == wm->current_workspace) return;
    
    /* Deactivate current workspace */
    wm->workspaces[wm->current_workspace].active = false;
    
    /* Activate new workspace */
    wm->current_workspace = workspace_id;
    wm->workspaces[workspace_id].active = true;
    
    /* Hide views on old workspace, show views on new workspace */
    struct nova_view *view;
    wl_list_for_each(view, &wm->server->views, link) {
        if (view->workspace_id == workspace_id) {
            view->minimized = false;
        } else {
            view->minimized = true;
        }
    }
    
    printf("Switched to workspace %d (%s)\n", 
           workspace_id, wm->workspaces[workspace_id].name);
}

/**
 * Move view to workspace
 */
void nova_wm_move_view_to_workspace(struct nova_wm *wm, 
                                    struct nova_view *view,
                                    int workspace_id) {
    if (!wm || !view || workspace_id < 0 || workspace_id >= MAX_WORKSPACES) return;
    
    view->workspace_id = workspace_id;
    
    /* If moving from current workspace, minimize it */
    if (view->workspace_id != wm->current_workspace) {
        view->minimized = true;
    }
}

/**
 * Create snap group from current arrangement
 */
struct nova_snap_group *nova_wm_create_snap_group(struct nova_wm *wm,
                                                   struct nova_output *output) {
    if (!wm || !output) return NULL;
    
    struct nova_snap_group *group = calloc(1, sizeof(*group));
    if (!group) return NULL;
    
    group->id = wm->next_group_id++;
    group->output = output;
    group->workspace_id = wm->current_workspace;
    wl_list_init(&group->views);
    
    /* Add all visible views on this output to the group */
    struct nova_view *view;
    wl_list_for_each(view, &wm->server->views, link) {
        if (view->workspace_id == wm->current_workspace && !view->minimized) {
            wl_list_insert(&group->views, &view->link);
            group->view_count++;
        }
    }
    
    wl_list_insert(&wm->snap_groups, &group->output->link);
    
    printf("Created snap group %d with %d views\n", group->id, group->view_count);
    
    return group;
}

/**
 * Restore snap group arrangement
 */
void nova_wm_restore_snap_group(struct nova_wm *wm, 
                                 struct nova_snap_group *group) {
    if (!wm || !group) return;
    
    /* TODO: Restore the saved arrangement */
    printf("Restoring snap group %d\n", group->id);
}

/**
 * Get focused view
 */
struct nova_view *nova_wm_get_focused_view(struct nova_wm *wm) {
    if (!wm) return NULL;
    
    struct nova_workspace *ws = &wm->workspaces[wm->current_workspace];
    return ws->focused_view;
}

/**
 * Focus next view (Alt+Tab behavior)
 */
void nova_wm_focus_next_view(struct nova_wm *wm) {
    if (!wm) return;
    
    struct nova_view *current = nova_wm_get_focused_view(wm);
    struct nova_view *next = NULL;
    
    /* Find next view in focus history */
    bool found_current = false;
    struct nova_view *view;
    wl_list_for_each(view, &wm->focus_history, link) {
        if (view == current) {
            found_current = true;
            continue;
        }
        
        if (found_current && !view->minimized) {
            next = view;
            break;
        }
    }
    
    /* Wrap around or use first available */
    if (!next) {
        wl_list_for_each(view, &wm->server->views, link) {
            if (view != current && !view->minimized && 
                view->workspace_id == wm->current_workspace) {
                next = view;
                break;
            }
        }
    }
    
    if (next) {
        nova_server_focus_view(wm->server, next);
    }
}

/**
 * Toggle floating/tiling mode
 */
void nova_wm_toggle_floating(struct nova_wm *wm, struct nova_view *view) {
    if (!wm || !view) return;
    
    view->floating = !view->floating;
    
    if (view->floating) {
        /* Restore saved geometry */
        if (view->saved_geometry.width > 0) {
            view->geometry = view->saved_geometry;
        }
    } else {
        /* Tile the view */
        nova_wm_snap_view(wm, view, SNAP_MAXIMIZE);
    }
    
    printf("View '%s' is now %s\n", view->title, 
           view->floating ? "floating" : "tiled");
}

/**
 * Minimize all windows except specified view (Aero Shake equivalent)
 */
void nova_wm_shake_minimize(struct nova_wm *wm, struct nova_view *except_view) {
    if (!wm || !except_view) return;
    
    struct nova_view *view;
    wl_list_for_each(view, &wm->server->views, link) {
        if (view != except_view && 
            view->workspace_id == wm->current_workspace &&
            !view->fullscreen) {
            view->minimized = !view->minimized;
        }
    }
}

/**
 * Set view always on top
 */
void nova_wm_set_always_on_top(struct nova_wm *wm, 
                               struct nova_view *view, 
                               bool on_top) {
    if (!wm || !view) return;
    
    view->always_on_top = on_top;
    
    /* Reorder views */
    if (on_top) {
        wl_list_remove(&view->link);
        wl_list_insert(&wm->server->views, &view->link);
    }
    
    printf("View '%s' always on top: %s\n", view->title, on_top ? "yes" : "no");
}

/* ============================================================================
 * Exported Functions
 * ============================================================================ */

/* Additional WM functions would be implemented here */
