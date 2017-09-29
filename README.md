# Chrome Screen Recorder Proof of Concept

## The Challenge

This project will contain several attempts to build a recorder of videos playing in Chrome. Requirements for a
successful attempt:

* Must be able to record all types of content.
* Must maintain 60fps on a minimal laptop (mainly, mine)
* Must do it headlessly
* Must record audio in sync
* Must record to disk losslessly

Now, this might take several attempts w/ several techs (e.g. CEF OSR, Windows DCs, Chrome headless screen frames,
Chrome extensions recordings, webrtc/getUserMedia, Chromecast emulation, etc). Some attempts in this repository may only
solve one problem (e.g. audio) but the PoC is not considered successful until it can all come together. Bonus points for
a cross-platform solution.

Good starting test case: https://www.youtube.com/embed/0vrdgDdPApQ

TODO: Need EME test case

## Attempts

* [`attempt1/`](attempt1) - Use Chrome automation and Chrome screen capture
  * Result - The screen frames are just not captured fast enough, but close (maybe try threaded)
* [`attempt2/`](attempt2) - Use Windows desktop duplication to capture
  * Result - Video is high quality, but cannot do it headlessly
* `attemptN` - Use CEF + offscreen rendering + captureStream for audio only. Try threading as needed.
* `attemptN` - Use CEF + captureStream (video + audio) w/ native callbacks. Try threading as needed.
* `attemptN` - Use dev tools protocol + Page.\*screencast\*. Try headless. Use non-JS lang w/ concurrency.
* `attemptN` - Custom Chrome build (e.g. a patch on top of https://github.com/Eloston/ungoogled-chromium/ if they get up
  and running again) to hook into the audio/video as close to the source as possible.
* `attemptN` - Custom Chrome build with custom CA key patch for Chromecast device. Then make Chromecast server.