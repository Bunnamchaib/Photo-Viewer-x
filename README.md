# Photo-Viewer-x
A pure Win32/GDI+ Windows photo viewer using 35x less RAM than Win11 default app (4.2MB vs 147MB) in a single &lt;1MB EXE. It packs all essential features: zoom, pan, folder browse, &amp; slideshow. Fully open-source—comes with a solid foundation, ready for anyone to fork, improve, and build upon!


# Photo Viewer

A lightweight native Windows image viewer built with pure Win32 API and GDI+.

Simple, fast, low-memory, and made for one job:
open images without making you wait.

## Why this exists

Sometimes you just want to view a photo.

That should be instant.

It should not feel heavy.
It should not eat unnecessary RAM.
It should not make a simple image open feel like launching a full media platform.

That was the whole idea behind this project:

> If the job is just "view an image", why do we have to wait so long? 55555

So this app was built to be:

- simple
- complete enough for real use
- fast to open
- low on resource usage
- easy to run

## Highlights

- Pure Win32 API + GDI+
- Lightweight native desktop app
- Fast startup
- Low RAM usage
- Single EXE deployment
- No extra runtime setup for normal use
- Clean custom UI
- Full screen mode
- Slide show mode
- Drag and drop support
- Folder image navigation
- Zoom, fit, actual size, pan
- Copy image / copy path
- Set desktop background
- Best-effort lock screen background support

## Real-world comparison

Tested by opening the same image:

| App | Memory Usage |
|---|---:|
| Windows 11 default image app | 147.4 MB |
| This Photo Viewer | 4.2 MB |

That is the whole point of this project.

Small job.
Small footprint.
Fast result.

## App size

| App | Size |
|---|---:|
| This app | 799 KB / 0.799 MB |
| Windows 11 image app | likely much larger |

This project is also designed to stay practical:

- one EXE file
- no complicated install flow
- no extra bundled junk
- just run it

## Features

### Image viewing

- Open image from file dialog
- Drag and drop image files into the app
- Open from command-line argument
- Scan the current folder and navigate images in the same folder
- Load only the current image to keep RAM low

### Navigation

- Previous image
- Next image
- Folder-based image browsing

### Zoom and pan

- Mouse wheel zoom
- Zoom In / Zoom Out
- Fit to Window
- Actual Size 100%
- Pan by dragging when zoomed in

### Display modes

- Full screen
- `F11` to toggle full screen
- `Esc` to exit full screen
- Slide show mode using images from the same folder
- `Esc` to stop slide show

### Useful actions

- Copy image
- Copy file path
- Open file location
- Set desktop background
- Set lock screen background (best effort, depending on Windows support)

## UI direction

This viewer keeps a desktop-native feel while still looking more modern than the old stock Win32 look.

Design goals:

- clean layout
- minimal chrome
- modern spacing
- lightweight custom-painted controls
- still fast and simple

## Technology

- Language: C++
- Window/UI: Win32 API
- Image loading/rendering: GDI+
- Build target: Windows
- Packaging: single EXE

## Build

This project is intended to compile with MinGW / Dev-C++ style toolchains.

### Resource compile

```bash
windres PhotoViewer.rc -O coff -o PhotoViewer_res.o
```

### Main build

```bash
g++ -std=c++17 -Wall -Wextra -O2 -mwindows -static -static-libgcc -static-libstdc++ PhotoViewer.cpp PhotoViewer_res.o -o PhotoViewer.exe -lgdiplus -lcomdlg32 -lshell32 -lole32
```

## Project files

- `PhotoViewer.cpp` - main application source
- `PhotoViewer.rc` - Windows resource file
- `resource.h` - resource IDs
- `photo.ico` - app icon
- `PhotoViewer.exe` - built executable

## Open source

This project is open source.

If anyone wants to improve it, redesign it, optimize it further, or add more features, feel free to continue from here.

Take it, build on it, and make it better.

## Philosophy

Software does not always need to be huge to be useful.

Sometimes the best tool is the one that:

- opens fast
- uses very little RAM
- does the job well
- gets out of your way

That is what this project is trying to be.
<img width="1166" height="772" alt="image" src="https://github.com/user-attachments/assets/3d3958a3-827c-433c-8764-1723bc8d66e3" />

<img width="1166" height="772" alt="image" src="https://github.com/user-attachments/assets/dd76dda6-f5fd-4449-9f68-cc0018ab76a2" />

<img width="1166" height="772" alt="image" src="https://github.com/user-attachments/assets/84c92aca-7800-4a54-b3c4-588b11c64fa2" />



**"RAM consumption varies depending on the type and size of the image being opened."**



<img width="1151" height="192" alt="image" src="https://github.com/user-attachments/assets/c8321c89-6396-4dd4-9281-dd56d026456a" />






