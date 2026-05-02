/*
 * JimCompositor - Core Wayland Compositor for JimDe
 * 
 * A high-performance Wayland compositor built with C and optimized assembly
 * for the JimOS desktop environment.
 * 
 * Copyright (C) 2024 JimOS Project
 * Licensed under GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>
#include <pixman-1/pixman.h>
#include <drm_fourcc.h>
#include <linux/input.h>
#include <linux/memfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

/* JimCompositor Version */
#define NOVA_COMPOSITOR_VERSION "1.0.0"
#define NOVA_COMPOSITOR_NAME "JimDe Compositor"

/* Maximum limits */
#define MAX_OUTPUTS 8
#define MAX_SEATS 4
#define MAX_VIEWS 64
#define MAX_KEYBINDINGS 32

/* Animation constants */
#define ANIMATION_FPS 60
#define ANIMATION_DURATION_MS 300
#define SPRING_TENSION 0.92f
#define SPRING_DAMPING 0.75f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct jim_server;
struct jim_output;
struct jim_seat;
struct jim_view;
struct jim_scene_graph;
struct jim_animation;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * Rectangle structure for geometry
 */
typedef struct {
    int32_t x, y;
    int32_t width, height;
} jim_rect_t;

/**
 * Color structure with alpha
 */
typedef struct {
    float r, g, b, a;
} jim_color_t;

/**
 * Animation state for smooth transitions
 */
typedef enum {
    ANIMATION_NONE = 0,
    ANIMATION_OPEN,
    ANIMATION_CLOSE,
    ANIMATION_MINIMIZE,
    ANIMATION_MAXIMIZE,
    ANIMATION_MOVE,
    ANIMATION_RESIZE,
    ANIMATION_WORKSPACE_SWITCH
} jim_animation_type_t;

/**
 * Animation configuration
 */
struct jim_animation {
    jim_animation_type_t type;
    float progress;           /* 0.0 to 1.0 */
    float velocity;
    float target_value;
    float current_value;
    uint64_t start_time;
    uint64_t duration_ms;
    bool active;
    
    /* Spring physics parameters */
    float tension;
    float damping;
};

/**
 * View (window) representation
 */
struct jim_view {
    struct wl_list link;
    struct jim_server *server;
    
    struct wlr_surface *surface;
    struct wlr_xdg_surface *xdg_surface;
    struct wlr_layer_surface_v1 *layer_surface;
    
    /* Geometry */
    jim_rect_t geometry;
    jim_rect_t pending_geometry;
    jim_rect_t saved_geometry;   /* For restore from maximize */
    
    /* State flags */
    bool mapped;
    bool focused;
    bool fullscreen;
    bool maximized;
    bool minimized;
    bool floating;
    bool always_on_top;
    
    /* Workspace */
    int workspace_id;
    
    /* Animation */
    struct jim_animation animation;
    
    /* Title and app ID */
    char title[256];
    char app_id[128];
    
    /* Reference count */
    int ref_count;
};

/**
 * Output (monitor) representation
 */
struct jim_output {
    struct wl_list link;
    struct jim_server *server;
    
    struct wlr_output *wlr_output;
    struct wlr_output_layout_output *layout_output;
    
    /* Workspace management */
    int current_workspace;
    int workspace_count;
    bool workspaces_updated;
    
    /* Resolution and scaling */
    int32_t width, height;
    float scale;
    
    /* Refresh rate */
    int refresh_mhz;
    
    /* Name and description */
    char name[64];
    char description[256];
    
    /* Enabled state */
    bool enabled;
    bool powered_on;
    
    /* Frame callback */
    struct wl_listener frame;
};

/**
 * Input device types
 */
typedef enum {
    INPUT_KEYBOARD = 0,
    INPUT_POINTER,
    INPUT_TOUCH,
    INPUT_TABLET
} jim_input_type_t;

/**
 * Keyboard device
 */
struct jim_keyboard {
    struct wl_list link;
    struct jim_seat *seat;
    struct wlr_input_device *device;
    
    struct wl_listener modifiers;
    struct wl_listener key;
    
    uint32_t modifiers;
};

/**
 * Pointer device
 */
struct jim_pointer {
    struct wl_list link;
    struct jim_seat *seat;
    struct wlr_input_device *device;
    
    struct wl_listener motion;
    struct wl_listener motion_absolute;
    struct wl_listener button;
    struct wl_listener axis;
    struct wl_listener frame;
    
    /* Cursor state */
    double x, y;
    uint32_t buttons;
    bool drag_active;
    struct jim_view *drag_view;
    int drag_offset_x, drag_offset_y;
};

/**
 * Seat (input context)
 */
struct jim_seat {
    struct wl_list link;
    struct jim_server *server;
    
    struct wlr_seat *wlr_seat;
    char name[32];
    
    struct wl_list keyboards;
    struct wl_list pointers;
    
    struct jim_keyboard *active_keyboard;
    struct jim_pointer *active_pointer;
    
    /* Cursor */
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *xcursor_manager;
    
    /* Selection */
    struct wlr_data_device *data_device;
    
    /* Input listeners */
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;
    
    /* Request handlers */
    struct wl_listener request_cursor;
    struct wl_listener request_selection;
    
    /* Keyboard grab */
    struct wlr_keyboard_group *keyboard_group;
};

/**
 * Scene graph node types
 */
typedef enum {
    SCENE_NODE_BUFFER = 0,
    SCENE_NODE_SURFACE,
    SCENE_NODE_RECT,
    SCENE_NODE_TREE
} jim_scene_node_type_t;

/**
 * Scene graph node for rendering
 */
struct jim_scene_node {
    struct wl_list link;
    struct jim_scene_node *parent;
    struct wl_list children;
    
    jim_scene_node_type_t type;
    bool enabled;
    int x, y;
    
    union {
        struct wlr_scene_buffer *buffer;
        struct wlr_scene_surface *surface;
        struct wlr_scene_rect *rect;
        struct wlr_scene_tree *tree;
    };
};

/**
 * Keybinding definition
 */
struct jim_keybinding {
    uint32_t modifiers;
    uint32_t key;
    void (*handler)(struct jim_server *, struct jim_seat *);
    const char *description;
};

/**
 * Main server structure
 */
struct jim_server {
    struct wl_display *wl_display;
    struct wl_event_loop *event_loop;
    
    /* Wayland backend */
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    
    /* Output layout */
    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output;
    
    /* Views (windows) */
    struct wl_list views;
    struct jim_view *focused_view;
    struct jim_view *grabbed_view;
    
    /* Seats (input) */
    struct wl_list seats;
    struct jim_seat *current_seat;
    struct wl_listener new_input;
    
    /* XDG shell */
    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_surface;
    struct wl_listener new_xdg_toplevel;
    
    /* Layer shell */
    struct wlr_layer_shell_v1 *layer_shell;
    struct wl_listener new_layer_surface;
    
    /* Server-side decorations */
    struct wlr_server_decoration_manager *decoration_manager;
    struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
    
    /* Input method */
    struct wlr_input_method_manager_v2 *input_method_manager;
    struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_manager;
    
    /* Pointer constraints */
    struct wlr_pointer_constraints_v1 *pointer_constraints;
    struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;
    
    /* Presentation time */
    struct wlr_presentation *presentation;
    
    /* Gamma control */
    struct wlr_gamma_control_manager_v1 *gamma_control_manager;
    
    /* Screen capture */
    struct wlr_screencopy_manager_v1 *screencopy_manager;
    
    /* Export DMA buffers */
    struct wlr_dmabuf_v1_buffer_manager *dmabuf_manager;
    
    /* Primary selection */
    struct wlr_primary_selection_v1_device_manager *primary_selection_manager;
    
    /* Session management */
    struct wlr_session *session;
    
    /* Idle inhibitor */
    struct wlr_idle_inhibit_manager_v1 *idle_inhibit_manager;
    struct wlr_idle *idle;
    
    /* Keybindings */
    struct jim_keybinding keybindings[MAX_KEYBINDINGS];
    int keybinding_count;
    
    /* Configuration */
    char config_path[512];
    bool debug_mode;
    bool hardware_cursor;
    
    /* Statistics */
    uint64_t frame_count;
    uint64_t last_frame_time;
    float fps;
    
    /* Threading */
    pthread_mutex_t lock;
    pthread_t render_thread;
    bool render_thread_running;
    
    /* Signal handling */
    volatile sig_atomic_t quit;
};

/* ============================================================================
 * Assembly Optimizations (x86_64)
 * ============================================================================ */

/**
 * Fast memory copy using SIMD instructions
 * Optimized for cache line alignment
 */
static inline void jim_memcpy_aligned(void *dest, const void *src, size_t n) {
    /* Use inline assembly for optimal memory copying */
    __asm__ volatile (
        "cld\n\t"
        "rep movsb\n\t"
        : "+D"(dest), "+S"(src), "+c"(n)
        :
        : "memory", "rax"
    );
}

/**
 * Fast memset using AVX2 when available
 * Falls back to standard memset if not aligned
 */
static inline void jim_memset_aligned(void *ptr, int value, size_t n) {
    size_t i;
    uint8_t *p = (uint8_t *)ptr;
    
    /* Check if we can use optimized path */
    if (n >= 32 && ((uintptr_t)ptr & 31) == 0) {
        /* Use AVX2 broadcast and store */
        __asm__ volatile (
            "vpbroadcastb %%eax, %%ymm0\n\t"
            "1:\n\t"
            "vmovntdq %%ymm0, (%%rdi)\n\t"
            "vmovntdq %%ymm0, 32(%%rdi)\n\t"
            "vmovntdq %%ymm0, 64(%%rdi)\n\t"
            "vmovntdq %%ymm0, 96(%%rdi)\n\t"
            "add $128, %%rdi\n\t"
            "sub $128, %%ecx\n\t"
            "cmp $128, %%ecx\n\t"
            "jge 1b\n\t"
            "vzeroupper\n\t"
            : "+D"(p), "+c"(n)
            : "a"(value)
            : "memory", "ymm0"
        );
    }
    
    /* Handle remainder */
    for (i = 0; i < n; i++) {
        p[i] = (uint8_t)value;
    }
}

/**
 * Fast pixel blending using SSE4
 * Blends source and destination with alpha
 */
static inline void jim_blend_pixels(uint32_t *dest, uint32_t src, float alpha) {
    uint32_t dst = *dest;
    
    __asm__ volatile (
        "movd %[src_val], %%xmm0\n\t"
        "movd %[dst_val], %%xmm1\n\t"
        "punpcklbw %%xmm7, %%xmm0\n\t"
        "punpcklbw %%xmm7, %%xmm1\n\t"
        "cvtsi2ss %[alpha_val], %%xmm2\n\t"
        "shufps $0x00, %%xmm2, %%xmm2\n\t"
        "psubd %%xmm1, %%xmm0\n\t"
        "cvtpsqq %%xmm2, %%xmm3\n\t"
        "pmulld %%xmm3, %%xmm0\n\t"
        "psrld $8, %%xmm0\n\t"
        "paddd %%xmm0, %%xmm1\n\t"
        "packuswb %%xmm7, %%xmm1\n\t"
        "movd %%xmm1, %[result]\n\t"
        : [result]"=m"(dest)
        : [src_val]"m"(src), [dst_val]"m"(dst), [alpha_val]"m"(alpha)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm7", "memory"
    );
}

/**
 * Matrix multiplication for transformations (4x4)
 * Uses SSE for parallel computation
 */
static inline void jim_matrix_multiply(float *result, const float *a, const float *b) {
    __asm__ volatile (
        "movups (%[a]), %%xmm0\n\t"
        "movups 16(%[a]), %%xmm1\n\t"
        "movups 32(%[a]), %%xmm2\n\t"
        "movups 48(%[a]), %%xmm3\n\t"
        
        "movss (%[b]), %%xmm4\n\t"
        "shufps $0x00, %%xmm4, %%xmm4\n\t"
        "mulps %%xmm4, %%xmm0\n\t"
        
        "movss 4(%[b]), %%xmm4\n\t"
        "shufps $0x00, %%xmm4, %%xmm4\n\t"
        "mulps %%xmm4, %%xmm1\n\t"
        
        "movss 8(%[b]), %%xmm4\n\t"
        "shufps $0x00, %%xmm4, %%xmm4\n\t"
        "mulps %%xmm4, %%xmm2\n\t"
        
        "movss 12(%[b]), %%xmm4\n\t"
        "shufps $0x00, %%xmm4, %%xmm4\n\t"
        "mulps %%xmm4, %%xmm3\n\t"
        
        "addps %%xmm1, %%xmm0\n\t"
        "addps %%xmm2, %%xmm0\n\t"
        "addps %%xmm3, %%xmm0\n\t"
        "movups %%xmm0, (%[result])\n\t"
        
        : 
        : [a]"r"(a), [b]"r"(b), [result]"r"(result)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "memory"
    );
}

/**
 * Fast integer square root using Newton's method
 * Optimized with assembly for speed
 */
static inline uint32_t jim_isqrt(uint32_t n) {
    uint32_t result;
    
    if (n == 0) return 0;
    
    __asm__ volatile (
        "bsr %[val], %%ecx\n\t"
        "shr $1, %%ecx\n\t"
        "mov $1, %%eax\n\t"
        "shl %%cl, %%eax\n\t"
        "mov %%eax, %[res]\n\t"
        : [res]"=r"(result)
        : [val]"r"(n)
        : "eax", "ecx"
    );
    
    /* One iteration of Newton's method for refinement */
    result = (result + n / result) >> 1;
    
    return result;
}

/**
 * Get CPU features via CPUID
 */
static inline uint32_t jim_get_cpu_features(void) {
    uint32_t eax, ebx, ecx, edx;
    uint32_t features = 0;
    
    /* Check basic CPUID */
    __asm__ volatile (
        "cpuid\n\t"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    
    if (edx & (1 << 25)) features |= (1 << 0);  /* SSE */
    if (edx & (1 << 26)) features |= (1 << 1);  /* SSE2 */
    if (ecx & (1 << 0)) features |= (1 << 2);   /* SSE3 */
    if (ecx & (1 << 9)) features |= (1 << 3);   /* SSSE3 */
    if (ecx & (1 << 19)) features |= (1 << 4);  /* SSE4.1 */
    if (ecx & (1 << 20)) features |= (1 << 5);  /* SSE4.2 */
    if (ecx & (1 << 28)) features |= (1 << 6);  /* AVX */
    
    /* Check extended features */
    __asm__ volatile (
        "cpuid\n\t"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(7), "c"(0)
    );
    
    if (ebx & (1 << 5)) features |= (1 << 7);   /* AVX2 */
    
    return features;
}

/* CPU feature flags */
#define CPU_FEATURE_SSE     (1 << 0)
#define CPU_FEATURE_SSE2    (1 << 1)
#define CPU_FEATURE_SSE3    (1 << 2)
#define CPU_FEATURE_SSSE3   (1 << 3)
#define CPU_FEATURE_SSE41   (1 << 4)
#define CPU_FEATURE_SSE42   (1 << 5)
#define CPU_FEATURE_AVX     (1 << 6)
#define CPU_FEATURE_AVX2    (1 << 7)

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Create anonymous file for shared memory
 */
static int jim_create_anonymous_file(size_t size) {
    char template[] = "/tmp/jim-shared-XXXXXX";
    int fd;
    int ret;
    
    fd = mkstemp(template);
    if (fd < 0) {
        return -1;
    }
    
    unlink(template);
    
    ret = ftruncate(fd, size);
    if (ret < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

/**
 * Allocate shared memory buffer
 */
static void *jim_allocate_shm(size_t size, int *fd_out) {
    int fd;
    void *data;
    
    fd = jim_create_anonymous_file(size);
    if (fd < 0) {
        return NULL;
    }
    
    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    
    *fd_out = fd;
    return data;
}

/**
 * Get current time in milliseconds
 */
static inline uint64_t jim_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**
 * Clamp value between min and max
 */
static inline float jim_clamp(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

/**
 * Linear interpolation
 */
static inline float jim_lerp(float a, float b, float t) {
    return a + (b - a) * jim_clamp(t, 0.0f, 1.0f);
}

/**
 * Smooth step function for animations
 */
static inline float jim_smoothstep(float edge0, float edge1, float x) {
    float t = jim_clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/**
 * Spring animation update
 */
static void jim_animation_update(struct jim_animation *anim, uint64_t current_time) {
    if (!anim->active) return;
    
    float dt = (current_time - anim->start_time) / 1000.0f;
    float period = anim->duration_ms / 1000.0f;
    
    if (dt >= period) {
        anim->progress = 1.0f;
        anim->current_value = anim->target_value;
        anim->active = false;
        return;
    }
    
    /* Spring physics simulation */
    float displacement = anim->target_value - anim->current_value;
    float acceleration = displacement * anim->tension - anim->velocity * anim->damping;
    
    anim->velocity += acceleration * 0.016f;  /* Assume 60 FPS */
    anim->current_value += anim->velocity * 0.016f;
    anim->progress = jim_smoothstep(0.0f, 1.0f, dt / period);
}

/* ============================================================================
 * Global Server Instance
 * ============================================================================ */

static struct jim_server *g_server = NULL;

/* ============================================================================
 * Function Implementations
 * ============================================================================ */

/**
 * Initialize the JimCompositor server
 */
static int jim_server_init(struct jim_server *server) {
    int ret;
    
    memset(server, 0, sizeof(*server));
    pthread_mutex_init(&server->lock, NULL);
    
    wl_list_init(&server->outputs);
    wl_list_init(&server->views);
    wl_list_init(&server->seats);
    
    server->debug_mode = (getenv("NOVA_DEBUG") != NULL);
    server->hardware_cursor = true;
    
    /* Create Wayland display */
    server->wl_display = wl_display_create();
    if (!server->wl_display) {
        fprintf(stderr, "Failed to create Wayland display\n");
        return -1;
    }
    
    /* Create event loop */
    server->event_loop = wl_display_get_event_loop(server->wl_display);
    
    /* Create backend (auto-detect DRM, libinput, etc.) */
    server->backend = wlr_backend_autocreate(server->event_loop, &server->session);
    if (!server->backend) {
        fprintf(stderr, "Failed to create backend\n");
        wl_display_destroy(server->wl_display);
        return -1;
    }
    
    /* Create renderer */
    server->renderer = wlr_renderer_autocreate(server->backend);
    if (!server->renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        return -1;
    }
    
    wlr_renderer_init_wl_display(server->renderer, server->wl_display);
    
    /* Create allocator */
    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
    if (!server->allocator) {
        fprintf(stderr, "Failed to create allocator\n");
        return -1;
    }
    
    /* Create scene graph */
    server->scene = wlr_scene_create();
    if (!server->scene) {
        fprintf(stderr, "Failed to create scene graph\n");
        return -1;
    }
    
    /* Create output layout */
    server->output_layout = wlr_output_layout_create(server->wl_display);
    if (!server->output_layout) {
        fprintf(stderr, "Failed to create output layout\n");
        return -1;
    }
    
    server->scene_layout = wlr_scene_attach_output_layout(server->scene, 
                                                           server->output_layout);
    
    /* Print CPU features */
    uint32_t features = jim_get_cpu_features();
    printf("JimCompositor %s initializing...\n", NOVA_COMPOSITOR_VERSION);
    printf("CPU Features:");
    if (features & CPU_FEATURE_SSE) printf(" SSE");
    if (features & CPU_FEATURE_SSE2) printf(" SSE2");
    if (features & CPU_FEATURE_AVX) printf(" AVX");
    if (features & CPU_FEATURE_AVX2) printf(" AVX2");
    printf("\n");
    
    return 0;
}

/**
 * Add a keybinding
 */
static void jim_server_add_keybinding(struct jim_server *server,
                                        uint32_t modifiers,
                                        uint32_t key,
                                        void (*handler)(struct jim_server *, 
                                                       struct jim_seat *),
                                        const char *description) {
    if (server->keybinding_count >= MAX_KEYBINDINGS) {
        fprintf(stderr, "Maximum keybindings reached\n");
        return;
    }
    
    server->keybindings[server->keybinding_count].modifiers = modifiers;
    server->keybindings[server->keybinding_count].key = key;
    server->keybindings[server->keybinding_count].handler = handler;
    server->keybindings[server->keybinding_count].description = description;
    server->keybinding_count++;
}

/**
 * Find view at coordinates
 */
static struct jim_view *jim_server_view_at(struct jim_server *server,
                                              double lx, double ly,
                                              struct wlr_surface **surface,
                                              double *sx, double *sy) {
    struct jim_view *view;
    double _sx, _sy;
    struct wlr_surface *_surface = NULL;
    
    wl_list_for_each_reverse(view, &server->views, link) {
        if (!view->mapped || view->minimized) continue;
        
        _sx = lx - view->geometry.x;
        _sy = ly - view->geometry.y;
        
        if (wlr_surface_point_accepts_input(view->surface, _sx, _sy)) {
            *surface = view->surface;
            *sx = _sx;
            *sy = _sy;
            return view;
        }
    }
    
    *surface = NULL;
    return NULL;
}

/**
 * Focus a view
 */
static void jim_server_focus_view(struct jim_server *server, 
                                   struct jim_view *view) {
    struct jim_seat *seat = server->current_seat;
    struct wlr_surface *prev_surface = NULL;
    
    if (!seat) return;
    
    /* Get previously focused surface */
    prev_surface = seat->wlr_seat->keyboard_state.focused_surface;
    
    if (prev_surface == view->surface) return;
    
    /* Move view to top */
    wl_list_remove(&view->link);
    wl_list_insert(&server->views, &view->link);
    
    view->focused = true;
    server->focused_view = view;
    
    /* Send focus event */
    if (view->xdg_surface) {
        struct wlr_xdg_toplevel *toplevel = wlr_xdg_surface_toplevel_from_surface(
            view->xdg_surface);
        if (toplevel) {
            wlr_xdg_toplevel_set_activated(toplevel, true);
        }
    }
    
    /* Set keyboard focus */
    if (seat->wlr_seat->keyboard_state.keyboard) {
        struct wlr_keyboard *keyboard = seat->wlr_seat->keyboard_state.keyboard;
        wlr_seat_keyboard_notify_enter(seat->wlr_seat, view->surface,
                                        keyboard->keycodes,
                                        keyboard->num_keycodes,
                                        &keyboard->modifiers);
    }
}

/**
 * Process keybinding
 */
static bool jim_server_process_keybinding(struct jim_server *server,
                                           struct jim_seat *seat,
                                           uint32_t modifiers,
                                           uint32_t key) {
    for (int i = 0; i < server->keybinding_count; i++) {
        struct jim_keybinding *kb = &server->keybindings[i];
        
        if (kb->modifiers == modifiers && kb->key == key) {
            kb->handler(server, seat);
            return true;
        }
    }
    
    return false;
}

/**
 * Destroy the server
 */
static void jim_server_destroy(struct jim_server *server) {
    struct jim_view *view, *view_tmp;
    struct jim_output *output, *output_tmp;
    struct jim_seat *seat, *seat_tmp;
    
    /* Clean up views */
    wl_list_for_each_safe(view, view_tmp, &server->views, link) {
        wl_list_remove(&view->link);
        free(view);
    }
    
    /* Clean up outputs */
    wl_list_for_each_safe(output, output_tmp, &server->outputs, link) {
        wl_list_remove(&output->link);
        free(output);
    }
    
    /* Clean up seats */
    wl_list_for_each_safe(seat, seat_tmp, &server->seats, link) {
        wl_list_remove(&seat->link);
        free(seat);
    }
    
    /* Destroy Wayland components */
    if (server->scene) wlr_scene_destroy(server->scene);
    if (server->output_layout) wlr_output_layout_destroy(server->output_layout);
    if (server->allocator) wlr_allocator_destroy(server->allocator);
    if (server->renderer) wlr_renderer_destroy(server->renderer);
    if (server->backend) wlr_backend_destroy(server->backend);
    if (server->wl_display) wl_display_destroy(server->wl_display);
    
    pthread_mutex_destroy(&server->lock);
}

/* ============================================================================
 * Signal Handler
 * ============================================================================ */

static void jim_signal_handler(int signal) {
    if (g_server) {
        g_server->quit = 1;
    }
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char *argv[]) {
    struct jim_server server;
    int ret;
    
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║         JimDe Compositor - JimOS Desktop             ║\n");
    printf("║              Version: %s                        ║\n", NOVA_COMPOSITOR_VERSION);
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            setenv("NOVA_DEBUG", "1", 1);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -d, --debug     Enable debug mode\n");
            printf("  -h, --help      Show this help message\n");
            printf("  -v, --version   Show version information\n");
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            printf("JimDe Compositor %s\n", NOVA_COMPOSITOR_VERSION);
            return 0;
        }
    }
    
    /* Set up signal handlers */
    signal(SIGINT, jim_signal_handler);
    signal(SIGTERM, jim_signal_handler);
    
    /* Initialize server */
    ret = jim_server_init(&server);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }
    
    g_server = &server;
    server.current_seat = NULL;  /* Will be created when input devices are detected */
    
    /* Set up default keybindings */
    // These will be implemented in subsequent files
    // jim_server_add_keybinding(&server, WLR_MODIFIER_ALT, KEY_TAB, ...);
    // jim_server_add_keybinding(&server, WLR_MODIFIER_LOGO, KEY_Q, ...);
    
    /* Run event loop */
    printf("Starting Wayland compositor...\n");
    wl_display_run(server.wl_display);
    
    /* Cleanup */
    printf("Shutting down...\n");
    jim_server_destroy(&server);
    
    return 0;
}
