# aperft
Chess perft analyzer that uses pre-compiled arrays of pseudo legal moves per piece type.

I am planning on writing several perft analyzers using various board representations and move generators.  This is the first in that series.

Similar to this, one of my earliest chess engines used a table of pre-compiled moves per piece type.  That move generator maintained a list of legal moves that was updated whenever a move was made.  The idea was that only removing invalidated moves and adding newly legal moves would require less work than regenerating the complete move list every time.  That theory turned out to be resoundingly false - with my implementation anyway.  I would like to try that approach again some day, using a vastly different implementation.

Anyway, I was curious just how fast a perft analyzer that uses a pre-compiled move table could be.  Before adding legality checking this approach would run at about 100-150 KLeafs per second on my i5 laptop.  Adding legality checking (and a check evasion generator) dropped that rate down to between 15 and 40 KLeafs per second.  So the pre-compiled list of pseudo legal moves is a decent way of generating moves, but a smarter approach is needed for legality checking.

I may try improving this by adding bitboard attack tables for legality checking - or something like that.  But that will probably be done as a different project or a branch of this project.

# bperft
This is `aperft` with directional slider attack tables.

Each square that is attacked by a slider knows which square the attacking slider(s) are on.  The attacker squares are stored according to the direction of their attack, this makes clearing/updating the from square in any direction very trivial.

# cperft?
A table similar to the _atkd table in `bperft` coule be used for move generation.  When I have time I will make a version of `bperft` that uses this idea instead of the pre-calculated move arrays of `aperft`.  It would use far less memory and would simplify/optimize the move array loops considerably.

NOTE: the movegen arrays do not need to be structured accoring to move direction.  They only need to be a compact list of farthest 'to' squares.  And they could be used for king and knight moves as well as sliders.  I haven't decided yet whether it makes sense to use this approach for pawn moves (probbaly for pawn attacks, but not necessarily for pushes).

# dperft??
It make be worth trying out a `dperft` version of `cperft` that continuously updates the farthest 'to' square in each direction.  This would provide mobility information for a real chess engine.  That info would not be useful for a perft analyser but it would be useful to see how fast this approach would be compared to `cperft`.

