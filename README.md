# C++ implementation of Brent's Variation for Hashing

This is a toy/test implementation of hashing using Brent's Variation.

Brent's variation is also called Brent's method; but you'll find that "Brent's Method" is also (and more commonly) used for a root-finding algorithm; it's also used for a cycle-finding algorithm. Same author, different topics.  Lua uses Brent's variation, and I picked up the nomenclature for the comments in their source code.

The paper describing the method is "Reducing the Retrieval Time of Scatter Storage Techniques" [[brent1973]]. It has complete details and justification.

Brent's variation differs from classic scatter table algorithms in how it handles insertions. Lookup is the same as in the classic "linear quotient" method [[brent1973], ref 2]. Deletion is handled with a variety of heuristics.

To quote Brent:

> The idea of our method . . . is to take more care than usual when
keys are inserted, in an attempt to reduce the number of
probes required for subsequent lookups.

In particular, if the new entry would be more than three probes away from its ideal location, the algorithm does extra work to try to put the new entry closer to its ideal location, by choosing another entry to move out of the way. This choice requires a bit of work, because the algorithm wants to make sure that the moved entry is *also* no more than three probes away from its ideal location.

For an introduction to Brent's variation, see (for example) [[kube2011]].

[brent1973]: https://maths-people.anu.edu.au/~brent/pd/rpb013.pdf
[kube2011]: https://cseweb.ucsd.edu/~kube/cls/100/Lectures/lec17.brentsordered/lec17.pdf
[scoop.sh]: https://scoop.sh

## This Program

The easiest way to run this program is to install clang on your system. I use [[scoop.sh]] for this on Windows, or `apt` on Ubuntu. (You can also use Visual C++, which I also use, but I figure VC++ users will know what to do from the command line.)

With clang, just say:

```bash
clang -o brent.exe brent.cpp
./brent.exe
```

The output will look like this:

```console
$ ./brent
brent hashing test
nCall: 127 nProbe: 511 nDeleteTry:0 nDeleteProbe: 0 nDeleteMove: 0 nRelocTry: 35 nRelocProbe: 267 nRelocMove: 33
done with inserts
nCall: 127 nProbe: 286 nDeleteTry:0 nDeleteProbe: 0 nDeleteMove: 0 nRelocTry: 0 nRelocProbe: 0 nRelocMove: 0
done with lookups
nCall: 127 nProbe: 151 nDeleteTry:28 nDeleteProbe: 135 nDeleteMove: 28 nRelocTry: 0 nRelocProbe: 0 nRelocMove: 0
done with deletes
$
```

You'll see there's a simple test (which could be greatly extended, but I had pressing other things to look at). The test does insertions, lookups, and deletes.

You can readily change the size of the table or change the test key sequence by editing the code and rebuilding. Of course, it would be more elegant to have command line arguments; see "more pressing things".

## License

This code is released under the MIT license, and is copyright by MCCI Corporation.
