#include "input.h"
#include <stdatomic.h>
#include <string.h>
#include "asyncinput.h"

static _Atomic int a_left = 0, a_right = 0, a_a = 0, a_d = 0;
static _Atomic int a_w = 0, a_s = 0;
static _Atomic int a_q = 0, a_e = 0, a_j = 0, a_l = 0;
static _Atomic int a_space = 0;
static _Atomic int a_enter = 0;
static _Atomic int a_shift = 0;
static _Atomic int a_switch_e = 0, a_switch_ins = 0;
static _Atomic int a_escape = 0;
static _Atomic int a_num[10];  // 0..9
static _Atomic int a_g = 0, a_m = 0, a_t = 0, a_r = 0;

static _Atomic int edge_jump = 0;
static _Atomic int edge_switch = 0;
static _Atomic int edge_advance = 0;
static _Atomic int edge_num[10];
static _Atomic int prev_space = 0;
static _Atomic int prev_enter = 0;
static _Atomic int prev_num[10];
static _Atomic int prev_e = 0, prev_ins = 0;
static _Atomic int edge_g = 0, edge_m = 0, edge_t = 0, edge_r = 0;
static _Atomic int prev_g = 0, prev_m = 0, prev_t = 0, prev_r = 0;

static void on_input(const struct ni_event* ev, void* ud) {
    (void)ud;
    if (ev->type != NI_EV_KEY)
        return;
    int down = ev->value != 0;
    switch (ev->code) {
        case NI_KEY_LEFT:
            atomic_store(&a_a, down);
            break;
        case NI_KEY_RIGHT:
            atomic_store(&a_d, down);
            break;
        case NI_KEY_UP:
            atomic_store(&a_w, down);
            break;
        case NI_KEY_DOWN:
            atomic_store(&a_s, down);
            break;
        case NI_KEY_A:
            atomic_store(&a_a, down);
            break;
        case NI_KEY_D:
            atomic_store(&a_d, down);
            break;
        case NI_KEY_W:
            atomic_store(&a_w, down);
            break;
        case NI_KEY_S:
            atomic_store(&a_s, down);
            break;
        case NI_KEY_Q:
            atomic_store(&a_q, down);
            break;
        case NI_KEY_E:
            atomic_store(&a_e, down);
            break;
        case NI_KEY_J:
            atomic_store(&a_j, down);
            break;
        case NI_KEY_L:
            atomic_store(&a_l, down);
            break;
        case NI_KEY_SPACE:
            atomic_store(&a_space, down);
            break;
        case NI_KEY_ENTER:
            atomic_store(&a_enter, down);
            break;
        case NI_KEY_G:
            atomic_store(&a_g, down);
            break;
        case NI_KEY_M:
            atomic_store(&a_m, down);
            break;
        case NI_KEY_T:
            atomic_store(&a_t, down);
            break;
        case NI_KEY_R:
            atomic_store(&a_r, down);
            break;
        case NI_KEY_0:
            atomic_store(&a_num[0], down);
            break;
        case NI_KEY_1:
            atomic_store(&a_num[1], down);
            break;
        case NI_KEY_2:
            atomic_store(&a_num[2], down);
            break;
        case NI_KEY_3:
            atomic_store(&a_num[3], down);
            break;
        case NI_KEY_4:
            atomic_store(&a_num[4], down);
            break;
        case NI_KEY_5:
            atomic_store(&a_num[5], down);
            break;
        case NI_KEY_6:
            atomic_store(&a_num[6], down);
            break;
        case NI_KEY_7:
            atomic_store(&a_num[7], down);
            break;
        case NI_KEY_8:
            atomic_store(&a_num[8], down);
            break;
        case NI_KEY_9:
            atomic_store(&a_num[9], down);
            break;
        case NI_KEY_LEFTSHIFT:
        case NI_KEY_RIGHTSHIFT:
            atomic_store(&a_shift, down);
            break;
        case NI_KEY_INSERT:
            atomic_store(&a_switch_ins, down);
            break;
        case NI_KEY_ESC:
            atomic_store(&a_escape, down);
            break;
        default:
            break;
    }
}

bool input_init(void) {
    if (ni_init(0) != 0)
        return false;
    if (ni_register_callback(on_input, NULL, 0) != 0)
        return false;
    return true;
}

void input_shutdown(void) {
    ni_shutdown();
}

void input_update(void) {
    int sp = atomic_load(&a_space);
    int psp = atomic_exchange(&prev_space, sp);
    if (sp && !psp)
        atomic_store(&edge_jump, 1);

    int ent = atomic_load(&a_enter);
    int pent = atomic_exchange(&prev_enter, ent);
    if ((sp && !psp) || (ent && !pent))
        atomic_store(&edge_advance, 1);

    for (int i = 0; i < 10; i++) {
        int v = atomic_load(&a_num[i]);
        int pv = atomic_exchange(&prev_num[i], v);
        if (v && !pv)
            atomic_store(&edge_num[i], 1);
    }

    int ee = atomic_load(&a_e);
    int pei = atomic_exchange(&prev_e, ee);
    if (ee && !pei)
        atomic_store(&edge_switch, 1);

    int ins = atomic_load(&a_switch_ins);
    int pins = atomic_exchange(&prev_ins, ins);
    if (ins && !pins)
        atomic_store(&edge_switch, 1);

    int g = atomic_load(&a_g); int pg = atomic_exchange(&prev_g, g); if (g && !pg) atomic_store(&edge_g, 1);
    int m = atomic_load(&a_m); int pm = atomic_exchange(&prev_m, m); if (m && !pm) atomic_store(&edge_m, 1);
    int t = atomic_load(&a_t); int pt = atomic_exchange(&prev_t, t); if (t && !pt) atomic_store(&edge_t, 1);
    int r = atomic_load(&a_r); int pr = atomic_exchange(&prev_r, r); if (r && !pr) atomic_store(&edge_r, 1);
}

// Left/Right for human walking
int input_move_dir(void) {
    int r = (atomic_load(&a_right) || atomic_load(&a_d)) ? 1 : 0;
    int l = (atomic_load(&a_left) || atomic_load(&a_a)) ? 1 : 0;
    return r - l;
}

// W/S for car acceleration (S = -1, W = +1)
int input_accel_dir(void) {
    int up = atomic_load(&a_w) ? 1 : 0;
    int dn = atomic_load(&a_s) ? 1 : 0;
    return up - dn;
}

// A/D for car yaw (A = -1, D = +1)
int input_yaw_dir(void) {
    int rr = atomic_load(&a_d) ? 1 : 0;
    int ll = atomic_load(&a_a) ? 1 : 0;
    return rr - ll;
}

bool input_jump_edge(void) {
    return atomic_exchange(&edge_jump, 0) != 0;
}
bool input_jump_down(void) {
    return atomic_load(&a_space) != 0;
}
bool input_boost_down(void) {
    return atomic_load(&a_shift) != 0;
}
bool input_pressed_switch(void) {
    return atomic_exchange(&edge_switch, 0) != 0;
}
bool input_quit_requested(void) {
    return atomic_load(&a_escape) != 0;
}
bool input_advance_dialogue_edge(void) {
    return atomic_exchange(&edge_advance, 0) != 0;
}
bool input_choice_edge(int index_1_to_9) {
    if (index_1_to_9 < 1 || index_1_to_9 > 9)
        return false;
    return atomic_exchange(&edge_num[index_1_to_9], 0) != 0;
}

