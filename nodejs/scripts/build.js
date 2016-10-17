'use strict';

const childProcess = require('child_process');
const minico = require('./_minico');

function exit (code) {
  // This makes sure everything has been flushed to
  // stdout and stderr, before exiting.
  //
  // Node has this annoying problem where it likes to close the process
  // before it's finished writing all the data to console.
  process.stdout.write('', () => {
    process.stderr.write('', () => {
      process.exit(code);
    });
  });
}

function buildFor (platform, nodeVersion, abi) {
  return new Promise((resolve, reject) => {
    const buildProcess = childProcess.exec('npm run build:gyp', {
      env: Object.assign(process.env, {
        BUILD_NODE_VERSION: nodeVersion,
        BUILD_PLATFORM: platform,
        BUILD_NODE_ABI: abi
      }),
      timeout: 120000,
      maxBuffer: 1024 * 1000
    });

    // Pipe the child process's stdout and stderr to our stdout and stderr.
    buildProcess.stdout.pipe(process.stdout);
    buildProcess.stderr.pipe(process.stderr);

    buildProcess.on('error', err => {
      // We don't want the exit handler code to run.
      buildProcess.removeAllListeners('exit');

      return reject(err);
    });

    buildProcess.on('exit', exitCode => {
      if (exitCode !== 0) {
        return reject(new Error(`Build exited with non-0 error code: ${exitCode}`));
      }

      return resolve();
    });
  });
}

let addonList;
if (process.argv[2] === 'all') {
  console.log('Building addon for all Linux platforms.');

  addonList = [
    ['linux', 'v4.4.3', '46'],
    ['linux', 'v5.11.0', '47'],
    ['linux', 'v6.4.0', '48']
  ];
} else {
  console.log('Building addon for this platform.');

  addonList = [
    [process.platform, process.version, process.versions.modules]
  ];
}

minico(function * () {
  for (let addon of addonList) {
    const platform = addon[0];
    const nodeVersion = addon[1];
    const abi = addon[2];
    console.log(`Building addon for: ${platform}, node ${nodeVersion} @ ABI ${abi}`);
    yield buildFor(platform, nodeVersion, abi);
  }
})().then(() => {
  return exit(0);
}).catch(err => {
  console.error('Could not build addon(s)');
  console.error(err.stack);
  return exit(1);
});
