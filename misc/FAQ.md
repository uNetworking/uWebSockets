## Build systems
Q:
```
Hi Alex,
I've used your library and I like it, however, to integrate it in my process it needs to use cmake and conan. I have written those for my own project, would you like me to submit a pull request? 

Regards,
Ioannis
```
A:
```
Hi Ioannis,

This is a very common request. I don't accept any specific build systems because I know from experience that doing so opens up hell. People simply cannot agree on which build system to use, or even how to use one particular build system.

That's why I no longer accept any such PRs. I've had too many CMake PRs dragging in completely different directions to know this is the only solution.

You'll have to clone the repo and create a new project in whatever build system you want to use.
```
