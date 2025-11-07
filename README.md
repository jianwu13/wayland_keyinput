# wayland_keyinput

## About
This sample program uses the Wayland C library to retrieve and display key inputs.
It was created using Gemini.

![Wayland_Keyinput](doc/Wayland_Keyinput.png)

Confirmed to be working on Ubuntu 25.10.

## Build Instructions

- Clone the [wayland_keyinput repository](https://github.com/jianwu13/wayland_keyinput.git):

    ```bash
    git clone git@github.com:jianwu13/wayland_keyinput.git
    ```

- make

    ```bash
    make
    ```

	or

    ```bash
    gcc wayland_echo.c xdg-shell-protocol.c -o wayland_echo $(pkg-config --cflags --libs wayland-client xkbcommon)
    ```

## Generate the Header File Manually:

    ```bash
    dpkg -L wayland-protocols | grep xdg-shell.xml
    ```

Expected output path is likely similar to: /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

Replace the path below with the actual path found via dpkg -L

    ```bash
    wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h
    ```
## Generate the Protocol C Implementation

    ```bash
    wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
    ```
