# aperft
Chess perft analyzer that uses pre-compiled arrays of pseudo legal moves per piece type.

I am planning on writing several perft analyzers using various board representations and move generators.  This is the first in that series.

Before adding legality checking this approach would run at about 100-150 MLeafs per second on my i5 laptop.  Adding legality checking (and a check evasion generator) dropped that rate down to between 15 and 80 MLeafs per second.  So the pre-compiled list of pseudo legal moves is a decent way of generating moves, but a smarter approach is needed for legality checking.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh aperft

    83864 aperft.O1
     min KLeafs/sec = 23079.2
     max KLeafs/sec = 60096.4

    real  5m15.405s
    user  5m15.748s
    sys   0m0.016s

    83456 aperft.O2
     min KLeafs/sec = 25244.9
     max KLeafs/sec = 67464.5

    real  4m33.247s
    user  4m33.520s
    sys   0m0.064s

    88208 aperft.O3
     min KLeafs/sec = 20711.3
     max KLeafs/sec = 68929.1

    real  4m57.895s
    user  4m58.172s
    sys   0m0.068s

# bperft
This is `aperft` with directional slider attack tables.

Each square that is attacked by a slider knows which square the attacking slider(s) are on.  The attacker squares are stored according to the direction of their attack, this makes clearing/updating the from square in any direction very trivial.

It's slightly slower than `aperft` but the attack maps would be very useful in a real chess engine.  It's also possible that tweaking the move generation routines to better take advantage of the attack maps could bring `bperft` performance up to par with `aperft`, maybe even surpass it.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh bperft

    88264 bperft.O1
     min KLeafs/sec = 24393.5
     max KLeafs/sec = 57594.6

    real  5m39.340s
    user  5m39.668s
    sys   0m0.056s

    83688 bperft.O2
     min KLeafs/sec = 27553.3
     max KLeafs/sec = 63153.1

    real  5m3.814s
    user  5m4.112s
    sys   0m0.052s

    92544 bperft.O3
     min KLeafs/sec = 26876.7
     max KLeafs/sec = 62340.1

    real  5m9.172s
    user  5m9.480s
    sys   0m0.040s

# cperft
This is `aperft` with pre-calculated move tables replaced with pre-calculated farthest square per direction tables.  Pawn pushes and castling moves are not pre-calculated.

I also attempted to optimize the AttackedBy and GetCheckEvasions routines a bit.  I will apply the same optimizations to those same routines in `aperft`.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh cperft

    75336 cperft.O1
     min KLeafs/sec = 24960.5
     max KLeafs/sec = 74125.8

    real  4m29.507s
    user  4m29.788s
    sys   0m0.032s

    83280 cperft.O2
     min KLeafs/sec = 22052.1
     max KLeafs/sec = 77975.3

    real  4m43.685s
    user  4m43.968s
    sys   0m0.036s

    92272 cperft.O3
     min KLeafs/sec = 22755.6
     max KLeafs/sec = 79122

    real  4m31.267s
    user  4m31.516s
    sys   0m0.060s

# dperft
This is `cperft` with directional slider attack tables.

Each square that is attacked by a slider knows which square the attacking slider(s) are on.  The attacker squares are stored according to the direction of their attack, this makes clearing/updating the from square in any direction very trivial.

It's slightly slower than `cperft` but the attack maps would be very useful in a real chess engine.  It's also possible that tweaking the move generation routines to better take advantage of the attack maps could bring `dperft` performance up to par with `cperft`, maybe even surpass it.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh dperft

    79632 dperft.O1
     min KLeafs/sec = 24719.7
     max KLeafs/sec = 70152.4

    real  4m55.769s
    user  4m56.060s
    sys   0m0.056s

    83352 dperft.O2
     min KLeafs/sec = 26921
     max KLeafs/sec = 71843.7

    real  5m2.335s
    user  5m2.628s
    sys   0m0.052s

    92320 dperft.O3
     min KLeafs/sec = 25909.8
     max KLeafs/sec = 69522.9

    real  4m55.951s
    user  4m56.268s
    sys   0m0.024s

# eperft
This is `dperft` with additional logic to update slider mobility maps.  This doesn't aid in move generation, it only provides extra information for use in a real chess engine.  And this perft program exists for the sole purpose of determining how much overhead is needed to keep the mobility maps updated.  It turns out that the bulk of the overhead is what was already in place for keeping the attack maps updated.  Adding the mobility maps is practically free.

It may be possible to use the slider mobility table for move generation.  But given that the mobility maps are organized by direction, resulting in gaps that must be traversed during move generation, I think it's cheaper to simply use the original slider movement maps that come from `dperft` because they don't have the gap problem.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    79664 eperft.O1
     min KLeafs/sec = 23795
     max KLeafs/sec = 68831.1

    real  5m24.630s
    user  5m24.956s
    sys   0m0.044s

    83384 eperft.O2
     min KLeafs/sec = 27456.8
     max KLeafs/sec = 70279.6

    real  4m55.777s
    user  4m56.048s
    sys   0m0.076s

    92352 eperft.O3
     min KLeafs/sec = 26744.6
     max KLeafs/sec = 68904.5

    real  4m55.269s
    user  4m55.548s
    sys   0m0.052s

