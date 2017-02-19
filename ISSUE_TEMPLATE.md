First check if your issue is a **frequently posted issue**:
* You are not using NAN, use it!
  * No, this project will never use NAN.
* You are not using prebuild, use it!
  * No.
* The EventEmitter of uws is not 100% compatible with standard EventEmitters
  * This is by design. EventEmitters are cancerous and uws does not aim to implement them in verbatim.
* The project is completely broken and fundamental feature X does not work at all
  * Chances are the issue is on your end. This project is used by more than you and we would have known if it was this broken.
* I cannot compile or link the project \<insert compiler or linker error\>
  * In 99% of cases this is issues on your end. In some cases I leave master in a broken state. Try a released version first.
  
**Include the following information (where applicable)**:

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
