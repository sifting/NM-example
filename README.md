# NM-example
Sample NM threading code

***

This shows off the core functionality required to get a NM threading model running on Linux.
The magic comes in a few parts: the signal handler and `pthread_kill`.

In Linux, signals are issued to the process, however, the thread in which the signal handler is invoked is controllable through a signal masks, and, more importantly, through `pthread_kill`, whose name is a misnomer: its primarily function is to send signals to threads, _NOT_ just to kill them. With these two mechanisms it is now possible to interrupt a thread via the signal handler. This is necessary since some jobs may run for indefinite periods of time, and without the ability to interrupt them, it would be impossible to have a timely responses to outside events. The final part of the magic trick is the humble `longjmp`. `longjmp` is safe to call from signal handlers, and in fact, the advanced linux programming guide says this perfectly reasonable thing to do. `longjmp` also has an interesting property: it can be used to program co-operative multi-threading, a.k.a. fibers, a.k.a. co-routines. Together with these pieces it's possible to create NM programming model. Tasks can be started, cancelled, paused, resumed or requeued from any thread with this mechanism, enabling some fascinating opportunities, such as a massively parallel depth-first-search, where each decision node could spawn a task to search, then once the goal is found, a mass cancel could be issued to save compute resources.
