#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h> // Essential for keymap decoding
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <errno.h>
#include "xdg-shell-client-protocol.h"

struct client_state {
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_compositor *wl_compositor;
    struct wl_seat *wl_seat;
    struct wl_keyboard *wl_keyboard;
    struct wl_surface *wl_surface;
    struct wl_shm *wl_shm; // Shared Memory interface

    // XDG Shell components for creating a window
    struct xdg_wm_base *xdg_wm_base;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    
    // XKB State
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;
};

// Forward declarations for listeners from previous responses
static const struct wl_keyboard_listener wl_keyboard_listener;
static const struct wl_registry_listener wl_registry_listener;

// Mandatory handler called when the surface gains keyboard focus.
static void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
                              uint32_t serial, struct wl_surface *surface,
                              struct wl_array *keys)
{
    // A real client would store the serial and the surface, 
    // but for this minimal client, we can just print a message.
    printf("--- Keyboard Focus Gained (Enter) ---\n");
}
// Mandatory handler called when the surface loses keyboard focus.
static void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
                              uint32_t serial, struct wl_surface *surface)
{
    // Print a message to confirm the event was successfully handled.
    printf("--- Keyboard Focus Lost (Leave) ---\n");
}
// Helper to create an anonymous file descriptor for shared memory
static int create_anon_file(off_t size) {
    char name[] = "/wayland-shm-XXXXXX";
    int fd = -1;
    
    // Use shm_open and shm_unlink for better security and portability
    fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) return -1;
    
    shm_unlink(name);
    
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// Function to create and attach the wl_buffer
static struct wl_buffer *create_buffer(struct client_state *state, int width, int height) {
    uint32_t stride = width * 4; // 4 bytes per pixel (BGRA)
    uint32_t size = stride * height;
    
    int fd = create_anon_file(size);
    if (fd < 0) {
        fprintf(stderr, "Failed to create shared memory file.\n");
        return NULL;
    }
    
    // Map the memory (we don't write to it, but it needs to be mapped)
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap shared memory.\n");
        close(fd);
        return NULL;
    }
    // We will use 0xFF999999 (Opaque Alpha + Light Gray RGB)
    uint32_t *pixel = data;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Write a visible gray color (0xFF is opaque alpha)
            pixel[y * width + x] = 0xFF999999; 
        }
    }
    
    // Create the wl_shm_pool
    struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
    
    // Create the wl_buffer from the pool (using ARGB8888, a standard format)
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    
    // Clean up temporary resources
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
    
    return buffer;
}

// --- XDG Surface Listener ---
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    // Acknowledge the configuration immediately
    xdg_surface_ack_configure(xdg_surface, serial);
    
    // Once configured, commit the surface to make it active/visible.
    // NOTE: This client still lacks a buffer, so the window will be invisible,
    // but the compositor now recognizes it as a focusable window.
    wl_surface_commit(((struct client_state *)data)->wl_surface);
    printf("XDG Surface configured and committed. Try clicking the window area.\n");
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

// --- XDG Toplevel Listener ---
static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
    // Window configuration updates (size, maximization, etc.)
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    // This is where you would normally set a flag to exit the main loop.
    printf("Window requested close. Exiting...\n");
    exit(EXIT_SUCCESS);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

// --- XDG WM Base Listener ---
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    // Required to keep the connection alive
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

// --- Keyboard Listener (Simplified Echo) ---

// 1. Keymap (Same as before)
static void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
                               uint32_t format, int32_t fd, uint32_t size) {
    struct client_state *state = data;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }
    // ... (rest of mmap/xkb_keymap_new_from_string logic) ...
    char *keymap_string = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (keymap_string == MAP_FAILED) { perror("mmap failed"); close(fd); return; }
    state->xkb_keymap = xkb_keymap_new_from_string(
        state->xkb_context, keymap_string, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(keymap_string, size);
    close(fd);
    if (state->xkb_keymap) { state->xkb_state = xkb_state_new(state->xkb_keymap); }
}

// 2. Key Handler (Now includes echo)
static void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
                            uint32_t serial, uint32_t time, uint32_t key,
                            uint32_t state_w)
{
    struct client_state *state = data;
    uint32_t keycode = key + 8; // Convert scancode to XKB keycode
    
    if (state_w == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // Translate keysym to a UTF-8 string (the actual character)
        char buffer[32];
        if (state->xkb_state && xkb_state_key_get_utf8(state->xkb_state, keycode, buffer, sizeof(buffer)) > 0) {
            printf("Key Pressed: '%s'\n", buffer);
        } else {
            printf("Key Pressed (No character mapping for scancode: %u)\n", key);
        }
    }
}

// 3. Modifiers (Same as before)
static void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                                  uint32_t serial, uint32_t mods_depressed,
                                  uint32_t mods_latched, uint32_t mods_locked,
                                  uint32_t group) {
    struct client_state *state = data;
    if (state->xkb_state) {
        xkb_state_update_mask(state->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    }
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap = wl_keyboard_keymap,
    .enter = wl_keyboard_enter,      // Can be NULL, handled by XDG shell focus
    .leave = wl_keyboard_leave,      // Can be NULL
    .key = wl_keyboard_key,
    .modifiers = wl_keyboard_modifiers,
    .repeat_info = NULL,
};

// --- Wayland Global Listeners (Updated) ---
static void registry_global(void *data, struct wl_registry *wl_registry,
                            uint32_t name, const char *interface, uint32_t version)
{
    struct client_state *state = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        state->wl_compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        state->wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 1);
        if (state->wl_seat) {
            state->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
            state->wl_keyboard = wl_seat_get_keyboard(state->wl_seat);
            wl_keyboard_add_listener(state->wl_keyboard, &wl_keyboard_listener, state);
        }
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        // BIND XDG Shell
        state->xdg_wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
    } else if (strcmp(interface, "wl_shm") == 0) {
    state->wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
    printf("Bound wl_shm global.\n");
    }
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {}

static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main(int argc, char *argv[]) {
    // Width and height for the visible window
    const int WINDOW_WIDTH = 200;
    const int WINDOW_HEIGHT = 200;
    
    struct client_state state = {0};
    struct wl_buffer *buffer = NULL; // Buffer will be created later

    // 1. Connect to the Wayland display server
    state.wl_display = wl_display_connect(NULL);
    if (!state.wl_display) {
        fprintf(stderr, "Failed to connect to Wayland display. Is a compositor running?\n");
        return EXIT_FAILURE;
    }
    printf("Connected to Wayland display.\n");

    // 2. Get the registry and start listening for globals
    state.wl_registry = wl_display_get_registry(state.wl_display);
    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);

    // Initial roundtrip to fetch all globals (compositor, seat, shm, xdg_wm_base)
    wl_display_dispatch(state.wl_display);
    wl_display_roundtrip(state.wl_display);

    // 3. Verify essential components are bound
    if (!state.wl_compositor || !state.wl_seat || !state.xdg_wm_base || !state.wl_shm) {
        fprintf(stderr, "Error: Missing required Wayland interfaces (compositor, seat, xdg_wm_base, or shm).\n");
        goto cleanup;
    }
    printf("All essential Wayland interfaces bound.\n");

    // 4. Create the surface (window) and XDG Shell objects
    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);

    // Attach listeners
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, &state);
    
    // Set properties for the compositor
    xdg_toplevel_set_title(state.xdg_toplevel, "Wayland Keyboard Echo Test");
    xdg_toplevel_set_min_size(state.xdg_toplevel, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    // 5. Create and attach a buffer to make the window visible
    buffer = create_buffer(&state, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!buffer) {
        fprintf(stderr, "Error: Failed to create wl_buffer for window visibility.\n");
        goto cleanup;
    }
    
    // Attach the buffer to the surface and commit (first commit)
    wl_surface_attach(state.wl_surface, buffer, 0, 0); 
    wl_surface_commit(state.wl_surface); 
    
    // Wait for the xdg_surface_configure event to complete the window mapping
    wl_display_roundtrip(state.wl_display);
    
    printf("--- Client Ready ---\n");
    printf("A %dx%d window is active. CLICK the window, then type keys.\n", WINDOW_WIDTH, WINDOW_HEIGHT);
    printf("--------------------\n");

    // 6. Main event loop
    while (wl_display_dispatch(state.wl_display) != -1) {
        // Events are processed in the listener functions (keyboard, xdg_surface, etc.)
    }

    // 7. Cleanup
cleanup:
    if (state.xkb_state) xkb_state_unref(state.xkb_state);
    if (state.xkb_keymap) xkb_keymap_unref(state.xkb_keymap);
    if (state.xkb_context) xkb_context_unref(state.xkb_context);
    
    // Destroy objects in reverse hierarchy (XDG before core Wayland)
    if (state.xdg_toplevel) xdg_toplevel_destroy(state.xdg_toplevel);
    if (state.xdg_surface) xdg_surface_destroy(state.xdg_surface);
    if (state.wl_surface) wl_surface_destroy(state.wl_surface);
    if (state.xdg_wm_base) xdg_wm_base_destroy(state.xdg_wm_base);
    if (state.wl_shm) wl_shm_destroy(state.wl_shm);
    
    if (state.wl_keyboard) wl_keyboard_destroy(state.wl_keyboard);
    if (state.wl_seat) wl_seat_destroy(state.wl_seat);
    if (state.wl_registry) wl_registry_destroy(state.wl_registry);
    if (buffer) wl_buffer_destroy(buffer); // Destroy the buffer
    
    wl_display_disconnect(state.wl_display);
    return EXIT_SUCCESS;
}
