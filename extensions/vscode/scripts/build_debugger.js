const execSync = require('child_process').execSync;
const path = require('path');
const fs = require('fs');

const root = path.resolve(__dirname, '../../..');
console.log(root);

const build_root = path.resolve(root, 'build');
if (fs.existsSync(build_root))
  fs.rmSync(build_root, { recursive: true });

const configure = `cmake --preset windows-clang-release`
console.log(configure);
execSync(configure, { stdio: 'inherit', cwd: root });

const build = `cmake --build --preset release-build`
console.log(build);
execSync(build, { stdio: 'inherit', cwd: root });

const src_path = path.resolve(root, 'build/luaud/luaud.exe');
const dest_path = path.resolve(root, 'extensions/vscode/debugger/luaud.exe');
console.log(src_path, dest_path);

const dest_parent = path.dirname(dest_path);
if (!fs.existsSync(dest_parent))
  fs.mkdirSync(dest_parent);

if (fs.existsSync(dest_path))
  fs.removeSync(dest_path);

fs.copyFileSync(src_path, dest_path);

