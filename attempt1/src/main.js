const fs = require('fs')
const puppeteer = require('puppeteer')

// Gather the params
const args = {}
for (const arg of process.argv) {
  if (arg.startsWith('--')) {
    const equalsIndex = arg.indexOf('=')
    if (equalsIndex !== -1) args[arg.substr(2, equalsIndex - 2)] = arg.substr(equalsIndex + 1)
  }
}
if (!args.url) throw new Error('URL required')

;(async () => {
  // Fire up the browser
  const browser = await puppeteer.launch({
    // Ug: https://bugs.chromium.org/p/chromium/issues/detail?id=706008
    headless: false,
    // TODO: smarter way to find this
    executablePath: 'C:\\Program Files (x86)\\Google\\Chrome Dev\\Application\\chrome.exe',
    args: [
      // '--auto-open-devtools-for-tabs',
      '--auto-select-desktop-capture-source=pickme',
      '--disable-infobars',
      '--load-extension=' + __dirname,  // eslint-disable-line no-path-concat
      '--no-sandbox',
      '--disable-setuid-sandbox',
      // No autoplay
      '--autoplay-policy=user-gesture-required'
    ],
    slowMo: 100
  })

  const page = await browser.newPage()

  let outStream = null
  await page.exposeFunction('recorderStart', () => {
    if (!outStream) {
      const fileName = args.file || 'out.webm'
      console.log('Opening ' + fileName)
      outStream = fs.createWriteStream(fileName, 'binary')
    }
  })
  await page.exposeFunction('recorderBlob', blob => {
    // Comes in here as a string
    if (outStream) {
      console.log('Writing ' + blob.length + ' bytes to ' + outStream.path)
      outStream.write(blob, 'binary')
    }
  })
  await page.exposeFunction('recorderStop', () => {
    console.log('Rec end')
    if (outStream) {
      console.log('Closing ' + outStream.path)
      outStream.end()
    }
  })
  await page.evaluateOnNewDocument(() => {
    window.addEventListener('message', event => {
      // Only handle backend messages here
      if (!event.data.type || !event.data.type.startsWith('REC_BACKEND_')) return
      switch (event.data.type) {
        case 'REC_BACKEND_START':
          window.recorderStart()
          break
        case 'REC_BACKEND_STOP':
          window.recorderStop()
          break
        case 'REC_BACKEND_BLOB':
          window.recorderBlob(event.data.blob)
          break
        default:
          console.log('Unrecognized message', event.data)
      }
    })
  })
  await page.goto(args.url)

  // Go over all "click-" and do the clicking as necessary
  for (const argName in args) {
    if (argName.startsWith('click-') && !argName.startsWith('click-delay-')) {
      const selector = args[argName]
      const delay = args['click-delay-' + argName.substr(6)]
      const doWait = () => {
        console.log('Waiting for', selector, 'delay', delay)
        page.waitForSelector(selector)
          .then(() => {
            console.log('Found selector', selector, 'clicking after delay')
            return page.waitFor(delay ? parseInt(delay, 10) : 0)
          })
          .then(() => page.click(selector).catch(() => doWait()))
      }
      doWait()
    }
  }

  // await browser.close();
})()
