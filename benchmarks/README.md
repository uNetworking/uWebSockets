## So you're the skeptic kind, huh? I like you already!

The world needs more like you. I could talk about how stupid and gullible 95% of all programmers are for hours, but in short it boils down to:

* Never trust *anyone* who talks about or *claims* performance of *any* kind unless a very clear and easy to reproduce *benchmark* has been openly presented and reviewed.
* Never accept random *numbers* which has no *point of reference* to similar software projects. The number "1 million" might sound fancy but without *any* kind of reference to *other* projects there's no knowing the worth of such a number. Comparative benchmarks are the only ones worth taking in. Think for yourself - how can the author claim "fast" if he/she has no benchmarks of what *others* are doing?
* Most people who do benchmarks are complete bunglers and do not know their ass from their head. Always make sure to *normalize* performance by spent CPU-time. That is, do NOT compare performance of software demanding 100% CPU-time on 57 CPU cores, with software running on 5% CPU-time on 1 CPU core. You would be stunned if you knew how few people actually care to even check this before posting their results to their literally millions of gullible followers.

Http | WebSockets
--- | ---
![](../misc/bigshot_lineup.png) | ![](../misc/websocket_lineup.png)

### The bystander effect
Having many users and many "stars" does not constitute a confirmation of any such *claims*. In fact, the opposite is more often true than not. How can this be true? Well for starters, actual performance often come with restrictions and more cumbersome APIs and it turns out most people are not really interested in doing more work than what they absolutely have to. Programmers want to *believe* their overly abstract garbage collected functional one liners to be "more efficient than machine code". They want to have the cake, and a cake cloning device too.

Also, there's such a fenomenom where we humans are *less* likely to help someone in need of help when there are others present. We like to think to ourselves that, with this many people around, there *surely* has to be someone already helping that poor fellow. The thought of someone dying of blood loss and nobody helping is so absurd to people that they immediately refuse to even consider that case.

The same goes with software projects with massive amounts of popularity. People think that it *cannot possilbly* be true that such a massively popular project is *completely* trash in regards to performance! *Surely* it cannot be *this* bad!?

The perfect example is Socket.IO - a project downloaded millions of times every single day. Books have been written, people use it everywhere. It *claims* using superlatives to be *the fastest in the universe and all other universes and across all space and time* yet delivers such a dreadful performance you would need close to 200 modern computers running egg fryingly hot just to saturate a mesely 1 Gbit network with small messages. I consider myself an open minded person but even I have problems wrapping my mind around such a massive load of insanity.

### Enough about psychology
The performance of µWebSockets is in most cases what you would expect from an efficient server software. In some cases, especially small messages, the difference in performance can be dramatical (10 - 15x). Because of the nature of WebSocket messages (they are mostly very small at sub 1kb), this plays a major role in performance.

Node.js, a very slow server, will perform at roghly the same performance as µWebSockets for large HTTP responses and or large WebSocket messages (think 50+ MB). This is because at those message sizes the real bottleneck is not the user space server software but instead the Linux kernel. You essentially bottleneck on syscalls at those message sizes unless you bring in user space TCP stacks, which very few do.

For small HTTP responses and small WebSocket messages (below 16 kb, especially below 4 kb) you can expect to see dramatical differences. If you pipeline responses you can see up to 1000x difference, but those numbers are mostly a load of bull.
