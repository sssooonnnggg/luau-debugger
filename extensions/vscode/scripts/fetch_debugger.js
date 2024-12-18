const extract = require('extract-zip')
const fs = require('fs')
const path = require('path')
const child_process = require('child_process')

const platforms = ['windows', 'ubuntu']
const root = path.resolve(__dirname, '..')

async function main() {
  const debugger_root = path.resolve(root, 'debugger')
  if (fs.existsSync(debugger_root)) {
    fs.rmdirSync(debugger_root, { recursive: true })
  }

  fs.mkdirSync(debugger_root)
  for (const platform of platforms) {
    const url = `https://github.com/sssooonnnggg/luau-debugger/releases/latest/download/luaud-${platform}.zip`
    const cmd = `curl -L -o ${root}/luaud-${platform}.zip ${url}`
    child_process.execSync(cmd)
    await extract(`./luaud-${platform}.zip`, { dir: debugger_root })
  }
}

main()

