# Webcam Snapshot Utility

A small Windows app for testing a webcam. It lets you choose a camera, see a live preview, take photos, and adjust brightness and contrast.

## Features

* Lists available webcams.
* Lists available camera resolutions.
* Lets you choose a webcam from a dropdown.
* Starts and stops the live camera preview.
* Takes photos and saves them as JPEG files.

## Requirements

### To run the app

* Windows
* A working webcam
* The correct webcam drivers
* Permission for the app to use the camera, if Windows asks for it

### To build the app

The project uses:

* MinGW `g++`
* C++17
* Standard Windows libraries



## Building

Open a Windows command prompt where the required MinGW tools are available and run:

```bash
make
```

## Running

Start the program:

```text
webcam_app.exe
```

The app will automatically find the webcams connected to your computer and prepare the `Captures` folder.

### 1. Choose a camera

Select a camera from the dropdown at the top-left of the window.

The first camera found is selected automatically.

### 2. Start the camera

Click:

**Start Camera**

The live camera image will appear in the preview area.

While the camera is running:

* The camera dropdown is disabled.
* **Stop Camera** is available.
* **Take Photo** is available.
* The status bar shows `Camera Running...`.

### 3. Adjust the image

Use the sliders on the right:

* **Brightness:** `-100` to `100`
* **Contrast:** `-100` to `100`

Both start at `0`.

Click **Reset Controls** to return both settings to `0`.

These adjustments are applied to the saved photo. They do not change the live preview.

### 4. Take a photo

Click:

**Take Photo**

The photo is saved in the `Captures` folder next to the program.

Example:

```text
Captures/
+-- Capture_20260825_194300_123ms.jpg
```

The filename includes the date, time, and milliseconds so that each photo gets a unique name.

Photos are saved as JPEG files with a quality setting of `90`.

### 5. Stop the camera

Click:

**Stop Camera**

The camera stops and the camera dropdown becomes available again.

## Where Photos Are Saved

Photos are stored next to the program:

```text
webcam_app.exe
Captures/
    Capture_20260825_194300_123ms.jpg
    Capture_20260825_194512_847ms.jpg
    ...
```

The `Captures` folder is created automatically when needed.

## Troubleshooting

### No cameras appear

Check that:

1. The webcam is connected.
2. Windows can see the webcam.
3. The webcam drivers are installed.
4. Another program is not already using the webcam.

### The camera will not start

Some webcams may not work with the video format used by this app.

Try another camera if one is available.

### A photo cannot be saved

The camera must be running and must have captured an image first.

If no image is available, the app shows:

```text
No active frame available to save.
```

Also make sure the folder containing `webcam_app.exe` allows the app to create and save files.

```

The main window contains the camera selector, start/stop buttons, photo button, preview area, brightness and contrast controls, and a status bar.

## Technical Notes

The app uses Windows' built-in camera support to find and communicate with webcams.

When a camera sends an image, the app keeps a copy of the latest frame. When you take a photo, it uses that frame and applies the selected brightness and contrast settings before saving it as a JPEG.

## License / Third-Party Code

This project includes `stb_image_write.h`.

See the included `stb_image_write.h` file for its license and redistribution terms.

No separate license is provided for this project.
