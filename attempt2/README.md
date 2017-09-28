## Attempt 2

## Tech Used

* Win32
* Media Foundation
* Direct3D

## How it Works

* Uses underlying Windows technology to use the GPU to capture desktop video

## Build

Prereqs:

* Latest Qt (5.x) installed w/ `qmake.exe` on the `PATH`
* Latest Go installed w/ `go.exe` on the `PATH`
* [MSVC 2015 Build Tools](http://landinghub.visualstudio.com/visual-cpp-build-tools) installed w/ the following
  executed to put 64-bit VC compiler on the `PATH`:
  `"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64`

Navigate to `src/` folder and run `go run build.go build release`

## Running

In `src/`

    release/attempt2.exe

This takes a simple screenshot and records ~15 seconds of video.

## Results

* Video is nice, high bitrate and framerate
* We may have some sample timing concerns when serializing to video
* All in same thread so surely missing some frames
* Audio is a separate concern
* Sadly we cannot do this in the background which sucks (CreateDesktop doesn't work w/ desktop duplication)

## TODO

* Multiple threads
* Make several videos comparing timing
* Audio