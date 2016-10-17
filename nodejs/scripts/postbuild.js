// This script runs after the install script's build process.
// All it does is move the built uws addon to the where it's
// supposed to be for the native.js require script.
const fs = require('fs');
const path = require('path');

const platform = process.env.BUILD_PLATFORM;
const abi = process.env.BUILD_NODE_ABI;

if (!platform || !abi) {
  throw new Error('No platform or ABI specified in environment variables!');
}

const fromFile = path.join(__dirname, '../build/Release/uws.node');
const toFile = path.join(__dirname, `../lib/uws_${platform}_${abi}.node`);

fs.renameSync(fromFile, toFile);
