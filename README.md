# aperft
Chess perft analyzer that uses pre-compiled arrays of pseudo legal moves per piece type.

I am planning on writing several perft analyzers using various board representations and move generators.  This is the first in that series.

Before adding legality checking this approach would run at about 100-150 MLeafs per second on my i5 laptop.  Adding legality checking (and a check evasion generator) dropped that rate down to between 15 and 80 MLeafs per second.  So the pre-compiled list of pseudo legal moves is a decent way of generating moves, but a smarter approach is needed for legality checking.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh aperft

    88096 aperft.O1
     min KLeafs/sec = 23293.3
     max KLeafs/sec = 60680

    real  4m57.278s
    user  4m57.568s
    sys   0m0.048s

    87776 aperft.O2
     min KLeafs/sec = 25449.5
     max KLeafs/sec = 67771.4

    real  4m41.584s
    user  4m41.896s
    sys   0m0.064s

    92352 aperft.O3
     min KLeafs/sec = 25026.8
     max KLeafs/sec = 66905.2

    real  4m41.127s
    user  4m41.380s
    sys   0m0.060s

# bperft
This is `aperft` with directional slider attack tables.

Each square that is attacked by a slider knows which square the attacking slider(s) are on.  The attacker squares are stored according to the direction of their attack, this makes clearing/updating the from square in any direction very trivial.

It's slightly slower than `aperft` but the attack maps would be very useful in a real chess engine.  It's also possible that tweaking the move generation routines to better take advantage of the attack maps could bring `bperft` performance up to par with `aperft`, maybe even surpass it.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh bperft

    85024 bperft.O1
     min KLeafs/sec = 19591.3
     max KLeafs/sec = 52363

    real  6m13.982s
    user  6m14.324s
    sys   0m0.088s

    84384 bperft.O2
     min KLeafs/sec = 26631.9
     max KLeafs/sec = 62561.6

    real  5m6.857s
    user  5m7.184s
    sys   0m0.068s

    88928 bperft.O3
     min KLeafs/sec = 24257.5
     max KLeafs/sec = 61004.9

    real  5m47.765s
    user  5m48.092s
    sys   0m0.052s

# cperft
This is `aperft` with pre-calculated move tables replaced with pre-calculated farthest square per direction tables.  Pawn pushes and castling moves are not pre-calculated.

I also attempted to optimize the AttackedBy and GetCheckEvasions routines a bit.  I will apply the same optimizations to those same routines in `aperft`.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh cperft

    79624 cperft.O1
     min KLeafs/sec = 21925
     max KLeafs/sec = 70690

    real  5m2.889s
    user  5m3.160s
    sys   0m0.064s

    83352 cperft.O2
     min KLeafs/sec = 24553.7
     max KLeafs/sec = 78322.2

    real  4m29.429s
    user  4m29.704s
    sys   0m0.024s

    96440 cperft.O3
     min KLeafs/sec = 26675.2
     max KLeafs/sec = 76466.9

    real  4m21.413s
    user  4m21.668s
    sys   0m0.036s

# dperft
This is `cperft` with directional slider attack tables.

Each square that is attacked by a slider knows which square the attacking slider(s) are on.  The attacker squares are stored according to the direction of their attack, this makes clearing/updating the from square in any direction very trivial.

It's slightly slower than `cperft` but the attack maps would be very useful in a real chess engine.  It's also possible that tweaking the move generation routines to better take advantage of the attack maps could bring `dperft` performance up to par with `cperft`, maybe even surpass it.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    $ ./test.sh dperft

    84040 dperft.O1
     min KLeafs/sec = 26340.5
     max KLeafs/sec = 64157.2

    real  5m19.746s
    user  5m20.040s
    sys   0m0.056s

    83464 dperft.O2
     min KLeafs/sec = 24069.8
     max KLeafs/sec = 66697.9

    real  5m18.551s
    user  5m18.848s
    sys   0m0.052s

    96872 dperft.O3
     min KLeafs/sec = 25773.4
     max KLeafs/sec = 67090.6

    real  4m43.968s
    user  4m44.240s
    sys   0m0.044s


# eperft
This is `dperft` with additional logic to update slider mobility maps.  This doesn't aid in move generation, it only provides extra information for use in a real chess engine.  And this perft program exists for the sole purpose of determining how much overhead is needed to keep the mobility maps updated.  It turns out that the bulk of the overhead is what was already in place for keeping the attack maps updated.  Adding the mobility maps is practically free.

It may be possible to use the slider mobility table for move generation.  But given that the mobility maps are organized by direction, resulting in gaps that must be traversed during move generation, I think it's cheaper to simply use the original slider movement maps that come from `dperft` because they don't have the gap problem.

Example test run on Intel® Core™ i5-5200U CPU @ 2.20GHz

    83760 eperft.O1
     min KLeafs/sec = 25827.8
     max KLeafs/sec = 63256.3

    real  5m5.586s
    user  5m5.736s
    sys   0m0.056s

    83296 eperft.O2
     min KLeafs/sec = 25947.5
     max KLeafs/sec = 67866.4

    real  5m4.986s
    user  5m5.148s
    sys   0m0.052s

    92400 eperft.O3
     min KLeafs/sec = 26642.7
     max KLeafs/sec = 66629.1

    real  5m10.241s
    user  5m10.392s
    sys   0m0.064s

