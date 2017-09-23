/* global chrome, MutationObserver */

window.onload = () => {
  if (window.recorderInjected) return
  Object.defineProperty(window, 'recorderInjected', { value: true, writable: false })

  // Setup message passing
  const port = chrome.runtime.connect(chrome.runtime.id)
  port.onMessage.addListener(msg => window.postMessage(msg, '*'))
  window.addEventListener('message', event => {
    // Relay client messages
    if (event.source === window && event.data.type && event.data.type.startsWith('REC_CLIENT_')) {
      port.postMessage(event.data)
    }
  })

  // Recursively find all video elements and append to the array
  const findVideoElements = (searchNodeList, retArr) => {
    for (const node of searchNodeList) {
      if (node.nodeName === 'VIDEO') {
        // Gotta make sure it's not there yet
        let found = false
        for (const other of retArr) {
          if (other.isSameNode(node)) {
            found = true
            break
          }
        }
        if (!found) retArr.push(node)
      }
      if (node.children && node.children.length > 0) findVideoElements(node.children, retArr)
    }
  }

  // Set handlers on the video
  const setVideoHandlers = videoElem => {
    if (!videoElem.recorderSet) {
      videoElem.recorderSet = true
      console.log('Setting video play handler', videoElem)
        // Setup onplay handler to trigger recording
      videoElem.onplay = () => {
        // Change the title so we can be picked
        document.title = 'pickme'
        window.postMessage({ type: 'REC_CLIENT_PLAY', data: { url: window.location.origin } }, '*')
      }
    }
  }

  // Find already existing videos
  for (const videoElem of document.querySelectorAll('video')) {
    setVideoHandlers(videoElem)
  }

  // Start the observer to find any video tags
  const obs = new MutationObserver(records => {
    const videoElems = []
    for (const record of records) findVideoElements(record.addedNodes, videoElems)
    if (videoElems.length > 0) setVideoHandlers(videoElems[0])
  })
  obs.observe(document, { childList: true, subtree: true })
}
