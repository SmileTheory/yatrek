# yatrek
Yet another Super Star Trek port/variant.

# What's this?
A C++ port of the "Super Star Trek" text game, as posted in David H. Ahl's "Basic Computer Games: Microcomputer Edition".  No features have been added or removed.

This port was created to maintain the original logic of that version, and leave a change history in case any of it was changed accidentally.  To that end, [oldbasic2cpp](https://github.com/SmileTheory/oldbasic2cpp) was used to mechanically convert the original code to C++, then the code was edited to resemble the old BASIC code as much as possible.

If you want a fully featured and deep Star Trek game, try Tom Almy's C port of UT Super Star Trek [here](http://www.almy.us/sst.html), or Eric S. Raymond's Python port of same [here](http://www.catb.org/~esr/super-star-trek/).

If you want a clean, readable version of Super Star Trek, try Chris Nystrom's C port [here](http://www.cactus.org/~nystrom/startrek.html).

# How do I use it?
For now, you'll have to compile it manually.

From a shell:

`g++ yatrek.cpp -o yatrek`

Then simply:

`./yatrek`

# Sources
Original BASIC source code (superstartrek.bas, superstartrekins.bas) retrieved from [Vintage Basic](http://www.vintage-basic.net/games.html).

Comparison code (startrek.txt) retrieved from [Pete Turnbull's Star Trek game ports page](http://www.dunnington.info/public/startrek/)
