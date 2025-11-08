# wayland\_keyinput

## 🌟 About
This project provides a minimal, self-contained **Wayland C client** designed to **capture and display keyboard input** using the XDG Shell and `libxkbcommon`. It serves as a tested and verified example for setting up complex Wayland dependencies from scratch.

This client was created with the assistance of the Gemini AI model.

| Feature | Status |
| :--- | :--- |
| **Functionality** | Keyboard echo test (key presses output to console) |
| **Confirmed OS** | Ubuntu 25.10 (Development Release) |
| **License** | MIT |

!(doc/Wayland_Keyinput.png)

---

## 📦 Prerequisites

To build and run this client, you need the following development packages installed.

### 1. Essential Development Libraries

| Library Package | Role in Code | Installation Command |
| :--- | :--- | :--- |
| **`libwayland-dev`** | Core Wayland API (connecting, registry). | `sudo apt install libwayland-dev` |
| **`libxkbcommon-dev`** | Translating raw key events to characters. | `sudo apt install libxkbcommon-dev` |
| **`wayland-protocols`** | Provides the source **XML definitions** (`xdg-shell.xml`). | `sudo apt install wayland-protocols` |
| **`wayland-scanner`** | Tool used to generate C code from protocol XML files. | `sudo apt install wayland-scanner` |

### 2. Note on Protocol Files

Due to inconsistencies in some distribution packages (like Ubuntu 25.10), the necessary **header and source files** for the XDG Shell protocol may not be installed.

Therefore, the protocol files must be **generated manually** using `wayland-scanner` before compilation (see Build Instructions below).

---

## 🛠️ Build Instructions

### 1. Clone the Repository

```bash
git clone git@github.com:jianwu13/wayland_keyinput.git
cd wayland_keyinput
```

### 2. Generate Protocol Source Files

First, verify the path to the protocol XML file:

```bash
dpkg -L wayland-protocols | grep xdg-shell.xml
# Expected path: /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
```

Next, use `wayland-scanner` to generate the required C files (replace the path if your system is different):

```bash
# Generate the header file (for the compiler to read)
wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h

# Generate the C implementation file (for the linker)
wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
```

### 3. Compile the Executable

You must link both the main C file and the generated protocol C file.

* **Using `make` (Recommended):**
```bash
make
```

* **Using `gcc` Directly:**
```bash
gcc wayland_echo.c xdg-shell-protocol.c -o wayland_echo $(pkg-config --cflags --libs wayland-client xkbcommon)
```

---

## 🏃 Running the Client

Ensure you are running the program from within a Wayland session (e.g., inside a terminal that is running on your Wayland compositor).

```bash
./wayland_echo
```

A small **solid gray window** will appear. **Click inside the window** to give it keyboard focus, and then type keys to see the echo output (e.g., "Key Pressed: 'a'") in your terminal.

---

## 💡 Troubleshooting Tips

If you encounter errors related to **`pkg-config`** not finding libraries, ensure you've installed all the necessary **`-dev`** packages listed in the **Prerequisites** section.

If the window is invisible or you experience crashes when clicking outside the window, review the `wayland_echo.c` source code to confirm the following fixes are present:
1.  **Buffer Filling:** The `create_buffer` function must explicitly fill the memory with a non-transparent color (e.g., `0xFF999999`).
2.  **Focus Handlers:** The `wl_keyboard_listener` must have non-`NULL` functions assigned to both the `.enter` (opcode 1) and `.leave` (opcode 2) events.
