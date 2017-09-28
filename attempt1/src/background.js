/* global chrome, MediaRecorder, FileReader */

chrome.runtime.onConnect.addListener(port => {
  let rec = null
  port.onMessage.addListener(msg => {
    switch (msg.type) {
      case 'REC_CLIENT_STOP':
        console.log('Stopping recording')
        if (!port.recorderPlaying || !rec) {
          console.log('Nothing to stop')
          return
        }
        port.recorderPlaying = false    
        rec.stop()    
        break
      case 'REC_CLIENT_PLAY':
        if (port.recorderPlaying) {
          console.log('Ignoring second play, already playing')
          return
        }
        port.recorderPlaying = true
        const tab = port.sender.tab
        tab.url = msg.data.url
        chrome.desktopCapture.chooseDesktopMedia(['tab', 'audio'], streamId => {
          // Get the stream
          navigator.webkitGetUserMedia({
            audio: { mandatory: { chromeMediaSource: 'system', chromeMediaSourceId: streamId } },
            video: {
              mandatory: {
                chromeMediaSource: 'desktop',
                chromeMediaSourceId: streamId,
                minWidth: 1280,
                maxWidth: 1280,
                minHeight: 720,
                maxHeight: 720,
                minFrameRate: 60,
              }
            }
          }, stream => {
            // Now that we have the stream, we can make a media recorder
            rec = new MediaRecorder(stream, {
              mimeType: 'video/webm; codecs=vp9',
              // mimeType: 'video/webm; codecs=h264',
              // mimeType: 'video/mpeg4',
              // mimeType: 'video/x-matroska;codecs=avc1',
              audioBitsPerSecond: 128000,
              videoBitsPerSecond: 30000000,
              // bitsPerSecond: 3000000 
            })
            rec.onerror = event => console.log('Recorder error', event)
            rec.onstart = event => port.postMessage({ type: 'REC_BACKEND_START' })
            rec.onstop = event => port.postMessage({ type: 'REC_BACKEND_STOP' })
            rec.ondataavailable = event => {
              if (event.data.size > 0) {
                // We have to read this with a FileReader and return that sadly
                // Ref: https://stackoverflow.com/questions/25668998/how-to-pass-a-blob-from-a-chrome-extension-to-a-chrome-app
                const reader = new FileReader()
                reader.onloadend = () => port.postMessage({ type: 'REC_BACKEND_BLOB', blob: reader.result })
                reader.onerror = error => console.log('Failed to convert blob', error)
                reader.readAsBinaryString(event.data)
              }
            }
            rec.start(200)
          }, error => console.log('Unable to get user media', error))
        })
        break
      default:
        console.log('Unrecognized message', msg)
    }
  })
})
