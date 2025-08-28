#include "input_local.h"
#include "asyncinput.h"
#include <atomic>

static std::atomic<bool> g_should_quit{false};
static std::atomic<int>  g_yaw_dir{0};     // -1,0,1
static std::atomic<int>  g_move_dir{0};     // -1,0,1
static std::atomic<bool> g_jump_down{false};
static std::atomic<bool> g_prev_jump{false};
static std::atomic<bool> g_jump_edge{false};
static std::atomic<bool> g_left_down{false};
static std::atomic<bool> g_right_down{false};
static std::atomic<bool> g_top_down{false};
static std::atomic<bool> g_bottom_down{false};

static void on_input(const struct ni_event* ev, void* ud) {
    (void)ud;
    if (ev->type == NI_EV_KEY) {
        bool down = (ev->value != 0);
        if (ev->code == NI_KEY_LEFT || ev->code == NI_KEY_A) {
            g_left_down.store(down, std::memory_order_relaxed);
        } else if (ev->code == NI_KEY_RIGHT || ev->code == NI_KEY_D) {
            g_right_down.store(down, std::memory_order_relaxed);
        } else if (ev->code == NI_KEY_SPACE) {
            g_jump_down.store(down, std::memory_order_relaxed);
        } else if (ev->code == NI_KEY_W || ev->code == NI_KEY_UP) {
            g_top_down.store(down, std::memory_order_relaxed);
        } else if (ev->code == NI_KEY_S || ev->code == NI_KEY_DOWN) {
            g_bottom_down.store(down, std::memory_order_relaxed);
        }
        if (down && (ev->code == NI_KEY_ESC || ev->code == NI_KEY_Q)) {
            g_should_quit.store(true, std::memory_order_relaxed);
        }
        int ad = (g_right_down.load(std::memory_order_relaxed) ? 1 : 0) - (g_left_down.load(std::memory_order_relaxed) ? 1 : 0);
        g_yaw_dir.store(ad, std::memory_order_relaxed);
        int ws = (g_top_down.load(std::memory_order_relaxed) ? 1 : 0) - (g_bottom_down.load(std::memory_order_relaxed) ? 1 : 0);
        g_move_dir.store(ws, std::memory_order_relaxed);
    }
}

bool input_init(void) {
    ni_enable_mice(0);
    if (ni_init(0) != 0) {
        g_should_quit.store(true, std::memory_order_relaxed); // immediate exit path if no input
        return false;
    }
    (void)ni_register_callback(on_input, NULL, 0);
    return true;
}

void input_shutdown(void) {
    ni_shutdown();
}

void input_begin_frame(void) {
    bool jd = g_jump_down.load(std::memory_order_relaxed);
    bool prev = g_prev_jump.load(std::memory_order_relaxed);
    g_jump_edge.store((jd && !prev), std::memory_order_relaxed);
    g_prev_jump.store(jd, std::memory_order_relaxed);
}

bool input_should_quit(void) { return g_should_quit.load(std::memory_order_relaxed); }
int  input_move_dir(void)    { return g_move_dir.load(std::memory_order_relaxed); }
int  input_yaw_dir(void)    { return g_yaw_dir.load(std::memory_order_relaxed); }
bool input_jump_edge(void)   { return g_jump_edge.load(std::memory_order_relaxed); }

