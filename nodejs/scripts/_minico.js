// https://github.com/seapunk/minico
// async/await emulation.

'use strict';

function spawn (coroutine) {
  return new Promise(function (resolve, reject) {
    (function next (val, err) {
      var result;
      try {
        result = err ? coroutine.throw(err) : coroutine.next(val);
      } catch (err) {
        return reject(err);
      }

      if (result.done) return resolve(result.value);

      Promise.resolve(result.value).then(next).catch(function (err) {
        next(null, err);
      });
    })();
  });
}

function copromise (coroutine) {
  return function () {
    return spawn(coroutine.apply(this, arguments));
  };
}

module.exports = copromise;
