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