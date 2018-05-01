This is my answer to [tinywm](http://incise.org/tinywm.html).

First off, thank you, Nick Welch, for writing tinywm. With only 50 lines of C, tinywm is a remarkably concise window manager, which I found essential for learning the gist of how a window manager works. No modification is required for achieving this end.

Of course, some corners were intentionally cut. It goes without many features most users would find essential, such as borders, toolbars, EWMH, etc. But as I tried to use it as an actual window manager, I also found that it exhibited buggy behavior, which I don't consider to be "nice to have" features.

Additionally, in an attempt to keep the code concise, some of the code is cryptic (specifically the arguments passed to XMoveResizeWindow).

So, while tinywm is an excellent way to convey the gist of window management, I felt I could contribute a version which would be instructive in different ways:

* Spell out things a bit more, which requires more lines of code but is easier to read.
* Fix the sluggish resizing bug by creating an opaque, phony window for the purpose of resizing. This also demonstrates how to create, map, and unmap windows.
* Fix the input focus bug.
* Use the XCB library, which drastically cuts the already tiny resource usage of tinywm. This also demonstrates the library used by most modern window managers.

Otherwise, it is identical in functionality to the original tinywm.
