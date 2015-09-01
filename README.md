# aperft
Chess perft analyzer that uses pre-compiled arrays of pseudo legal moves per piece type.

I am planning on writing several perft analyzers using various board representations and move generators.  This is the first in that series.

Before adding legality checking this approach would run at about 100-150 MLeafs per second on my i5 laptop.  Adding legality checking (and a check evasion generator) dropped that rate down to between 15 and 80 MLeafs per second.  So the pre-compiled list of pseudo legal moves is a decent way of generating moves, but a smarter approach is needed for legality checking.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh aperft

    80464 aperft.O1
     min KLeafs/sec = 18465.9
     max KLeafs/sec = 55578.4

    real  6m7.375s
    user  6m7.076s
    sys 0m0.060s

    83896 aperft.O2
     min KLeafs/sec = 20997.1
     max KLeafs/sec = 61469.6

    real  5m30.147s
    user  5m29.776s
    sys 0m0.096s

    84168 aperft.O3
     min KLeafs/sec = 19457.7
     max KLeafs/sec = 63422

    real  5m41.394s
    user  5m40.996s
    sys 0m0.100s

# bperft
This is `aperft` with directional slider attack tables.

Each square that is attacked by a slider knows which square the attacking slider(s) are on.  The attacker squares are stored according to the direction of their attack, this makes clearing/updating the from square in any direction very trivial.

It's slightly slower than `aperft` but the attack maps would be very useful in a real chess engine.  It's also possible that tweaking the move generation routines to better take advantage of the attack maps could bring `bperft` performance up to par with `aperft`, maybe even surpass it.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh bperft

    84936 bperft.O1
     min KLeafs/sec = 18956.7
     max KLeafs/sec = 50519.2

    real  6m48.395s
    user  6m47.928s
    sys 0m0.044s

    84384 bperft.O2
     min KLeafs/sec = 27894.6
     max KLeafs/sec = 59966.2

    real  5m16.572s
    user  5m16.220s
    sys 0m0.032s

    88928 bperft.O3
     min KLeafs/sec = 26469
     max KLeafs/sec = 59305.2

    real  5m27.083s
    user  5m26.724s
    sys 0m0.084s

# cperft
This is `aperft` with pre-calculated move tables replaced with pre-calculated farthest square per direction tables.  Pawn pushes and castling moves are not pre-calculated.

I also attempted to optimize the AttackedBy and GetCheckEvasions routines a bit.  I will apply the same optimizations to those same routines in `aperft`.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh cperft

    79576 cperft.O1
     min KLeafs/sec = 24934.3
     max KLeafs/sec = 70203.2

    real  4m55.266s
    user  4m55.532s
    sys 0m0.084s

    87192 cperft.O2
     min KLeafs/sec = 21511.7
     max KLeafs/sec = 76376.5

    real  4m45.614s
    user  4m45.896s
    sys 0m0.032s

    92360 cperft.O3
     min KLeafs/sec = 22073.4
     max KLeafs/sec = 79577.1

    real  4m42.969s
    user  4m43.244s
    sys 0m0.056s

# dperft
This is `cperft` with directional slider attack tables.

Each square that is attacked by a slider knows which square the attacking slider(s) are on.  The attacker squares are stored according to the direction of their attack, this makes clearing/updating the from square in any direction very trivial.

It's slightly slower than `cperft` but the attack maps would be very useful in a real chess engine.  It's also possible that tweaking the move generation routines to better take advantage of the attack maps could bring `dperft` performance up to par with `cperft`, maybe even surpass it.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh dperft

    88136 dperft.O1
     min KLeafs/sec = 24779.1
     max KLeafs/sec = 62000.9

    real  5m45.568s
    user  5m45.956s
    sys 0m0.108s

    87560 dperft.O2
     min KLeafs/sec = 22223.8
     max KLeafs/sec = 66060.9

    real  5m14.949s
    user  5m15.312s
    sys 0m0.040s

    96816 dperft.O3
     min KLeafs/sec = 29344.5
     max KLeafs/sec = 68249

    real  4m42.749s
    user  4m43.044s
    sys 0m0.068s

