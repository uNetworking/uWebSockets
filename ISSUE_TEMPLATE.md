## Frequently posted issues
**I cannot compile or link the project \<insert compiler or linker error\>**  
Try a released version, master branch is where development happens. If you still have issues then you need to check your end very thoroughly before posting this issue. You are not the only one using µWS and it is probably not this broken.

**Fundamental feature X is broken**  
Again, check your end very thoroughly before posting such an issue. µWS is successfully used by many others.

**You are not using NAN, prebuild or \<insert overrated project X\>**  
I know, this is by design. Things like NAN would render this project a million times more bloated.

**Feature X is not 100% identical to feature X of `ws`**  
Things like EventEmitters differ in behavior between the two projects. Other differences are listed on the main README. This is by design.

**Compilation of µWebSockets has failed and there is no pre-compiled binary available for your system**  
This error message is more accurate than you might think. Install a C++11 compiler or update your distribution. You can also dig in uws.js yourself for more information before posting this issue.
  
## Include the following information (where applicable):

* What operating system are you running?
  * Linux
  * OS X
  * Windows
* Are you running any CPU architechute out of the normal?
  * I run 32-bit in the year 2017
* What platform/language are you using?
  * C++
  * Node.js
* Are you using SSL, if so, how?
  * SSL is not part of the equation
  * SSL via proxy (eg. NGINX)
  * SSL via µWS or Node.js
* What version of uws are you using?
  * master
  * Release v0.x.x
* What features of µWS are involved in the issue?
  * Server support (WebSocket)
  * Client support (WebSocket)
  * Server support (HTTP)
* How does the issue reproduce?
  * Randomly (specify approximate percentage)
  * All the time
* Can you show me some short code that triggers your issue or any other important information?
