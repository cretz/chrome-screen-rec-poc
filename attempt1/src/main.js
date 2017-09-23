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
      '--auto-open-devtools-for-tabs',
      '--auto-select-desktop-capture-source=pickme',
      '--load-extension=' + __dirname,  // eslint-disable-line no-path-concat
      '--no-sandbox',
      '--disable-setuid-sandbox'
    ],
    slowMo: 100
  })

  const page = await browser.newPage()
  await page.exposeFunction('recorderStart', () => {
    console.log('Recorder has begun!')
  })
  await page.exposeFunction('recorderBlob', blob => {
    // Comes in here as a string
    console.log('Recorder blob', blob.length)
    // TODO: persist
  })
  await page.exposeFunction('recorderStop', () => {
    console.log('Recorder has ended!')
  })
  await page.evaluateOnNewDocument(() => {
    window.addEventListener('message', event => {
      // Only handle backend messages here
      if (!event.data.type || !event.data.type.startsWith('REC_BACKEND_')) return
      switch (event.data.type) {
        case 'REC_BACKEND_START':
          window.recorderBegin()
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
  await page.goto(args.url, { waitUntil: 'load' })

  // Click the play button
  if (args.play) {
    await page.waitForSelector(args.play)
    await page.click(args.play)
  }

  // await browser.close();
})()
