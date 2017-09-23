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
* Node part waits for fullscreen selector (optional) to become available and clicks it (TODO)
* Node part waits for play button selector to become available and clicks it
* Extension backend script, on play, records the tab and sends a blob every second to the node part
* The node part serializes the blob to disk (TODO)

It's much more complicated than that. It has to do things like set parameters for auto-selecting the tab, handle message
passing of blobs, etc. Obviously this is tailored for just this test and would be much more robust in real use.

## Install

* Have chromium dev installed (Windows is at C:\Program Files (x86)\Google\Chrome Dev\Application\chrome.exe)
* Nav to `src/`
* Set env var `PUPPETEER_SKIP_CHROMIUM_DOWNLOAD` to `1` to make sure the full Chromium
* Do `npm install`
* Go to `node_modules/puppeteer/lib/Launcher.js` and remove the `--disable-extensions` line (see TODO section)

## Running

  node main.js --url=<some-url> --fullscreen=<selector-for-fullscreen-button> --play=<selector-for-play-button>

## TODO

* Need desktopCapture on other platforms, ref: https://bugs.chromium.org/p/chromium/issues/detail?id=223639#c29
* Need extension support in headless, ref: https://bugs.chromium.org/p/chromium/issues/detail?id=706008
* Need them to stop blocking extensions in Puppeteer, ref: https://github.com/GoogleChrome/puppeteer/issues/850