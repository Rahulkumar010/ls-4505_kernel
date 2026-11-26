/*
 * libbadwolf_js_on.c ? Tiny LD_PRELOAD helper for BadWolf on LS-4505
 * Defaults the per-tab [JS] toggle button to ON (active) when each tab is
 * created while keeping the toolbar [JS] button 100% clickable to disable JS.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

static void *last_js_btn = NULL;

void *gtk_toggle_button_new_with_mnemonic(const char *label) {
    static void *(*real_fn)(const char *) = NULL;
    if (!real_fn)
        real_fn = (void *(*)(const char *))dlsym(RTLD_NEXT, "gtk_toggle_button_new_with_mnemonic");
    void *btn = real_fn(label);
    if (btn && label && strstr(label, "JS"))
        last_js_btn = btn;
    return btn;
}

unsigned long g_signal_connect_data(void *instance, const char *detailed_signal,
                                    void *c_handler, void *data,
                                    void *destroy_data, int connect_flags) {
    static unsigned long (*real_fn)(void *, const char *, void *, void *, void *, int) = NULL;
    static void (*set_active)(void *, int) = NULL;
    if (!real_fn)
        real_fn = (unsigned long (*)(void *, const char *, void *, void *, void *, int))
                  dlsym(RTLD_NEXT, "g_signal_connect_data");
    if (!set_active)
        set_active = (void (*)(void *, int))dlsym(RTLD_NEXT, "gtk_toggle_button_set_active");

    unsigned long id = real_fn(instance, detailed_signal, c_handler, data, destroy_data, connect_flags);
    if (instance && instance == last_js_btn && detailed_signal && strcmp(detailed_signal, "toggled") == 0) {
        last_js_btn = NULL;
        if (set_active)
            set_active(instance, 1);
    }
    return id;
}
