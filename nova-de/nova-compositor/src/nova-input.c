/*
 * Nova Input Handling
 * 
 * Keyboard, pointer, and touch input handling for NovaDe compositor
 * 
 * Copyright (C) 2024 NovaOS Project
 * Licensed under GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <linux/input.h>

#include "nova-compositor.h"

/* Internal structure definitions */
struct nova_server {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_scene *scene;
    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_list views;
    struct wl_list seats;
    struct nova_view *focused_view;
    struct nova_seat *current_seat;
    void *wm;  /* Window manager */
    volatile sig_atomic_t quit;
};

struct nova_seat {
    struct wl_list link;
    struct nova_server *server;
    struct wlr_seat *wlr_seat;
    char name[32];
    struct wl_list keyboards;
    struct wl_list pointers;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *xcursor_manager;
    double cursor_x, cursor_y;
    uint32_t buttons;
};

struct nova_view {
    struct wl_list link;
    struct wlr_surface *surface;
    void *xdg_surface;
    nova_rect_t geometry;
    bool mapped;
    bool minimized;
    int workspace_id;
    char title[256];
};

/* Global state */
static struct xkb_context *xkb_context = NULL;
static struct xkb_keymap *xkb_keymap = NULL;

/**
 * Initialize XKB context
 */
static int nova_input_init_xkb(void) {
    xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!xkb_context) {
        fprintf(stderr, "Failed to create XKB context\n");
        return -1;
    }
    
    xkb_keymap = xkb_keymap_new_from_names(xkb_context, NULL,
                                            XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!xkb_keymap) {
        fprintf(stderr, "Failed to create XKB keymap\n");
        xkb_context_unref(xkb_context);
        return -1;
    }
    
    return 0;
}

/**
 * Cleanup XKB resources
 */
static void nova_input_cleanup_xkb(void) {
    if (xkb_keymap) {
        xkb_keymap_unref(xkb_keymap);
        xkb_keymap = NULL;
    }
    if (xkb_context) {
        xkb_context_unref(xkb_context);
        xkb_context = NULL;
    }
}

/**
 * Handle keyboard key event
 */
static void nova_input_handle_key(struct nova_seat *seat,
                                   struct wlr_keyboard *keyboard,
                                   uint32_t time, uint32_t key,
                                   uint32_t state) {
    struct nova_server *server = seat->server;
    
    /* Notify seat of key event */
    wlr_seat_set_keyboard(seat->wlr_seat, keyboard);
    wlr_seat_keyboard_notify_key(seat->wlr_seat, time, key, state);
    
    /* Get modifier state */
    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
    
    /* Check for compositor keybindings */
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        /* Example: Alt+Tab for window switching */
        if (modifiers & WLR_MODIFIER_ALT && key == KEY_TAB) {
            /* TODO: Implement window switcher */
            printf("Alt+Tab detected\n");
            return;
        }
        
        /* Example: Super+Q to close window */
        if (modifiers & WLR_MODIFIER_LOGO && key == KEY_Q) {
            if (server->focused_view && server->focused_view->xdg_surface) {
                /* TODO: Close focused window */
                printf("Close window requested\n");
            }
            return;
        }
        
        /* Example: Super+Enter for terminal */
        if (modifiers & WLR_MODIFIER_LOGO && key == KEY_ENTER) {
            /* TODO: Launch terminal */
            printf("Launch terminal\n");
            return;
        }
        
        /* Example: Super+[1-9] for workspace switching */
        if (modifiers & WLR_MODIFIER_LOGO && key >= KEY_1 && key <= KEY_9) {
            int ws = key - KEY_1;
            printf("Switch to workspace %d\n", ws);
            /* TODO: nova_wm_switch_workspace(server->wm, ws); */
            return;
        }
    }
}

/**
 * Keyboard modifiers event handler
 */
static void handle_keyboard_modifiers(struct wl_listener *listener, void *data) {
    struct nova_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    
    wlr_seat_set_keyboard(keyboard->seat->wlr_seat, keyboard->device->keyboard);
    wlr_seat_keyboard_notify_modifiers(keyboard->seat->wlr_seat,
                                        &keyboard->device->keyboard->modifiers);
}

/**
 * Keyboard key event handler
 */
static void handle_keyboard_key(struct wl_listener *listener, void *data) {
    struct nova_keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event *event = data;
    
    nova_input_handle_key(keyboard->seat, keyboard->device->keyboard,
                          event->time_msec, event->keycode, event->state);
}

/**
 * Create keyboard device
 */
static void nova_input_create_keyboard(struct nova_seat *seat,
                                        struct wlr_input_device *device) {
    struct nova_keyboard *keyboard = calloc(1, sizeof(*keyboard));
    if (!keyboard) {
        fprintf(stderr, "Failed to allocate keyboard\n");
        return;
    }
    
    keyboard->seat = seat;
    keyboard->device = device;
    
    /* Set up listeners */
    keyboard->modifiers.notify = handle_keyboard_modifiers;
    keyboard->key.notify = handle_keyboard_key;
    
    wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
    wl_signal_add(&device->keyboard->events.key, &keyboard->key);
    
    wl_list_insert(&seat->keyboards, &keyboard->link);
    
    /* Set keyboard capabilities */
    wlr_seat_set_keyboard(seat->wlr_seat, device->keyboard);
    
    printf("Keyboard created: %s\n", device->name);
}

/**
 * Pointer motion event handler
 */
static void handle_pointer_motion(struct wl_listener *listener, void *data) {
    struct nova_pointer *pointer = wl_container_of(listener, pointer, motion);
    struct wlr_pointer_motion_event *event = data;
    
    /* Move cursor */
    wlr_cursor_move(pointer->seat->cursor, &pointer->device->pointer,
                    event->delta_x, event->delta_y);
    
    /* Update cursor coordinates */
    pointer->seat->cursor_x = pointer->seat->cursor->x;
    pointer->seat->cursor_y = pointer->seat->cursor->y;
    
    /* Process cursor movement */
    /* TODO: Process cursor interaction with views */
}

/**
 * Pointer motion absolute event handler
 */
static void handle_pointer_motion_absolute(struct wl_listener *listener, void *data) {
    struct nova_pointer *pointer = wl_container_of(listener, pointer, motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    
    wlr_cursor_warp_absolute(pointer->seat->cursor, &pointer->device->pointer,
                             event->x, event->y);
    
    pointer->seat->cursor_x = pointer->seat->cursor->x;
    pointer->seat->cursor_y = pointer->seat->cursor->y;
}

/**
 * Pointer button event handler
 */
static void handle_pointer_button(struct wl_listener *listener, void *data) {
    struct nova_pointer *pointer = wl_container_of(listener, pointer, button);
    struct wlr_pointer_button_event *event = data;
    
    /* Notify seat */
    wlr_seat_pointer_notify_button(pointer->seat->wlr_seat,
                                    event->time_msec, event->button,
                                    event->state);
    
    /* Track button state */
    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        pointer->seat->buttons |= (1 << event->button);
    } else {
        pointer->seat->buttons &= ~(1 << event->button);
    }
    
    /* Handle click on window */
    /* TODO: Implement focus on click */
}

/**
 * Pointer axis event handler (scrolling)
 */
static void handle_pointer_axis(struct wl_listener *listener, void *data) {
    struct nova_pointer *pointer = wl_container_of(listener, pointer, axis);
    struct wlr_pointer_axis_event *event = data;
    
    /* Notify seat of scroll */
    wlr_seat_pointer_notify_axis(pointer->seat->wlr_seat,
                                  event->time_msec, event->orientation,
                                  event->delta, event->delta_discrete,
                                  event->source, event->relative_direction);
}

/**
 * Create pointer device
 */
static void nova_input_create_pointer(struct nova_seat *seat,
                                       struct wlr_input_device *device) {
    struct nova_pointer *pointer = calloc(1, sizeof(*pointer));
    if (!pointer) {
        fprintf(stderr, "Failed to allocate pointer\n");
        return;
    }
    
    pointer->seat = seat;
    pointer->device = device;
    
    /* Set up listeners */
    pointer->motion.notify = handle_pointer_motion;
    pointer->motion_absolute.notify = handle_pointer_motion_absolute;
    pointer->button.notify = handle_pointer_button;
    pointer->axis.notify = handle_pointer_axis;
    
    wl_signal_add(&device->pointer->events.motion, &pointer->motion);
    wl_signal_add(&device->pointer->events.motion_absolute, &pointer->motion_absolute);
    wl_signal_add(&device->pointer->events.button, &pointer->button);
    wl_signal_add(&device->pointer->events.axis, &pointer->axis);
    
    wl_list_insert(&seat->pointers, &pointer->link);
    
    printf("Pointer created: %s\n", device->name);
}

/**
 * Create seat for input devices
 */
struct nova_seat *nova_input_create_seat(struct nova_server *server, const char *name) {
    struct nova_seat *seat = calloc(1, sizeof(*seat));
    if (!seat) {
        fprintf(stderr, "Failed to allocate seat\n");
        return NULL;
    }
    
    seat->server = server;
    strncpy(seat->name, name ? name : "seat0", sizeof(seat->name) - 1);
    seat->name[sizeof(seat->name) - 1] = '\0';
    
    wl_list_init(&seat->keyboards);
    wl_list_init(&seat->pointers);
    
    /* Create Wayland seat */
    seat->wlr_seat = wlr_seat_create(server->wl_display, seat->name);
    if (!seat->wlr_seat) {
        fprintf(stderr, "Failed to create wlr_seat\n");
        free(seat);
        return NULL;
    }
    
    /* Create cursor */
    seat->cursor = wlr_cursor_create();
    if (!seat->cursor) {
        fprintf(stderr, "Failed to create cursor\n");
        wlr_seat_destroy(seat->wlr_seat);
        free(seat);
        return NULL;
    }
    
    wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);
    
    /* Create cursor manager */
    seat->xcursor_manager = wlr_xcursor_manager_create(NULL, 24);
    if (!seat->xcursor_manager) {
        fprintf(stderr, "Failed to create xcursor manager\n");
        wlr_cursor_destroy(seat->cursor);
        wlr_seat_destroy(seat->wlr_seat);
        free(seat);
        return NULL;
    }
    
    /* Set up seat capabilities */
    wlr_seat_set_capabilities(seat->wlr_seat,
                              WL_SEAT_CAPABILITY_KEYBOARD |
                              WL_SEAT_CAPABILITY_POINTER);
    
    wl_list_insert(&server->seats, &seat->link);
    server->current_seat = seat;
    
    printf("Seat created: %s\n", seat->name);
    
    return seat;
}

/**
 * Handle new input device
 */
static void handle_new_input(struct wl_listener *listener, void *data) {
    struct nova_server *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;
    
    struct nova_seat *seat = server->current_seat;
    if (!seat) {
        seat = nova_input_create_seat(server, "seat0");
        if (!seat) return;
    }
    
    switch (device->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            nova_input_create_keyboard(seat, device);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            nova_input_create_pointer(seat, device);
            break;
        default:
            printf("Unhandled input device type: %d\n", device->type);
            break;
    }
}

/**
 * Initialize input subsystem
 */
int nova_input_init(struct nova_server *server) {
    if (nova_input_init_xkb() != 0) {
        return -1;
    }
    
    wl_list_init(&server->seats);
    
    /* Set up input listener */
    server->new_input.notify = handle_new_input;
    wl_signal_add(&server->backend->events.new_input, &server->new_input);
    
    printf("Input subsystem initialized\n");
    
    return 0;
}

/**
 * Cleanup input subsystem
 */
void nova_input_fini(struct nova_server *server) {
    struct nova_seat *seat, *tmp_seat;
    struct nova_keyboard *keyboard, *tmp_kb;
    struct nova_pointer *pointer, *tmp_ptr;
    
    wl_list_for_each_safe(seat, tmp_seat, &server->seats, link) {
        wl_list_for_each_safe(keyboard, tmp_kb, &seat->keyboards, link) {
            wl_list_remove(&keyboard->modifiers.link);
            wl_list_remove(&keyboard->key.link);
            wl_list_remove(&keyboard->link);
            free(keyboard);
        }
        
        wl_list_for_each_safe(pointer, tmp_ptr, &seat->pointers, link) {
            wl_list_remove(&pointer->motion.link);
            wl_list_remove(&pointer->motion_absolute.link);
            wl_list_remove(&pointer->button.link);
            wl_list_remove(&pointer->axis.link);
            wl_list_remove(&pointer->link);
            free(pointer);
        }
        
        if (seat->xcursor_manager) {
            wlr_xcursor_manager_destroy(seat->xcursor_manager);
        }
        if (seat->cursor) {
            wlr_cursor_destroy(seat->cursor);
        }
        if (seat->wlr_seat) {
            wlr_seat_destroy(seat->wlr_seat);
        }
        
        wl_list_remove(&seat->link);
        free(seat);
    }
    
    nova_input_cleanup_xkb();
    
    printf("Input subsystem cleaned up\n");
}

/**
 * Warp cursor to position
 */
void nova_server_warp_cursor(struct nova_server *server, double lx, double ly) {
    if (!server || !server->current_seat || !server->current_seat->cursor) {
        return;
    }
    
    wlr_cursor_warp_closest(server->current_seat->cursor, NULL, lx, ly);
    server->current_seat->cursor_x = lx;
    server->current_seat->cursor_y = ly;
}

/**
 * Get current seat
 */
struct nova_seat *nova_server_get_current_seat(struct nova_server *server) {
    return server ? server->current_seat : NULL;
}
