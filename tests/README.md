# Testing
A serious project needs extensive testing. If this folder looks empty to you, or lacking - mind you this is only the unit tests (which are a growing number).

* We test with 500+ **standards conformance tests** known as the Autobahn|Testsuite where we get a full perfect score.
* In the fuzzing folder you will find extensive **security fuzz testing** under AddressSanitizer, MemoySanitizer, UndefinedBehaviorSanitizer executed by Google on a daily basis.
* In benchmarks folder you can find **performance testing** - we've done extensive performance testing of every commit since before we even had a single line of code.
* We are working on a set of **integration smoke tests** for testing basic features against real-world web browsers (some browsers don't fully follow standards but we still want to play along).
* And of course in this folder you can find the very **module unit tests** that check individual modules.
* Customers and users indirectly feed us with **real world, high scale production testing**.
* Semmle perform **static code analysis** and security testing for us, where we get the perfect A+ score.
