/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2017 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <cassert>

#include "bitboard.h"
#include "pawns.h"
#include "position.h"
#include "thread.h"

namespace {

  #define V Value
  #define S(mg, eg) make_score(mg, eg)

  // Isolated pawn penalty by opposed flag
  const Score Isolated[VARIANT_NB][2] = {
    { S(45, 40), S(30, 27) },
#ifdef ANTI
    { S(50, 80), S(54, 69) },
#endif
#ifdef ATOMIC
    { S(27, 28), S(24, 14) },
#endif
#ifdef CRAZYHOUSE
    { S(45, 40), S(30, 27) },
#endif
#ifdef HORDE
    { S(60, 44), S(18, 38) },
#endif
#ifdef KOTH
    { S(45, 40), S(30, 27) },
#endif
#ifdef LOSERS
    { S(50, 80), S(54, 69) },
#endif
#ifdef RACE
    {},
#endif
#ifdef RELAY
    { S(45, 40), S(30, 27) },
#endif
#ifdef THREECHECK
    { S(45, 40), S(30, 27) },
#endif
  };

  // Backward pawn penalty by opposed flag
  const Score Backward[VARIANT_NB][2] = {
    { S(56, 33), S(41, 19) },
#ifdef ANTI
    { S(64, 25), S(26, 50) },
#endif
#ifdef ATOMIC
    { S(48, 21), S(35, 15) },
#endif
#ifdef CRAZYHOUSE
    { S(56, 33), S(41, 19) },
#endif
#ifdef HORDE
    { S(48, 26), S(80, 15) },
#endif
#ifdef KOTH
    { S(56, 33), S(41, 19) },
#endif
#ifdef LOSERS
    { S(64, 25), S(26, 50) },
#endif
#ifdef RACE
    {},
#endif
#ifdef RELAY
    { S(56, 33), S(41, 19) },
#endif
#ifdef THREECHECK
    { S(56, 33), S(41, 19) },
#endif
  };

  // Unsupported pawn penalty for pawns which are neither isolated or backward
  const Score Unsupported[VARIANT_NB] = {
    S( 17,   8),
#ifdef ANTI
    S(-45, -48),
#endif
#ifdef ATOMIC
    S( 39,   0),
#endif
#ifdef CRAZYHOUSE
    S( 17,   8),
#endif
#ifdef HORDE
    S( 47,  50),
#endif
#ifdef KOTH
    S( 17,   8),
#endif
#ifdef LOSERS
    S(-45, -48),
#endif
#ifdef RACE
    S(  0,   0),
#endif
#ifdef RELAY
    S( 17,   8),
#endif
#ifdef THREECHECK
    S( 17,   8),
#endif
  };

  // Connected pawn bonus by opposed, phalanx, twice supported and rank
  Score Connected[VARIANT_NB][2][2][2][RANK_NB];

  // Doubled pawn penalty
  const Score Doubled[VARIANT_NB] = {
    S(18, 38),
#ifdef ANTI
    S( 4, 51),
#endif
#ifdef ATOMIC
    S( 0,  0),
#endif
#ifdef CRAZYHOUSE
    S(18, 38),
#endif
#ifdef HORDE
    S(10, 78),
#endif
#ifdef KOTH
    S(18, 38),
#endif
#ifdef LOSERS
    S( 4, 51),
#endif
#ifdef RACE
    S( 0,  0),
#endif
#ifdef RELAY
    S(18, 38),
#endif
#ifdef THREECHECK
    S(18, 38),
#endif
  };

  // Lever bonus by rank
  const Score Lever[RANK_NB] = {
    S( 0,  0), S( 0,  0), S(0, 0), S(0, 0),
    S(17, 16), S(33, 32), S(0, 0), S(0, 0)
  };

  // Weakness of our pawn shelter in front of the king by [distance from edge][rank].
  // RANK_1 = 0 is used for files where we have no pawns or our pawn is behind our king.
  const Value ShelterWeakness[VARIANT_NB][4][RANK_NB] = {
  {
    { V(100), V(20), V(10), V(46), V(82), V( 86), V( 98) },
    { V(116), V( 4), V(28), V(87), V(94), V(108), V(104) },
    { V(109), V( 1), V(59), V(87), V(62), V( 91), V(116) },
    { V( 75), V(12), V(43), V(59), V(90), V( 84), V(112) }
  },
#ifdef ANTI
  {},
#endif
#ifdef ATOMIC
  {
    { V( 88), V(34), V( 5), V(44), V( 89), V( 90), V( 94) },
    { V(116), V(61), V(-4), V(80), V( 95), V(101), V(104) },
    { V( 97), V(68), V(34), V(82), V( 62), V(104), V(110) },
    { V(103), V(44), V(44), V(77), V(103), V( 66), V(118) }
  },
#endif
#ifdef CRAZYHOUSE
  {
    { V(238), V( 6), V( 82), V(130), V(120), V(166), V(232) },
    { V(330), V( 0), V(120), V(184), V(186), V(184), V(192) },
    { V(196), V( 0), V(156), V(136), V(216), V(196), V(242) },
    { V(176), V(34), V(112), V(170), V(182), V(194), V(276) }
  },
#endif
#ifdef HORDE
  {
    { V(100), V(20), V(10), V(46), V(82), V( 86), V( 98) },
    { V(116), V( 4), V(28), V(87), V(94), V(108), V(104) },
    { V(109), V( 1), V(59), V(87), V(62), V( 91), V(116) },
    { V( 75), V(12), V(43), V(59), V(90), V( 84), V(112) }
  },
#endif
#ifdef KOTH
  {
    { V(100), V(20), V(10), V(46), V(82), V( 86), V( 98) },
    { V(116), V( 4), V(28), V(87), V(94), V(108), V(104) },
    { V(109), V( 1), V(59), V(87), V(62), V( 91), V(116) },
    { V( 75), V(12), V(43), V(59), V(90), V( 84), V(112) }
  },
#endif
#ifdef LOSERS
  {
    { V(100), V(20), V(10), V(46), V(82), V( 86), V( 98) },
    { V(116), V( 4), V(28), V(87), V(94), V(108), V(104) },
    { V(109), V( 1), V(59), V(87), V(62), V( 91), V(116) },
    { V( 75), V(12), V(43), V(59), V(90), V( 84), V(112) }
  },
#endif
#ifdef RACE
  {},
#endif
#ifdef RELAY
  {
    { V(100), V(20), V(10), V(46), V(82), V( 86), V( 98) },
    { V(116), V( 4), V(28), V(87), V(94), V(108), V(104) },
    { V(109), V( 1), V(59), V(87), V(62), V( 91), V(116) },
    { V( 75), V(12), V(43), V(59), V(90), V( 84), V(112) }
  },
#endif
#ifdef THREECHECK
  {
    { V(105), V( 1), V(22), V( 52), V(86), V( 89), V( 98) },
    { V(116), V( 3), V(55), V(109), V(81), V( 97), V( 99) },
    { V(121), V(23), V(69), V( 93), V(58), V( 88), V(112) },
    { V( 94), V(11), V(52), V( 67), V(90), V( 85), V(112) }
  },
#endif
  };

  // Danger of enemy pawns moving toward our king by [type][distance from edge][rank].
  // For the unopposed and unblocked cases, RANK_1 = 0 is used when opponent has no pawn
  // on the given file, or their pawn is behind our king.
  const Value StormDanger[VARIANT_NB][4][4][RANK_NB] = {
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  //BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  //Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  79), V(23), V( 1) },  //BlockedByPawn
      { V( 0),  V(   0), V( 148), V(27), V( 2) },
      { V( 0),  V(   0), V( 161), V(16), V( 1) },
      { V( 0),  V(   0), V( 171), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  //Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#ifdef ANTI
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  //BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  //Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  79), V(23), V( 1) },  //BlockedByPawn
      { V( 0),  V(   0), V( 148), V(27), V( 2) },
      { V( 0),  V(   0), V( 161), V(16), V( 1) },
      { V( 0),  V(   0), V( 171), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  //Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#endif
#ifdef ATOMIC
  {
    { { V(-25),  V(-332), V(-235), V( 79), V( 41) },  //BlockedByKing
      { V(-17),  V(  35), V( 206), V(-21), V(-11) },
      { V(-31),  V(  52), V( 103), V( 42), V( 94) },
      { V( -5),  V( 101), V(  67), V( 29), V( 64) } },
    { { V(-47),  V(  62), V( 114), V( 16), V( 13) },  //Unopposed
      { V( 82),  V(  41), V( 161), V( 48), V( 35) },
      { V( 44),  V(  56), V( 115), V( 17), V( 48) },
      { V(189),  V( 112), V( 202), V( 69), V(186) } },
    { { V(  1),  V( -56), V(  70), V( -5), V(-42) },  //BlockedByPawn
      { V( -2),  V( -12), V( 145), V( 56), V( 24) },
      { V(-39),  V(  32), V(  98), V( 60), V( -1) },
      { V(-11),  V( -70), V( 194), V( 58), V(138) } },
    { { V( 27),  V(  -3), V(  91), V(105), V( 27) },  //Unblocked
      { V(128),  V( -27), V(  81), V( 59), V( 27) },
      { V(126),  V(  69), V(  69), V( 33), V(  1) },
      { V(115),  V(  -7), V( 204), V( 74), V( 70) } }
  },
#endif
#ifdef CRAZYHOUSE
  {
    { { V(-34),  V(-366), V(-249), V( 12), V( 80) },  //BlockedByKing
      { V( -6),  V( 122), V( 158), V( 90), V(  3) },
      { V( 35),  V(  89), V( 174), V( 87), V( 86) },
      { V(-77),  V(  17), V( 154), V( 82), V( 99) } },
    { { V( 71),  V(  67), V( 177), V( 49), V( 28) },  //Unopposed
      { V(-86),  V( 108), V( 104), V( 86), V( 26) },
      { V(  8),  V(  20), V( 107), V(137), V( 35) },
      { V(-95),  V(  69), V( 101), V(-10), V(-43) } },
    { { V( -8),  V(  75), V( 276), V( 14), V(-71) },  //BlockedByPawn
      { V(-28),  V( -10), V( 231), V(  8), V( -6) },
      { V( 59),  V( -14), V( 300), V( 26), V( -3) },
      { V(-81),  V(   2), V( 104), V( 79), V(-19) } },
    { { V( 73),  V(  78), V(  88), V( 46), V( 75) },  //Unblocked
      { V( 35),  V(  48), V( -21), V( 22), V(-52) },
      { V( 37),  V(  67), V( 122), V(  6), V( 64) },
      { V( -5),  V(  55), V( 101), V( 61), V( 33) } }
  },
#endif
#ifdef HORDE
  {
    { { V(-11),  V(-364), V(-337), V( 43), V( 69) },  //BlockedByKing
      { V(-24),  V(   2), V( 133), V(-33), V(-73) },
      { V(  9),  V(  72), V( 152), V( 99), V( 66) },
      { V( 71),  V(  18), V(  38), V( 30), V( 69) } },
    { { V( 18),  V( -11), V( 131), V( 42), V(114) },  //Unopposed
      { V( -4),  V(  63), V( -77), V( 62), V( 28) },
      { V( 66),  V(  82), V(  43), V( 11), V( 95) },
      { V(-12),  V(  45), V(  93), V(110), V( 78) } },
    { { V( 23),  V(   8), V(  86), V(-30), V(-15) },  //BlockedByPawn
      { V(105),  V(  35), V(  49), V( 78), V(-29) },
      { V(-74),  V( -27), V( 216), V( 25), V( 33) },
      { V(-14),  V(  24), V( 212), V( 80), V( -6) } },
    { { V(115),  V(  48), V( 103), V(-30), V( -9) },  //Unblocked
      { V( 67),  V(  66), V( 157), V( 38), V( 39) },
      { V( 87),  V(  48), V(  27), V(-21), V(-90) },
      { V( -7),  V(  24), V( 101), V( 90), V( 34) } }
  },
#endif
#ifdef KOTH
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  //BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  //Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  79), V(23), V( 1) },  //BlockedByPawn
      { V( 0),  V(   0), V( 148), V(27), V( 2) },
      { V( 0),  V(   0), V( 161), V(16), V( 1) },
      { V( 0),  V(   0), V( 171), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  //Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#endif
#ifdef LOSERS
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  //BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  //Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  79), V(23), V( 1) },  //BlockedByPawn
      { V( 0),  V(   0), V( 148), V(27), V( 2) },
      { V( 0),  V(   0), V( 161), V(16), V( 1) },
      { V( 0),  V(   0), V( 171), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  //Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#endif
#ifdef RACE
  {},
#endif
#ifdef RELAY
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  //BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  //Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  79), V(23), V( 1) },  //BlockedByPawn
      { V( 0),  V(   0), V( 148), V(27), V( 2) },
      { V( 0),  V(   0), V( 161), V(16), V( 1) },
      { V( 0),  V(   0), V( 171), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  //Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#endif
#ifdef THREECHECK
  {
    { { V(-40),  V(-310), V(-236), V( 86), V(107) },  //BlockedByKing
      { V( 24),  V(  80), V( 168), V( 38), V( -4) },
      { V( 16),  V( -41), V( 171), V( 63), V( 19) },
      { V( 12),  V(  80), V( 182), V( 36), V(-16) } },
    { { V( 27),  V( -18), V( 175), V( 31), V( 29) },  //Unopposed
      { V(106),  V(  81), V( 106), V( 86), V( 19) },
      { V( 42),  V(  62), V(  96), V( 84), V( 40) },
      { V(129),  V(  73), V( 124), V(103), V( 80) } },
    { { V(-15),  V(   9), V( -73), V(-15), V(-41) },  //BlockedByPawn
      { V(-28),  V(  28), V(  66), V( 25), V( -2) },
      { V(-38),  V( -30), V( 147), V( 24), V( 29) },
      { V(-30),  V(  39), V( 188), V(114), V( 63) } },
    { { V( 56),  V(  89), V(  34), V( -6), V(-54) },  //Unblocked
      { V( 80),  V( 123), V( 189), V( 83), V(-32) },
      { V( 89),  V(  26), V( 128), V(112), V( 78) },
      { V(166),  V(  29), V( 202), V( 18), V(109) } }
  },
#endif
  };

  // Max bonus for king safety. Corresponds to start position with all the pawns
  // in front of the king and no enemy pawn on the horizon.
  const Value MaxSafetyBonus = V(258);

#ifdef HORDE
  const Score ImbalancedHorde = S(30, 30);
#endif

  #undef S
  #undef V

  template<Color Us>
  Score evaluate(const Position& pos, Pawns::Entry* e) {

    const Color  Them  = (Us == WHITE ? BLACK      : WHITE);
    const Square Up    = (Us == WHITE ? NORTH      : SOUTH);
    const Square Right = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    const Square Left  = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    Bitboard b, neighbours, stoppers, doubled, supported, phalanx;
    Bitboard lever, leverPush, connected;
    Square s;
    bool opposed, backward;
    Score score = SCORE_ZERO;
    const Square* pl = pos.squares<PAWN>(Us);
    const Bitboard* pawnAttacksBB = StepAttacksBB[make_piece(Us, PAWN)];

    Bitboard ourPawns   = pos.pieces(Us  , PAWN);
    Bitboard theirPawns = pos.pieces(Them, PAWN);

    e->passedPawns[Us]   = e->pawnAttacksSpan[Us] = 0;
    e->semiopenFiles[Us] = 0xFF;
    e->kingSquares[Us]   = SQ_NONE;
    e->pawnAttacks[Us]   = shift<Right>(ourPawns) | shift<Left>(ourPawns);
    e->pawnsOnSquares[Us][BLACK] = popcount(ourPawns & DarkSquares);
    e->pawnsOnSquares[Us][WHITE] = pos.count<PAWN>(Us) - e->pawnsOnSquares[Us][BLACK];
#ifdef CRAZYHOUSE
    if (pos.is_house())
        e->pawnsOnSquares[Us][WHITE] = popcount(ourPawns & ~DarkSquares);
#endif

#ifdef HORDE
    if (pos.is_horde() && pos.is_horde_color(Us))
    {
        int l = 0, m = 0, r = popcount(ourPawns & FileBB[FILE_A]);
        for (File f1 = FILE_A; f1 <= FILE_H; ++f1)
        {
            l = m; m = r; r = f1 < FILE_H ? popcount(ourPawns & FileBB[f1 + 1]) : 0;
            score -= ImbalancedHorde * m / (1 + l * r);
        }
    }
#endif

    // Loop through all pawns of the current color and score each pawn
    while ((s = *pl++) != SQ_NONE)
    {
        assert(pos.piece_on(s) == make_piece(Us, PAWN));

        File f = file_of(s);

        e->semiopenFiles[Us]   &= ~(1 << f);
        e->pawnAttacksSpan[Us] |= pawn_attack_span(Us, s);

        // Flag the pawn
        opposed    = theirPawns & forward_bb(Us, s);
        stoppers   = theirPawns & passed_pawn_mask(Us, s);
        lever      = theirPawns & pawnAttacksBB[s];
        leverPush  = theirPawns & pawnAttacksBB[s + Up];
        doubled    = ourPawns   & (s - Up);
        neighbours = ourPawns   & adjacent_files_bb(f);
        phalanx    = neighbours & rank_bb(s);
#ifdef HORDE
        if (pos.is_horde() && rank_of(s) == RANK_1)
            supported = false;
        else
#endif
        supported  = neighbours & rank_bb(s - Up);
        connected  = supported | phalanx;

        // A pawn is backward when it is behind all pawns of the same color on the
        // adjacent files and cannot be safely advanced.
        if (!neighbours || lever || relative_rank(Us, s) >= RANK_5)
            backward = false;
        else
        {
            // Find the backmost rank with neighbours or stoppers
            b = rank_bb(backmost_sq(Us, neighbours | stoppers));

            // The pawn is backward when it cannot safely progress to that rank:
            // either there is a stopper in the way on this rank, or there is a
            // stopper on adjacent file which controls the way to that rank.
            backward = (b | shift<Up>(b & adjacent_files_bb(f))) & stoppers;

            assert(!backward || !(pawn_attack_span(Them, s + Up) & neighbours));
        }

        // Passed pawns will be properly scored in evaluation because we need
        // full attack info to evaluate them. Include also not passed pawns
        // which could become passed after one or two pawn pushes when are
        // not attacked more times than defended.
        if (   !(stoppers ^ lever ^ leverPush)
            && !(ourPawns & forward_bb(Us, s))
            && popcount(supported) >= popcount(lever)
            && popcount(phalanx)   >= popcount(leverPush))
            e->passedPawns[Us] |= s;

        // Score this pawn
        if (!neighbours)
            score -= Isolated[pos.variant()][opposed];

        else if (backward)
            score -= Backward[pos.variant()][opposed];

        else if (!supported)
            score -= Unsupported[pos.variant()];

#ifdef HORDE
        if (pos.is_horde() && relative_rank(Us, s) == 0) {} else
#endif
        if (connected)
            score += Connected[pos.variant()][opposed][!!phalanx][more_than_one(supported)][relative_rank(Us, s)];

        if (doubled && !supported)
            score -= Doubled[pos.variant()];

        if (lever)
            score += Lever[relative_rank(Us, s)];
    }

    return score;
  }

} // namespace

namespace Pawns {

/// Pawns::init() initializes some tables needed by evaluation. Instead of using
/// hard-coded tables, when makes sense, we prefer to calculate them with a formula
/// to reduce independent parameters and to allow easier tuning and better insight.

void init() {

  static const int Seed[VARIANT_NB][RANK_NB] = {
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#ifdef ANTI
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
#ifdef ATOMIC
    { 0,18, 11, 14, 82,109, 170, 315 },
#endif
#ifdef CRAZYHOUSE
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
#ifdef HORDE
    { 36, 28, 3, 1, 115, 107, 321, 332 },
#endif
#ifdef KOTH
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
#ifdef LOSERS
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
#ifdef RACE
    {},
#endif
#ifdef RELAY
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
#ifdef THREECHECK
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
  };

  for (Variant var = CHESS_VARIANT; var < VARIANT_NB; ++var)
  for (int opposed = 0; opposed <= 1; ++opposed)
      for (int phalanx = 0; phalanx <= 1; ++phalanx)
          for (int apex = 0; apex <= 1; ++apex)
              for (Rank r = RANK_2; r < RANK_8; ++r)
  {
      int v = (Seed[var][r] + (phalanx ? (Seed[var][r + 1] - Seed[var][r]) / 2 : 0)) >> opposed;
      v += (apex ? v / 2 : 0);
      Connected[var][opposed][phalanx][apex][r] = make_score(v, v * (r-2) / 4);
  }
}


/// Pawns::probe() looks up the current position's pawns configuration in
/// the pawns hash table. It returns a pointer to the Entry if the position
/// is found. Otherwise a new Entry is computed and stored there, so we don't
/// have to recompute all when the same pawns configuration occurs again.

Entry* probe(const Position& pos) {

  Key key = pos.pawn_key();
  Entry* e = pos.this_thread()->pawnsTable[key];

  if (e->key == key)
      return e;

  e->key = key;
  e->score = evaluate<WHITE>(pos, e) - evaluate<BLACK>(pos, e);
  e->asymmetry = popcount(e->semiopenFiles[WHITE] ^ e->semiopenFiles[BLACK]);
  e->openFiles = popcount(e->semiopenFiles[WHITE] & e->semiopenFiles[BLACK]);
  return e;
}


/// Entry::shelter_storm() calculates shelter and storm penalties for the file
/// the king is on, as well as the two closest files.

template<Color Us>
Value Entry::shelter_storm(const Position& pos, Square ksq) {

  const Color Them = (Us == WHITE ? BLACK : WHITE);

  enum { BlockedByKing, Unopposed, BlockedByPawn, Unblocked };

  Bitboard b = pos.pieces(PAWN) & (in_front_bb(Us, rank_of(ksq)) | rank_bb(ksq));
  Bitboard ourPawns = b & pos.pieces(Us);
  Bitboard theirPawns = b & pos.pieces(Them);
  Value safety = MaxSafetyBonus;
  File center = std::max(FILE_B, std::min(FILE_G, file_of(ksq)));

  for (File f = center - File(1); f <= center + File(1); ++f)
  {
      b = ourPawns & file_bb(f);
      Rank rkUs = b ? relative_rank(Us, backmost_sq(Us, b)) : RANK_1;

      b  = theirPawns & file_bb(f);
      Rank rkThem = b ? relative_rank(Us, frontmost_sq(Them, b)) : RANK_1;

      int d = std::min(f, FILE_H - f);
      safety -=  ShelterWeakness[pos.variant()][d][rkUs]
               + StormDanger[pos.variant()]
                 [f == file_of(ksq) && rkThem == relative_rank(Us, ksq) + 1 ? BlockedByKing  :
                  rkUs   == RANK_1                                          ? Unopposed :
                  rkThem == rkUs + 1                                        ? BlockedByPawn  : Unblocked]
                 [d][rkThem];
  }

  return safety;
}


/// Entry::do_king_safety() calculates a bonus for king safety. It is called only
/// when king square changes, which is about 20% of total king_safety() calls.

template<Color Us>
Score Entry::do_king_safety(const Position& pos, Square ksq) {

  kingSquares[Us] = ksq;
  castlingRights[Us] = pos.can_castle(Us);
  int minKingPawnDistance = 0;
#ifdef THREECHECK
  CheckCount checks = pos.is_three_check() ? pos.checks_given(~Us) : CHECKS_0;
#endif

  Bitboard pawns = pos.pieces(Us, PAWN);
  if (pawns)
      while (!(DistanceRingBB[ksq][minKingPawnDistance++] & pawns)) {}

  Value bonus = shelter_storm<Us>(pos, ksq);

  // If we can castle use the bonus after the castling if it is bigger
  if (pos.can_castle(MakeCastling<Us, KING_SIDE>::right))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_G1)));

  if (pos.can_castle(MakeCastling<Us, QUEEN_SIDE>::right))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_C1)));

#ifdef THREECHECK
  // Decrease score when checks have been taken
  return make_score(bonus, (-16 * minKingPawnDistance) + (-2 * checks));
#else
  return make_score(bonus, -16 * minKingPawnDistance);
#endif
}

// Explicit template instantiation
template Score Entry::do_king_safety<WHITE>(const Position& pos, Square ksq);
template Score Entry::do_king_safety<BLACK>(const Position& pos, Square ksq);

} // namespace Pawns
