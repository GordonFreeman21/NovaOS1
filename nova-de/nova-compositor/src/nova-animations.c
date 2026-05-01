/*
 * Nova Animations
 * 
 * Smooth animation system for NovaDe compositor
 * Using spring physics and bezier curves
 * 
 * Copyright (C) 2024 NovaOS Project
 * Licensed under GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "nova-compositor.h"

#define ANIMATION_FPS 60
#define DT (1.0f / 60.0f)

/**
 * Cubic bezier interpolation
 * B(t) = (1-t)³·P0 + 3(1-t)²·t·P1 + 3(1-t)·t²·P2 + t³·P3
 */
static float cubic_bezier(float t, float p1x, float p1y, float p2x, float p2y) {
    float mt = 1.0f - t;
    float mt2 = mt * mt;
    float mt3 = mt2 * mt;
    float t2 = t * t;
    float t3 = t2 * t;
    
    /* Y coordinate of bezier curve */
    return 3.0f * mt2 * t * p1y + 3.0f * mt * t2 * p2y + t3;
}

/**
 * Spring animation update
 * F = -kx - cv (Hooke's law with damping)
 */
void nova_animation_spring_update(float *current, float target, 
                                   float *velocity, float tension, 
                                   float damping, float dt) {
    float displacement = target - *current;
    float acceleration = displacement * tension - (*velocity) * damping;
    
    *velocity += acceleration * dt;
    *current += (*velocity) * dt;
}

/**
 * Ease-in-out quadratic
 */
float nova_ease_in_out_quad(float t) {
    t = fmaxf(0.0f, fminf(1.0f, t));
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

/**
 * Ease-in-out cubic
 */
float nova_ease_in_out_cubic(float t) {
    t = fmaxf(0.0f, fminf(1.0f, t));
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

/**
 * Ease-out elastic (spring-like bounce)
 */
float nova_ease_out_elastic(float t) {
    t = fmaxf(0.0f, fminf(1.0f, t));
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    
    float c4 = (2.0f * M_PI) / 3.0f;
    return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
}

/**
 * Animate rectangle with easing
 */
void nova_animate_rect(nova_rect_t *rect, const nova_rect_t *target, 
                       float progress, nova_animation_type_t type) {
    if (!rect || !target) return;
    
    float eased_progress;
    
    switch (type) {
        case ANIMATION_OPEN:
            eased_progress = nova_ease_out_elastic(progress);
            break;
        case ANIMATION_CLOSE:
            eased_progress = nova_ease_in_out_cubic(progress);
            break;
        case ANIMATION_MINIMIZE:
            eased_progress = nova_ease_in_out_quad(progress);
            break;
        default:
            eased_progress = nova_ease_in_out_cubic(progress);
            break;
    }
    
    rect->x = (int32_t)((float)rect->x + ((float)target->x - (float)rect->x) * eased_progress);
    rect->y = (int32_t)((float)rect->y + ((float)target->y - (float)rect->y) * eased_progress);
    rect->width = (int32_t)((float)rect->width + ((float)target->width - (float)rect->width) * eased_progress);
    rect->height = (int32_t)((float)rect->height + ((float)target->height - (float)rect->height) * eased_progress);
}

/**
 * Initialize animation system
 */
int nova_animations_init(void) {
    printf("Animation system initialized (%d FPS)\n", ANIMATION_FPS);
    return 0;
}

/**
 * Cleanup animation system
 */
void nova_animations_fini(void) {
    printf("Animation system cleaned up\n");
}
