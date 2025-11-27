const { execSync } = require('child_process');
const { version } = require('../package.json');
const path = require('path');

const outPath = path.join('out', `luau-debugger-${version}.vsix`);
execSync(`npx vsce package -o "${outPath}"`, { stdio: 'inherit' });
