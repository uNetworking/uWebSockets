
# Motivation
The original [`µWS` project](https://github.com/alexhultman/uWebSockets) makes no compromise and states to be the fastest ws server (not for only for nodejs, but of all available implementations). The whole thing is designed to handle zillions of lnk/socket per server and the source code is _strongly_ designed for performances.

This project intends to soften the nodejs' integration of the whole thing. (and do not aim to be used as a c++ lib at all)


# Differences with uws
* Change in the file tree structure (helps the npm workflow, just `git clone & npm install`)
* no pre-compiled binaries whatsoever
* Expose local binding port & addr



# TODO
* Sync with uws source code
* Switch to nan for multi-version abstraction


=====
Please refer the [`µWS` main repo](https://github.com/alexhultman/uWebSockets)  for an up-to-date version.
