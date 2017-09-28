# Attempt 1

## Tech Used

* Node
* Puppeteer for automating Chrome (ref: https://github.com/GoogleChrome/puppeteer)
* Chrome extension with desktopCapture
* HTML5 MediaRecorder API 

## How it Works

* Starts node app
* Node app starts chrome in automation mode with extension loaded
* Node makes callback for future recorder blobs
* Navigate to URL
* Extension frontend script collects video tags and listens for play to notifies extension backend script
* Node part waits for selectors to show up and clicks em
* Extension backend script, on play, records the tab and sends a blob every second to the node part
* The node part serializes the blob to disk

It's much more complicated than that. It has to do things like set parameters for auto-selecting the tab, handle message
passing of blobs, etc. Obviously this is tailored for just this test and would be much more robust in real use.

## Install

* Have chromium dev installed (Windows is at C:\Program Files (x86)\Google\Chrome Dev\Application\chrome.exe)
* Nav to `src/`
* Set env var `PUPPETEER_SKIP_CHROMIUM_DOWNLOAD` to `1` to make sure the full Chromium is not downloaded
* Do `npm install`
* Go to `node_modules/puppeteer/lib/Launcher.js` and remove the `--disable-extensions` line (see TODO section)

## Running

    node main.js --url=URL --file=FILE --click-N=SELECTOR --click-delay-N=MS

Args:

* `--url` - The URL to load. Required.
* `--file` - The file to save. Default is `out.webm`
* `--click-N` - A selector to an element to click. `N` should be unique.
* `--click-delay-N` - Number of milliseconds after seeing the selector before click should occur. Default is no time.

Example:

    node main.js --url="https://www.youtube.com/embed/0vrdgDdPApQ?autoplay=0&modestbranding=1&showinfo=0&controls=0" --click-play=.ytp-large-play-button

## Results

These are only the results so far

* The video recording causes short pauses on each save interval. Probably need web workers.
* Video playback is choppy regardless of FPS or bitrate settings. Need to check whether it's just VLC or what.

Chances are this just ain't gonna work for us as Chrome's built-in screen recording is too slow.

## TODO

* Need desktopCapture on other platforms, ref: https://bugs.chromium.org/p/chromium/issues/detail?id=223639#c29
* Need extension support in headless, ref: https://bugs.chromium.org/p/chromium/issues/detail?id=706008
* Need them to stop blocking extensions in Puppeteer, ref: https://github.com/GoogleChrome/puppeteer/issues/850