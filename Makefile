# --- Variables for easier maintenance ---
CFLAGS := $(shell pkg-config --cflags wayland-client xkbcommon)
LDLIBS := $(shell pkg-config --libs wayland-client xkbcommon)

# --- Final Target (Linking Step) ---
wayland_echo: wayland_echo.o xdg-shell-protocol.o
	gcc $^ -o $@ $(LDLIBS)

# --- Compilation Targets (Creating Object Files) ---

# Compiles the main application source file
wayland_echo.o: wayland_echo.c xdg-shell-client-protocol.h
	gcc -c $< $(CFLAGS) -o $@

# Compiles the generated XDG protocol source file
xdg-shell-protocol.o: xdg-shell-protocol.c
	gcc -c $< $(CFLAGS) -o $@

# --- Protocol Generation (Automation) ---

# Rule to automatically generate the implementation file needed for linking
xdg-shell-protocol.c:
	wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml $@

# Rule to automatically generate the header file needed for compilation
xdg-shell-client-protocol.h:
	wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml $@

# --- Cleanup Target ---
.PHONY: clean
clean:
	rm -f *.o wayland_echo xdg-shell-protocol.c xdg-shell-client-protocol.h
