//----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//----------------------------------------------------------------------------
#include <algorithm>
#include <iostream>
#include <string>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include "Windows.h"
#else
#include <sys/time.h>
#endif

//-----------------------------------------------------------------------------
#define COLOR(x)      static_cast<Color>((x) & 1)
#define IS_COLOR(x)   (((x) == Black) || ((x) == White))
#define IS_SQUARE(x)  (((x) >= A1) && ((x) <= H8))
#define IS_PTYPE(x)   (((x) >= (White|Pawn)) && ((x) <= (Black|King)))
#define IS_PAWN(x)    ((Black|(x)) == (Black|Pawn))
#define IS_KING(x)    ((Black|(x)) == (Black|King))
#define IS_CAP(x)     (((x) >= (White|Pawn)) && ((x) <= (Black|Queen)))
#define IS_PROMO(x)   (((x) >= (White|Knight)) && ((x) <= (Black|Queen)))
#define IS_SLIDER(x)  (((x) >= (White|Bishop)) && (((x) <= (Black|Queen))))
#define IS_MTYPE(x)   (((x) >= PawnMove) && ((x) <= CastleLong))
#define IS_COORD(x)   (((x) >= 0) && ((x) < 8))
#define IS_X(x)       (((x) >= 'a') && ((x) <= 'h'))
#define IS_Y(x)       (((x) >= '1') && ((x) <= '8'))
#define TO_X(x)       ((x) - 'a')
#define TO_Y(x)       ((x) - '1')
#define SQR(x,y)      ((x) + ((y) * 8))
#define XC(x)         ((x) & 7)
#define YC(x)         ((x) / 8)
#define IS_DIR(x)     (IS_DIAG(x) || IS_CROSS(x))
#define IS_DIAG(x)    (((x) == SouthWest) || \
                       ((x) == SouthEast) || \
                       ((x) == NorthWest) || \
                       ((x) == NorthEast))
#define IS_CROSS(x)   (((x) == South) || \
                       ((x) == West) || \
                       ((x) == East) || \
                       ((x) == North))

//-----------------------------------------------------------------------------
typedef bool Color;

//-----------------------------------------------------------------------------
enum Limits {
  MaxPlies     = 100,
  MoveListSize = 128
};

//-----------------------------------------------------------------------------
enum Square {
  A1,B1,C1,D1,E1,F1,G1,H1, // 00,01,02,03,04,05,06,07
  A2,B2,C2,D2,E2,F2,G2,H2, // 08,09,10,11,12,13,14,15
  A3,B3,C3,D3,E3,F3,G3,H3, // 16,17,18,19,20,21,22,23
  A4,B4,C4,D4,E4,F4,G4,H4, // 24,25,26,27,28,29,30,31
  A5,B5,C5,D5,E5,F5,G5,H5, // 32,33,34,35,36,37,38,39
  A6,B6,C6,D6,E6,F6,G6,H6, // 40,41,42,43,44,45,46,47
  A7,B7,C7,D7,E7,F7,G7,H7, // 48,49,50,51,52,53,54,55
  A8,B8,C8,D8,E8,F8,G8,H8  // 56,57,58,59,60,61,62,63
};

//-----------------------------------------------------------------------------
enum Direction {
  SouthWest = -9,
  South     = -8,
  SouthEast = -7,
  West      = -1,
  East      =  1,
  NorthWest =  7,
  North     =  8,
  NorthEast =  9
};

//-----------------------------------------------------------------------------
enum ColorType {
  White,
  Black
};

//-----------------------------------------------------------------------------
enum PieceType {
  Pawn   = 2,
  Knight = 4,
  Bishop = 6,
  Rook   = 8,
  Queen  = 10,
  King   = 12
};

//-----------------------------------------------------------------------------
enum MoveType {
  NoMove,
  PawnMove,
  PawnLung,
  PawnCap,
  KnightMove = Knight,
  BishopMove = Bishop,
  RookMove   = Rook,
  QueenMove  = Queen,
  KingMove,
  CastleShort,
  CastleLong
};

//-----------------------------------------------------------------------------
enum CastleRights {
  WhiteShort  = 0x02,
  WhiteLong   = 0x04,
  WhiteCastle = 0x06,
  BlackShort  = 0x08,
  BlackLong   = 0x10,
  BlackCastle = 0x18
};

//-----------------------------------------------------------------------------
enum Masks {
  FourBits   = 0xF,
  SixBits    = 0x3F,
  TwentyBits = 0xFFFFF
};

//-----------------------------------------------------------------------------
enum Shifts {
  FromShift  = 4,
  ToShift    = 4+6,
  CapShift   = 4+6+6,
  PromoShift = 4+6+6+4
};

//-----------------------------------------------------------------------------
const int _TOUCH[64] = {
  ~WhiteLong, ~0, ~0, ~0, ~WhiteCastle, ~0, ~0, ~WhiteShort,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,
  ~BlackLong, ~0, ~0, ~0, ~BlackCastle, ~0, ~0, ~BlackShort
};

//-----------------------------------------------------------------------------
char _dist[64][64] = {0};
char _dir[64][64] = {0};
char _board[64] = {0};
char _king[2] = {0};
char _pieceCount[2] = {0};
char _diagSliders[2] = {0};
char _crossSliders[2] = {0};

//--------------------------------------------------------------------------
void InitDistDir() {
  memset(_dir, 0, sizeof(_dir));
  memset(_dist, 0, sizeof(_dist));
  for (int a = 0; a < 64; ++a) {
    for (int b = 0; b < 64; ++b) {
      const int x1 = XC(a);
      const int y1 = YC(a);
      const int x2 = XC(b);
      const int y2 = YC(b);
      const int d1 = abs(x1 - x2);
      const int d2 = abs(y1 - y2);
      _dist[a][b]  = std::max<int>(d1, d2);
      if (x1 == x2) {
        if (y1 > y2) {
          _dir[a][b] = South;
        }
        else if (y1 < y2) {
          _dir[a][b] = North;
        }
      }
      else if (y1 == y2) {
        if (x1 > x2) {
          _dir[a][b] = West;
        }
        else if (x1 < x2) {
          _dir[a][b] = East;
        }
      }
      else if (x1 > x2) {
        if (y1 > y2) {
          if (((SQR(x1, y1) - SQR(x2, y2)) % 9) == 0) {
            _dir[a][b] = SouthWest;
          }
        }
        else if (y1 < y2) {
          if (((SQR(x2, y2) - SQR(x1, y1)) % 7) == 0) {
            _dir[a][b] = NorthWest;
          }
        }
      }
      else if (x1 < x2) {
        if (y1 > y2) {
          if (((SQR(x1, y1) - SQR(x2, y2)) % 7) == 0) {
            _dir[a][b] = SouthEast;
          }
        }
        else if (y1 < y2) {
          if (((SQR(x2, y2) - SQR(x1, y1)) % 9) == 0) {
            _dir[a][b] = NorthEast;
          }
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
inline int Distance(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  return _dist[from][to];
}

//-----------------------------------------------------------------------------
inline int Direction(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  return _dir[from][to];
}

//-----------------------------------------------------------------------------
uint64_t _pawnCaps[64][2] = {0};
uint64_t _knightMoves[64] = {0};
uint64_t _bishopMoves[64] = {0};
uint64_t _rookMoves[64] = {0};
uint64_t _queenMoves[64] = {0};
uint64_t _kingMoves[64] = {0};

//-----------------------------------------------------------------------------
bool VerifyMoveMap(const int from, uint64_t map) {
  assert(IS_SQUARE(from));
  int sqrs[64] = {0};
  int sqr;
  sqrs[from] = 1;
  while (map) {
    if (!(map & 0xFF)) {
      return false;
    }
    sqr = ((map & 0xFF) - 1);
    if (!IS_SQUARE(sqr)) {
      return false;
    }
    if (sqrs[sqr]) {
      return false;
    }
    sqrs[sqr] = 1;
    map >>= 8;
  }
  return true;
}

//-----------------------------------------------------------------------------
bool GetToSquare(const int from, const int dir[2], int& to) {
  assert(IS_SQUARE(from));
  assert(dir[0] || dir[1]);
  const int x = (XC(from) + dir[0]);
  const int y = (YC(from) + dir[1]);
  to = SQR(x, y);
  return (IS_SQUARE(to) && (to != from) && IS_COORD(x) && IS_COORD(y));
}

//-----------------------------------------------------------------------------
template<Color color>
void InitPawnMoves(const int from) {
  assert(IS_SQUARE(from));
  uint64_t mvs = 0ULL;
  int shift = 0;
  int to = 0;
  if (color ? (YC(from) > 0) : (YC(from) < 7)) {
    if (XC(from) > 0) {
      assert(shift <= 56);
      to = (from + (color ? SouthWest : NorthWest));
      mvs |= (uint64_t(to + 1) << shift);
      shift += 8;
    }
    if (XC(from) < 7) {
      assert(shift <= 56);
      to = (from + (color ? SouthEast : NorthEast));
      mvs |= (uint64_t(to + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _pawnCaps[from][color] = mvs;
}

//-----------------------------------------------------------------------------
void InitKnightMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[8][2] = {
    { -1, -2 },
    {  1, -2 },
    { -2, -1 },
    {  2, -1 },
    { -2,  1 },
    {  2,  1 },
    { -1,  2 },
    {  1,  2 }
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  int to = 0;
  for (int i = 0; i < 8; ++i) {
    if (GetToSquare(from, dir[i], to)) {
      assert(shift <= 56);
      mvs |= (uint64_t(to + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _knightMoves[from] = mvs;
}

//-----------------------------------------------------------------------------
void InitBishopMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[4][2] = {
    { -1, -1 },
    {  1, -1 },
    { -1,  1 },
    {  1,  1 }
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  int to = 0;
  for (int i = 0; i < 4; ++i) {
    int end = -1;
    for (int frm = from; GetToSquare(frm, dir[i], to); frm = to) {
      end = to;
    }
    if (end != -1) {
      assert(IS_SQUARE(end));
      assert(shift <= 56);
      assert(IS_DIAG(Direction(from, end)));
      mvs |= (uint64_t(end + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _bishopMoves[from] = mvs;
}

//-----------------------------------------------------------------------------
void InitRookMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[4][2] = {
    {  0, -1 },
    { -1,  0 },
    {  1,  0 },
    {  0,  1 }
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  int to = 0;
  for (int i = 0; i < 4; ++i) {
    int end = -1;
    for (int frm = from; GetToSquare(frm, dir[i], to); frm = to) {
      end = to;
    }
    if (end != -1) {
      assert(IS_SQUARE(end));
      assert(shift <= 56);
      assert(IS_CROSS(Direction(from, end)));
      mvs |= (uint64_t(end + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _rookMoves[from] = mvs;
}

//-----------------------------------------------------------------------------
void InitQueenMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[8][2] = {
    { -1, -1 },
    {  0, -1 },
    {  1, -1 },
    { -1,  0 },
    {  1,  0 },
    { -1,  1 },
    {  0,  1 },
    {  1,  1 }
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  int to = 0;
  for (int i = 0; i < 8; ++i) {
    int end = -1;
    for (int frm = from; GetToSquare(frm, dir[i], to); frm = to) {
      end = to;
    }
    if (end != -1) {
      assert(IS_SQUARE(end));
      assert(shift <= 56);
      assert(IS_DIR(Direction(from, end)));
      mvs |= (uint64_t(end + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _queenMoves[from] = mvs;
}

//-----------------------------------------------------------------------------
void InitKingMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[8][2] = {
    { -1, -1 },
    {  0, -1 },
    {  1, -1 },
    { -1,  0 },
    {  1,  0 },
    { -1,  1 },
    {  0,  1 },
    {  1,  1 }
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  int to = 0;
  for (int i = 0; i < 8; ++i) {
    if (GetToSquare(from, dir[i], to)) {
      assert(shift <= 56);
      mvs |= (uint64_t(to + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _kingMoves[from] = mvs;
}

//-----------------------------------------------------------------------------
void InitMoveMaps() {
  memset(_pawnCaps, 0, sizeof(_pawnCaps));
  memset(_knightMoves, 0, sizeof(_knightMoves));
  memset(_bishopMoves, 0, sizeof(_bishopMoves));
  memset(_rookMoves, 0, sizeof(_rookMoves));
  memset(_queenMoves, 0, sizeof(_queenMoves));
  memset(_kingMoves, 0, sizeof(_kingMoves));
  for (int sqr = A1; sqr <= H8; ++sqr) {
    InitPawnMoves<White>(sqr);
    InitPawnMoves<Black>(sqr);
    InitKnightMoves(sqr);
    InitBishopMoves(sqr);
    InitRookMoves(sqr);
    InitQueenMoves(sqr);
    InitKingMoves(sqr);
  }
}

//-----------------------------------------------------------------------------
uint64_t _atkd[64];
const char _atkShift[19] = {
   0, // (-9) SouthWest
   8, // (-8) South
  16, // (-7) SouthEast
  -1, // (-6)
  -1, // (-5)
  -1, // (-4)
  -1, // (-3)
  -1, // (-2)
  24, // (-1) West
  -1, // ( 0)
  32, // ( 1) East
  -1, // ( 2)
  -1, // ( 3)
  -1, // ( 4)
  -1, // ( 5)
  -1, // ( 6)
  40, // ( 7) NorthEast
  48, // ( 8) North
  56  // ( 9) NorthEast
};

//-----------------------------------------------------------------------------
inline int AttackShift(const int dir) {
  assert(IS_DIR(dir));
  assert(_atkShift[dir + 9] >= 0);
  assert(_atkShift[dir + 9] < 64);
  assert(!(_atkShift[dir + 9] % 8));
  return _atkShift[dir + 9];
}

//-----------------------------------------------------------------------------
inline void AddAttack(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  assert(from != to);
  assert(IS_SLIDER(_board[from]));
  const int shift = AttackShift(Direction(from, to));
  assert(!(_atkd[to] & (0xFFULL << shift)) ||
         ((_atkd[to] & (0xFFULL << shift)) == (uint64_t(from + 1) << shift)));
  _atkd[to] |= (uint64_t(from + 1) << shift);
}

//-----------------------------------------------------------------------------
inline void ClearAttack(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  assert(from != to);
  assert(IS_SLIDER(_board[from]));
  _atkd[to] &= ~(0xFFULL << AttackShift(Direction(from, to)));
}

//-----------------------------------------------------------------------------
void AddAttacksFrom(const int pc, const int from) {
  assert(IS_SLIDER(pc));
  assert(IS_SQUARE(from));
  assert(_board[from] == pc);
  uint64_t mvs = (pc < Rook)  ? _bishopMoves[from] :
                 (pc < Queen) ? _rookMoves[from]
                              : _queenMoves[from];
  for (; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = Direction(from, end);
    assert(IS_DIR(dir));
    for (int to = (from + dir);; to += dir) {
      assert(IS_SQUARE(to));
      assert(Direction(from, to) == dir);
      AddAttack(from, to);
      if ((to == end) || _board[to]) {
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------------
void ClearAttacksFrom(const int pc, const int from) {
  assert(IS_SLIDER(pc));
  assert(IS_SQUARE(from));
  assert(_board[from] == pc);
  uint64_t mvs = (pc < Rook)  ? _bishopMoves[from] :
                 (pc < Queen) ? _rookMoves[from]
                              : _queenMoves[from];
  for (; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = Direction(from, end);
    assert(IS_DIR(dir));
    for (int to = (from + dir);; to += dir) {
      assert(IS_SQUARE(to));
      assert(Direction(from, to) == dir);
      ClearAttack(from, to);
      if ((to == end) || _board[to]) {
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------------
void TruncateAttacks(const int to, const int stop) {
  assert(IS_SQUARE(to));
  for (uint64_t tmp = _atkd[to]; tmp; tmp >>= 8) {
    if (tmp & 0xFF) {
      const int from = static_cast<int>((tmp & 0xFF) - 1);
      assert(IS_SQUARE(from));
      assert(IS_SLIDER(_board[from]));
      const int dir = Direction(from, to);
      for (int ato = (to + dir); IS_SQUARE(ato); ato += dir) {
        if (Direction(from, ato) != dir) {
          break;
        }
        ClearAttack(from, ato);
        if ((ato == stop) || _board[ato]) {
          break;
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
void ExtendAttacks(const int to) {
  assert(IS_SQUARE(to));
  assert(!_board[to]);
  for (uint64_t tmp = _atkd[to]; tmp; tmp >>= 8) {
    if (tmp & 0xFF) {
      const int from = static_cast<int>((tmp & 0xFF) - 1);
      assert(IS_SQUARE(from));
      assert(IS_SLIDER(_board[from]));
      const int dir = Direction(from, to);
      assert(IS_DIR(dir));
      for (int ato = (to + dir); IS_SQUARE(ato); ato += dir) {
        if (Direction(from, ato) != dir) {
          break;
        }
        AddAttack(from, ato);
        if (_board[ato]) {
          break;
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
uint64_t Now() {
#ifdef WIN32
  return static_cast<uint64_t>(GetTickCount());
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return static_cast<uint64_t>((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
}

//-----------------------------------------------------------------------------
std::string SquareStr(const int sqr) {
  assert(IS_SQUARE(sqr));
  char sbuf[4];
  sbuf[0] = ('a' + XC(sqr));
  sbuf[1] = ('1' + YC(sqr));
  sbuf[2] = 0;
  return std::string(sbuf);
}

//-----------------------------------------------------------------------------
class Move {
public:
  Move() : desc(NoMove) { }
  Move(const Move& other) : desc(other.desc) { Validate(); }
  Move& operator=(const Move& other) {
    other.Validate();
    desc = other.desc;
    return *this;
  }
  Move& Set(const int type, const int from, const int to,
            const int cap = 0, const int promo = 0)
  {
    desc = (type                 |
           (from  <<  FromShift) |
           (to    <<    ToShift) |
           (cap   <<   CapShift) |
           (promo << PromoShift));
    assert(Type() == type);
    assert(From() == from);
    assert(To() == to);
    assert(Cap() == cap);
    assert(Promo() == promo);
    Validate();
    return *this;
  }
  void Clear() { desc = NoMove; }
  bool operator!() const { return !desc; }
  bool operator==(const Move& other) const { return (desc == other.desc); }
  bool operator!=(const Move& other) const { return (desc != other.desc); }
  operator bool() const { return desc; }
  operator int() const { return desc; }
  bool Valid() const { return (Type() && (From() != To())); }
  int Type() const { return (desc & FourBits);  }
  int From() const { return ((desc >> FromShift) & SixBits); }
  int To() const { return ((desc >> ToShift) & SixBits); }
  int Cap() const { return ((desc >> CapShift) & FourBits); }
  int Promo() const { return ((desc >> PromoShift) & FourBits); }
  std::string ToString() const {
    char sbuf[6];
    sbuf[0] = ('a' + XC(From()));
    sbuf[1] = ('1' + YC(From()));
    sbuf[2] = ('a' + XC(To()));
    sbuf[3] = ('1' + YC(To()));
    switch (Black|Promo()) {
    case (Black|Knight): sbuf[4] = 'n'; sbuf[5] = 0; break;
    case (Black|Bishop): sbuf[4] = 'b'; sbuf[5] = 0; break;
    case (Black|Rook):   sbuf[4] = 'r'; sbuf[5] = 0; break;
    case (Black|Queen):  sbuf[4] = 'q'; sbuf[5] = 0; break;
    default:
      assert(!Promo());
      sbuf[4] = 0;
    }
    return std::string(sbuf);
  }
  bool operator<(const Move& other) const {
    return (strcmp(ToString().c_str(), other.ToString().c_str()) < 0);
  }
private:
  int desc;
  void Validate() const {
#ifndef NDEBUG
    if (desc) {
      assert(IS_MTYPE(Type()));
      assert(IS_SQUARE(From()));
      assert(IS_SQUARE(To()));
      assert(From() != To());
      assert(!Cap() || IS_CAP(Cap()));
      assert(!Promo() || IS_PROMO(Promo()));
    }
#endif
  }
};

//-----------------------------------------------------------------------------
const char* NextWord(const char* p) {
  while (p && *p && isspace(*p)) ++p;
  return p;
}

//-----------------------------------------------------------------------------
const char* NextSpace(const char* p) {
  while (p && *p && !isspace(*p)) ++p;
  return p;
}

//-----------------------------------------------------------------------------
class Node {
public:
  void SetParent(Node* val) {
    assert(val != this);
    parent = val;
  }
  void SetChild(Node* val) {
    assert(child != this);
    child = val;
  }
  void SetPly(const int val) {
    assert((ply >= 0) && (ply < MaxPlies));
    ply = val;
  }
  Node* Parent() const { return parent; }
  Node* Child() const { return child; }
  int Ply() const { return ply; }
  int ColorToMove() const { return (state & 1); }
  int EpSquare() const { return ep; }
  void Print() const {
    std::cout << std::endl;
    for (int y = 7; y >= 0; --y) {
      for (int x = 0; x < 8; ++x) {
        switch (_board[SQR(x, y)]) {
        case (White|Pawn):   std::cout << " P"; break;
        case (White|Knight): std::cout << " N"; break;
        case (White|Bishop): std::cout << " B"; break;
        case (White|Rook):   std::cout << " R"; break;
        case (White|Queen):  std::cout << " Q"; break;
        case (White|King):   std::cout << " K"; break;
        case (Black|Pawn):   std::cout << " p"; break;
        case (Black|Knight): std::cout << " n"; break;
        case (Black|Bishop): std::cout << " b"; break;
        case (Black|Rook):   std::cout << " r"; break;
        case (Black|Queen):  std::cout << " q"; break;
        case (Black|King):   std::cout << " k"; break;
        default:
          std::cout << (((x ^ y) & 1) ? " -" : "  ");
        }
      }
      switch (y) {
      case 7:
        std::cout << (ColorToMove() ? "  Black to move" : "  White to move");
        break;
      case 6:
        std::cout << "  Castling Rights   : ";
        if (state & WhiteShort) std::cout << 'K';
        if (state & WhiteLong)  std::cout << 'Q';
        if (state & BlackShort) std::cout << 'k';
        if (state & BlackLong)  std::cout << 'q';
        break;
      case 5:
        if (ep) {
          std::cout << "  En Passant Square : " << SquareStr(ep);
        }
        break;
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }
  template<Color color>
  bool AttackedBy(const int sqr) const {
    assert(IS_SQUARE(sqr));
    if (_atkd[sqr]) {
      for (uint64_t mvs = _atkd[sqr]; mvs; mvs >>= 8) {
        if (mvs & 0xFF) {
          assert(IS_SQUARE((mvs & 0xFF) - 1));
          assert(IS_SLIDER(_board[(mvs & 0xFF) - 1]));
          assert(IS_DIR(Direction(((mvs & 0xFF) - 1), sqr)));
          if (COLOR(_board[(mvs & 0xFF) - 1]) == color) {
            return true;
          }
        }
      }
    }
    for (uint64_t mvs = _knightMoves[sqr]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      assert(IS_SQUARE((mvs & 0xFF) - 1));
      assert(int((mvs & 0xFF) - 1) != sqr);
      if (_board[(mvs & 0xFF) - 1] == (color|Knight)) {
        return true;
      }
    }
    for (uint64_t mvs = _kingMoves[sqr]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int from = ((mvs & 0xFF) - 1);
      assert(IS_DIR(Direction(sqr, from)));
      switch (_board[from]) {
      case (color|Pawn):
        if (color) {
          switch (Direction(sqr, from)) {
          case NorthWest: case NorthEast:
            return true;
          }
        }
        else {
          switch (Direction(sqr, from)) {
          case SouthWest: case SouthEast:
            return true;
          }
        }
        break;
      case (color|King):
        return true;
      }
    }
    return false;
  }
  bool VerifyAttacksTo(const int to, const bool do_assert) const {
    uint64_t atk = 0;
    for (uint64_t mvs = _queenMoves[to]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int end = ((mvs & 0xFF) - 1);
      const int dir = Direction(end, to);
      assert(IS_DIR(dir));
      for (int from = (to - dir);; from -= dir) {
        assert(IS_SQUARE(from));
        assert(Direction(from, to) == dir);
        const int pc = _board[from];
        if (pc) {
          if (IS_SLIDER(pc) &&
              ((IS_DIAG(dir) && ((Black|pc) != (Black|Rook))) ||
               (IS_CROSS(dir) && ((Black|pc) != (Black|Bishop)))))
          {
            atk |= (uint64_t(from + 1) << AttackShift(dir));
          }
          break;
        }
        if (from == end) {
          break;
        }
      }
    }
    if (do_assert) {
      assert(atk == _atkd[to]);
    }
    return (atk == _atkd[to]);
  }
  bool VerifyAttacks(const bool do_assert) const {
    for (int sqr = A1; sqr <= H8; ++sqr) {
      if (!VerifyAttacksTo(sqr, do_assert)) {
        return false;
      }
    }
    return true;
  }
  bool LoadFEN(const char* fen) {
    memset(_atkd, 0, sizeof(_atkd));
    memset(_board, 0, sizeof(_board));
    memset(_pieceCount, 0, sizeof(_pieceCount));
    memset(_diagSliders, 0, sizeof(_diagSliders));
    memset(_crossSliders, 0, sizeof(_crossSliders));
    _king[White] = -1;
    _king[Black] = -1;
    state = 0;
    ep = 0;
    const char* p = fen;
    for (int y = 7; y >= 0; --y) {
      for (int x = 0; x < 8; ++x, ++p) {
        if (!*p) {
          std::cout << "Incomplete board position" << std::endl;
          return false;
        }
        else if (isdigit(*p)) {
          x += (*p - '1');
        }
        else {
          switch (*p) {
          case 'b':
            _board[SQR(x, y)] = (Black|Bishop);
            _pieceCount[Black]++;
            _diagSliders[Black]++;
            break;
          case 'B':
            _board[SQR(x, y)] = (White|Bishop);
            _pieceCount[White]++;
            _diagSliders[White]++;
            break;
          case 'k':
            _board[SQR(x, y)] = (Black|King);
            _king[Black] = SQR(x, y);
            _pieceCount[Black]++;
            break;
          case 'K':
            _board[SQR(x, y)] = (White|King);
            _king[White] = SQR(x, y);
            _pieceCount[White]++;
            break;
          case 'n':
            _board[SQR(x, y)] = (Black|Knight);
            _pieceCount[Black]++;
            break;
          case 'N':
            _board[SQR(x, y)] = (White|Knight);
            _pieceCount[White]++;
            break;
          case 'p':
            _board[SQR(x, y)] = (Black|Pawn);
            _pieceCount[Black]++;
            break;
          case 'P':
            _board[SQR(x, y)] = (White|Pawn);
            _pieceCount[White]++;
            break;
          case 'q':
            _board[SQR(x, y)] = (Black|Queen);
            _pieceCount[Black]++;
            _diagSliders[Black]++;
            _crossSliders[Black]++;
            break;
          case 'Q':
            _board[SQR(x, y)] = (White|Queen);
            _pieceCount[White]++;
            _diagSliders[White]++;
            _crossSliders[White]++;
            break;
          case 'r':
            _board[SQR(x, y)] = (Black|Rook);
            _pieceCount[Black]++;
            _crossSliders[Black]++;
            break;
          case 'R':
            _board[SQR(x, y)] = (White|Rook);
            _pieceCount[White]++;
            _crossSliders[White]++;
            break;
          default:
            std::cout << "Invalid character at " << p << std::endl;
            return false;
          }
        }
      }
      if (y > 0) {
        if (*p == '/') {
          ++p;
        }
        else if (*p) {
          std::cout << "Expected '/' at " << p << std::endl;
          return false;
        }
        else {
          std::cout << "Incomplete board position" << std::endl;
          return false;
        }
      }
    }
    if (!IS_SQUARE(_king[White])) {
      std::cout << "No white king in this position" << std::endl;
      return false;
    }
    if (!IS_SQUARE(_king[Black])) {
      std::cout << "No black king in this position" << std::endl;
      return false;
    }
    for (int sqr = 0; sqr < 64; ++sqr) {
      if (IS_SLIDER(_board[sqr])) {
        AddAttacksFrom(_board[sqr], sqr);
      }
    }
    assert(VerifyAttacks(true));
    p = NextSpace(p);
    p = NextWord(p);
    switch (*p) {
    case 'b':
      state |= Black;
      if (AttackedBy<Black>(_king[White])) {
        std::cout << "The king can be captured in this position" << std::endl;
        return false;
      }
      break;
    case 'w':
      state |= White;
      if (AttackedBy<White>(_king[Black])) {
        std::cout << "The king can be captured in this position" << std::endl;
        return false;
      }
      break;
    case 0:
      std::cout << "Missing side to move (w|b)" << std::endl;
      return false;
    default:
      std::cout << "Expected 'w' or 'b' at " << p << std::endl;
      return false;
    }
    p = NextSpace(p);
    p = NextWord(p);
    while (*p && !isspace(*p)) {
      switch (*p++) {
      case '-': break;
      case 'k': state |= BlackShort; continue;
      case 'K': state |= WhiteShort; continue;
      case 'q': state |= BlackLong;  continue;
      case 'Q': state |= WhiteLong;  continue;
      default:
        std::cout << "Unexpected castle rights at " << p << std::endl;
        return false;
      }
      break;
    }
    p = NextSpace(p);
    p = NextWord(p);
    if (IS_X(p[0]) && IS_Y(p[1])) {
      const int x = TO_X(*p++);
      const int y = TO_Y(*p++);
      ep = SQR(x, y);
    }
    return true;
  }
  template<Color color>
  void Exec(const Move& move, Node& dest) const {
    assert(ColorToMove() == color);
    assert(move.Valid());
    assert(!move.Cap() || IS_CAP(move.Cap()));
    assert(!move.Cap() || COLOR(move.Cap()) != color);
    assert(!move.Promo() || IS_PROMO(move.Promo()));
    assert(!move.Promo() || COLOR(move.Promo()) == color);
    assert(_board[move.To()] == move.Cap());
    const int from = move.From();
    const int to   = move.To();
    const int cap  = move.Cap();
    const int pc   = _board[from];
    if (IS_SLIDER(pc)) {
      ClearAttacksFrom(pc, from);
    }
    if (IS_SLIDER(cap)) {
      ClearAttacksFrom(cap, to);
    }
    switch (move.Type()) {
    case PawnMove:
      assert(pc == (color|Pawn));
      assert(!_board[to]);
      assert(!cap);
      _board[from] = 0;
      if (move.Promo()) {
        assert(IS_PROMO(move.Promo()));
        assert(COLOR(move.Promo()) == color);
        assert(YC(to) == (color ? 0 : 7));
        _board[to] = move.Promo();
      }
      else {
        assert(YC(to) != (color ? 0 : 7));
        _board[to] = (color|Pawn);
      }
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = 0;
      break;
    case PawnLung:
      assert(pc == (color|Pawn));
      assert(YC(from) == (color ? 6 : 1));
      assert(!_board[to]);
      assert(!_board[to + (color ? North : South)]);
      assert(!cap);
      assert(!move.Promo());
      _board[from] = 0;
      _board[to] = (color|Pawn);
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = (to + (color ? North : South));
      break;
    case PawnCap:
      assert(pc == (color|Pawn));
      _board[from] = 0;
      if (move.Promo()) {
        assert(IS_PROMO(move.Promo()));
        assert(COLOR(move.Promo()) == color);
        assert(YC(to) == (color ? 0 : 7));
        assert(cap);
        _board[to] = move.Promo();
      }
      else {
        assert(YC(to) != (color ? 0 : 7));
        _board[to] = (color|Pawn);
        if (!cap) {
          assert(ep && (to == ep));
          assert(_board[ep + (color ? North : South)] == ((!color)|Pawn));
          _board[ep + (color ? North : South)] = 0;
          _pieceCount[!color]--;
          assert(_pieceCount[!color] > 0);
          if (_atkd[ep + (color ? North : South)]) {
            ExtendAttacks(ep + (color ? North : South));
          }
        }
      }
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = 0;
      break;
    case KnightMove:
    case BishopMove:
    case RookMove:
    case QueenMove:
      assert(pc == (color|move.Type()));
      assert(_board[to] == cap);
      assert(!move.Promo());
      _board[from] = 0;
      _board[to] = pc;
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = 0;
      break;
    case KingMove:
      assert(pc == (color|King));
      assert(_king[color] == from);
      assert(!move.Promo());
      _board[from] = 0;
      _board[to] = (color|King);
      _king[color] = to;
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = 0;
      break;
    case CastleShort:
      assert(from == (color ? E8 : E1));
      assert(to == (color ? G8 : G1));
      assert(_king[color] == from);
      assert(!cap);
      assert(!move.Promo());
      assert(_board[color ? E8 : E1] == (color|King));
      assert(_board[color ? H8 : H1] == (color|Rook));
      assert(!_board[color ? F8 : F1]);
      assert(!_board[color ? G8 : G1]);
      assert(!AttackedBy<!color>(color ? E8 : E1));
      assert(!AttackedBy<!color>(color ? F8 : F1));
      assert(!AttackedBy<!color>(color ? G8 : G1));
      ClearAttacksFrom((color|Rook), (color ? H8 : H1));
      _board[color ? E8 : E1] = 0;
      _board[color ? F8 : F1] = (color|Rook);
      _board[color ? G8 : G1] = (color|King);
      _board[color ? H8 : H1] = 0;
      _king[color] = (color ? G8 : G1);
      AddAttacksFrom((color|Rook), (color ? F8 : F1));
      dest.state = ((state ^ 1) & ~(color ? BlackCastle : WhiteCastle));
      dest.ep = 0;
      break;
    case CastleLong:
      assert(from == (color ? E8 : E1));
      assert(to == (color ? C8 : C1));
      assert(_king[color] == from);
      assert(!cap);
      assert(!move.Promo());
      assert(_board[color ? E8 : E1] == (color|King));
      assert(_board[color ? A8 : A1] == (color|Rook));
      assert(!_board[color ? B8 : B1]);
      assert(!_board[color ? C8 : C1]);
      assert(!_board[color ? D8 : D1]);
      assert(!AttackedBy<!color>(color ? C8 : C1));
      assert(!AttackedBy<!color>(color ? D8 : D1));
      assert(!AttackedBy<!color>(color ? E8 : E1));
      ClearAttacksFrom((color|Rook), (color ? A8 : A1));
      _board[color ? A8 : A1] = 0;
      _board[color ? C8 : C1] = (color|King);
      _board[color ? D8 : D1] = (color|Rook);
      _board[color ? E8 : E1] = 0;
      _king[color] = (color ? C8 : C1);
      AddAttacksFrom((color|Rook), (color ? D8 : D1));
      dest.state = ((state ^ 1) & ~(color ? BlackCastle : WhiteCastle));
      dest.ep = 0;
      break;
    default:
      assert(false);
    }
    if (cap) {
      _pieceCount[!color]--;
      assert(_pieceCount[!color] > 0);
      switch (cap) {
      case ((!color)|Bishop):
        _diagSliders[!color]--;
        assert(_diagSliders[!color] >= 0);
        break;
      case ((!color)|Rook):
        _crossSliders[!color]--;
        assert(_crossSliders[!color] >= 0);
        break;
      case ((!color)|Queen):
        _diagSliders[!color]--;
        _crossSliders[!color]--;
        assert(_diagSliders[!color] >= 0);
        assert(_crossSliders[!color] >= 0);
        break;
      }
    }
    switch (move.Promo()) {
    case (color|Bishop):
      _diagSliders[color]++;
      assert(_diagSliders[color] < 12);
      break;
    case (color|Rook):
      _crossSliders[color]++;
      assert(_crossSliders[color] < 12);
      break;
    case (color|Queen):
      _diagSliders[color]++;
      _crossSliders[color]++;
      assert(_diagSliders[color] < 12);
      assert(_crossSliders[color] < 12);
      break;
    }
    if (!cap && _atkd[to]) {
      TruncateAttacks(to, from);
    }
    if (_atkd[from]) {
      ExtendAttacks(from);
    }
    if (IS_SLIDER(_board[to])) {
      AddAttacksFrom(_board[to], to);
    }
//    if (!VerifyAttacks(false)) {
//      std::cout << "EXEC " << move.ToString() << std::endl;
//      Print();
//    }
//    assert(VerifyAttacks(true));
  }
  template<Color color>
  void Undo(const Move& move) const {
    assert(ColorToMove() == color);
    assert(move.Valid());
    assert(!move.Cap() || IS_CAP(move.Cap()));
    assert(!move.Cap() || COLOR(move.Cap()) != color);
    assert(!move.Promo() || IS_PROMO(move.Promo()));
    assert(!move.Promo() || COLOR(move.Promo()) == color);
    assert(!_board[move.From()]);
    const int from = move.From();
    const int to   = move.To();
    const int cap  = move.Cap();
    const int pc   = _board[to];
    assert(pc);
    assert(COLOR(pc) == color);
    if (IS_SLIDER(pc)) {
      ClearAttacksFrom(pc, to);
    }
    switch (move.Type()) {
    case PawnMove:
    case PawnLung:
      assert(move.Promo() ? (pc == move.Promo()) : (pc == (color|Pawn)));
      assert(!cap);
      _board[from] = (color|Pawn);
      _board[to] = 0;
      break;
    case PawnCap:
      if (cap) {
        _board[to] = cap;
      }
      else {
        assert(pc == (color|Pawn));
        assert(ep && (to == ep));
        assert(!_board[ep + (color ? North : South)]);
        if (_atkd[ep + (color ? North : South)]) {
          TruncateAttacks(ep + (color ? North : South), -1);
        }
        _board[to] = 0;
        _board[ep + (color ? North : South)] = ((!color)|Pawn);
        _pieceCount[!color]++;
        assert(_pieceCount[!color] <= 16);
      }
      _board[from] = (color|Pawn);
      break;
    case KnightMove:
    case BishopMove:
    case RookMove:
    case QueenMove:
      assert(pc == (color|move.Type()));
      _board[from] = pc;
      _board[to] = cap;
      break;
    case KingMove:
      assert(pc == (color|King));
      assert(_king[color] == to);
      _board[from] = pc;
      _board[to] = cap;
      _king[color] = from;
      break;
    case CastleShort:
      assert(from == (color ? E8 : E1));
      assert(to == (color ? G8 : G1));
      assert(_king[color] == to);
      assert(pc == (color|King));
      assert(!cap);
      assert(!move.Promo());
      assert(!_board[color ? E8 : E1]);
      assert(!_board[color ? H8 : H1]);
      assert(_board[color ? F8 : F1] == (color|Rook));
      assert(_board[color ? G8 : G1] == (color|King));
      ClearAttacksFrom((color|Rook), (color ? F8 : F1));
      _board[color ? E8 : E1] = (color|King);
      _board[color ? F8 : F1] = 0;
      _board[color ? G8 : G1] = 0;
      _board[color ? H8 : H1] = (color|Rook);
      _king[color] = from;
      AddAttacksFrom((color|Rook), (color ? H8 : H1));
      break;
    case CastleLong:
      assert(from == (color ? E8 : E1));
      assert(to == (color ? C8 : C1));
      assert(_king[color] == to);
      assert(pc == (color|King));
      assert(!cap);
      assert(!move.Promo());
      assert(!_board[color ? E8 : E1]);
      assert(!_board[color ? A8 : A1]);
      assert(!_board[color ? B8 : B1]);
      assert(_board[color ? C8 : C1] == (color|King));
      assert(_board[color ? D8 : D1] == (color|Rook));
      ClearAttacksFrom((color|Rook), (color ? D8 : D1));
      _board[color ? A8 : A1] = (color|Rook);
      _board[color ? C8 : C1] = 0;
      _board[color ? D8 : D1] = 0;
      _board[color ? E8 : E1] = (color|King);
      _king[color] = from;
      AddAttacksFrom((color|Rook), (color ? A8 : A1));
      break;
    default:
      assert(false);
    }
    if (_atkd[from]) {
      TruncateAttacks(from, to);
    }
    if (cap) {
      _pieceCount[!color]++;
      assert(_pieceCount[!color] <= 16);
      switch (cap) {
      case ((!color)|Bishop):
        _diagSliders[!color]++;
        assert(_diagSliders[!color] < 12);
        AddAttacksFrom(cap, to);
        break;
      case ((!color)|Rook):
        _crossSliders[!color]++;
        assert(_crossSliders[!color] < 12);
        AddAttacksFrom(cap, to);
        break;
      case ((!color)|Queen):
        _diagSliders[!color]++;
        _crossSliders[!color]++;
        assert(_diagSliders[!color] < 12);
        assert(_crossSliders[!color] < 12);
        AddAttacksFrom(cap, to);
        break;
      }
    }
    else if (_atkd[to]) {
      ExtendAttacks(to);
    }
    if (!cap && (move.Type() == PawnCap) &&
        _atkd[ep + (color ? North : South)])
    {
      TruncateAttacks(ep + (color ? North : South), from);
    }
    if (IS_SLIDER(_board[from])) {
      AddAttacksFrom(_board[from], from);
    }
    switch (move.Promo()) {
    case (color|Bishop):
      _diagSliders[color]--;
      assert(_diagSliders[color] >= 0);
      break;
    case (color|Rook):
      _crossSliders[color]--;
      assert(_crossSliders[color] >= 0);
      break;
    case (color|Queen):
      _diagSliders[color]--;
      _crossSliders[color]--;
      assert(_diagSliders[color] >= 0);
      assert(_crossSliders[color] >= 0);
      break;
    }
//    if (!VerifyAttacks(false)) {
//      std::cout << "UNDO " << move.ToString() << std::endl;
//      Print();
//    }
//    assert(VerifyAttacks(true));
  }
  void AddMove(const Color color, const int type, const int from, const int to,
               const int cap = 0, const int promo = 0)
  {
    assert(IS_MTYPE(type));
    assert(IS_SQUARE(from));
    assert(IS_SQUARE(to));
    assert(from != to);
    assert(moveCount >= 0);
    assert(moveCount < MoveListSize);
    if (_diagSliders[!color] + _crossSliders[!color]) {
      if (pinDir[from] && (pinDir[from] != abs(Direction(from, to)))) {
        return;
      }
    }
    moves[moveCount++].Set(type, from, to, cap, promo);
  }
  template<Color color>
  void GetPawnMoves(const int from) {
    assert(IS_SQUARE(from));
    assert(_board[from] == (color|Pawn));
    int cap;
    int to;
    for (uint64_t mvs = _pawnCaps[from][color]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      to = ((mvs & 0xFF) - 1);
      assert(Distance(from, to) == 1);
      if (!(cap = _board[to])) {
        if (ep && (to == ep) &&
            !EpPinned<color>(from, (ep + (color ? North : South))))
        {
          AddMove(color, PawnCap, from, to);
        }
      }
      else if (COLOR(cap) != color) {
        if (YC(to) == (color ? 0 : 7)) {
          AddMove(color, PawnCap, from, to, cap, (color|Queen));
          AddMove(color, PawnCap, from, to, cap, (color|Rook));
          AddMove(color, PawnCap, from, to, cap, (color|Bishop));
          AddMove(color, PawnCap, from, to, cap, (color|Knight));
        }
        else {
          AddMove(color, PawnCap, from, to, cap);
        }
      }
    }
    to = (from + (color ? South : North));
    assert(IS_SQUARE(to));
    if (!_board[to]) {
      if (YC(to) == (color ? 0 : 7)) {
        AddMove(color, PawnMove, from, to, 0, (color|Queen));
        AddMove(color, PawnMove, from, to, 0, (color|Rook));
        AddMove(color, PawnMove, from, to, 0, (color|Bishop));
        AddMove(color, PawnMove, from, to, 0, (color|Knight));
      }
      else {
        AddMove(color, PawnMove, from, to);
        if (YC(from) == (color ? 6 : 1)) {
          to += (color ? South : North);
          assert(IS_SQUARE(to));
          if (!_board[to]) {
            AddMove(color, PawnLung, from, to);
          }
        }
      }
    }
  }
  void GetKnightMoves(const Color color, const int from) {
    assert(IS_SQUARE(from));
    assert(_board[from] == (color|Knight));
    for (uint64_t mvs = _knightMoves[from]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(IS_SQUARE(to));
      assert(to != from);
      const int cap = _board[to];
      if (!cap) {
        AddMove(color, KnightMove, from, to);
      }
      else if (COLOR(cap) != color) {
        AddMove(color, KnightMove, from, to, cap);
      }
    }
  }
  void GetSliderMoves(const Color color, const int type,
                      uint64_t mvs, const int from)
  {
    assert(IS_SQUARE(from));
    assert(_board[from] == (color|type));
    while (mvs) {
      assert(mvs & 0xFF);
      const int end = ((mvs & 0xFF) - 1);
      const int dir = (Direction(from, end));
      assert(IS_DIR(dir));
      for (int to = (from + dir);; to += dir) {
        assert(IS_SQUARE(to));
        assert(Direction(from, to) == dir);
        const int cap = _board[to];
        if (!cap) {
          AddMove(color, type, from, to);
        }
        else {
          if (COLOR(cap) != color) {
            AddMove(color, type, from, to, cap);
          }
          break;
        }
        if (to == end) {
          break;
        }
      }
      mvs >>= 8;
    }
  }
  template<Color color>
  void GetKingMoves(const int from) {
    assert(IS_SQUARE(from));
    assert(_board[from] == (color|King));
    assert(_king[color] == from);
    assert(!AttackedBy<!color>(from));
    for (uint64_t mvs = _kingMoves[from]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(Distance(from, to) == 1);
      if (!AttackedBy<!color>(to)) {
        const int cap = _board[to];
        if (!cap) {
          AddMove(color, KingMove, from, to);
          if ((to == (color ? F8 : F1)) &&
              (state & (color ? BlackShort : WhiteShort)) &&
              !_board[color ? G8 : G1] &&
              !AttackedBy<!color>(color ? G8 : G1))
          {
            assert(from == (color ? E8 : E1));
            assert(_board[color ? H8 : H1] == (color|Rook));
            AddMove(color, CastleShort, from, (color ? G8 : G1));
          }
          else if ((to == (color ? D8 : D1)) &&
                   (state & (color ? BlackLong : WhiteLong)) &&
                   !_board[color ? C8 : C1] &&
                   !_board[color ? B8 : B1] &&
                   !AttackedBy<!color>(color ? C8 : C1))
          {
            assert(from == (color ? E8 : E1));
            assert(_board[color ? A8 : A1] == (color|Rook));
            AddMove(color, CastleLong, from, (color ? C8 : C1));
          }
        }
        else if (COLOR(cap) != color) {
          AddMove(color, KingMove, from, to, cap);
        }
      }
    }
  }
  template<Color color>
  bool EpPinned(const int from, const int cap) const {
    assert(IS_SQUARE(_king[color]));
    assert(IS_SQUARE(from));
    assert(IS_SQUARE(cap));
    assert(from != cap);
    assert(from != _king[color]);
    assert(_board[from] == (color|Pawn));
    assert(_board[cap] == ((!color)|Pawn));
    assert(_board[_king[color]] == (color|King));
    if (!_crossSliders[!color]) {
      return false;
    }
    const int y = YC(from);
    assert(y == YC(cap));
    assert(y == (color ? 3 : 4));
    assert(Distance(from, cap) == 1);
    if (y != YC(_king[color])) {
      return false;
    }
    for (int pc, x = (XC(std::min<int>(from, cap)) - 1); x >= 0; --x) {
      if ((pc = _board[SQR(x, y)]) == (color|King)) {
        for (x = (XC(std::max<int>(from, cap)) + 1); x < 8; ++x) {
          if (((pc = _board[SQR(x, y)]) == ((!color)|Rook)) ||
              (pc == ((!color)|Queen)))
          {
            return true;
          }
          else if (pc) {
            break;
          }
        }
        break;
      }
      else if ((pc == ((!color)|Rook)) || (pc == ((!color)|Queen))) {
        for (x = (XC(std::max<int>(from, cap)) + 1); x < 8; ++x) {
          if ((pc = _board[SQR(x, y)]) == (color|King)) {
            return true;
          }
          else if (pc) {
            break;
          }
        }
        break;
      }
      else if (pc) {
        break;
      }
    }
    return false;
  }
  template<Color color>
  int GetCheckEvasions() {
    assert(IS_SQUARE(_king[color]));
    assert(_board[_king[color]] == (color|King));
    int from = _king[color];
    int attackers = 0;
    int squareCount = 0;
    int squares[40];
    int xrayCount = 0;
    int xray[4] = {-1,-1,-1,-1};
    if (_diagSliders[!color] + _crossSliders[!color]) {
      memset(pinDir, 0, sizeof(pinDir));
    }
    for (uint64_t mvs = _knightMoves[from]; mvs; mvs >>=8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(IS_SQUARE(to));
      assert(to != from);
      if (_board[to] == ((!color)|Knight)) {
        attackers++;
        assert(squareCount < 40);
        squares[squareCount++] = to;
      }
    }
    for (uint64_t mvs = _queenMoves[from]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int end = ((mvs & 0xFF) - 1);
      const int dir = Direction(from, end);
      assert(IS_DIR(dir));
      int newSquareCount = squareCount;
      int firstPiece = 0;
      int to = (from + dir);
      while (true) {
        assert(IS_SQUARE(to));
        assert(Direction(from, to) == dir);
        assert(newSquareCount < 40);
        squares[newSquareCount++] = to;
        if ((to == end) || _board[to]) {
          firstPiece = _board[to];
          break;
        }
        to += dir;
      }
      assert(newSquareCount > squareCount);
      if (!firstPiece) {
        continue;
      }
      assert(IS_SQUARE(to));
      assert(firstPiece == _board[to]);
      if (COLOR(firstPiece) == color) {
        if (_atkd[to]) {
          const int af = ((_atkd[to] >> AttackShift(-dir)) & 0xFF);
          if (af) {
            assert(IS_SQUARE(af - 1));
            assert(IS_SLIDER(_board[af - 1]));
            if (COLOR(_board[af - 1]) != color) {
              pinDir[to] = abs(dir);
            }
          }
        }
        continue;
      }
      switch (firstPiece) {
      case ((!color)|Pawn):
        if (Distance(from, to) == 1) {
          if (color) {
            switch (dir) {
            case SouthWest: case SouthEast:
              squareCount = newSquareCount;
              attackers++;
            }
          }
          else {
            switch (dir) {
            case NorthWest: case NorthEast:
              squareCount = newSquareCount;
              attackers++;
            }
          }
        }
        break;
      case ((!color)|Bishop):
        switch (dir) {
        case SouthWest: case SouthEast: case NorthWest: case NorthEast:
          if (Distance(from, to) > 1) {
            assert(xrayCount < 4);
            xray[xrayCount++] = (from + dir);
          }
          assert(xrayCount < 4);
          xray[xrayCount++] = (from - dir);
          squareCount = newSquareCount;
          attackers++;
        }
        break;
      case ((!color)|Rook):
        switch (dir) {
        case South: case West: case East: case North:
          if (Distance(from, to) > 1) {
            assert(xrayCount < 4);
            xray[xrayCount++] = (from + dir);
          }
          assert(xrayCount < 4);
          xray[xrayCount++] = (from - dir);
          squareCount = newSquareCount;
          attackers++;
        }
        break;
      case ((!color)|Queen):
        if (Distance(from, to) > 1) {
          assert(xrayCount < 4);
          xray[xrayCount++] = (from + dir);
        }
        assert(xrayCount < 4);
        xray[xrayCount++] = (from - dir);
        squareCount = newSquareCount;
        attackers++;
        break;
      }
    }
    assert(attackers < 3);
    if (!attackers) {
      return 0;
    }
    if (attackers == 1) {
      // get non-king moves that block or capture the piece giving check
      for (int z = 0; z < squareCount; ++z) {
        const int to = squares[z];
        assert(IS_SQUARE(to));
        const int cap = _board[to];
        if ((cap == ((!color)|Pawn)) &&
            (ep == (to + (color ? South : North))))
        {
          if (XC(ep) > 0) {
            from = (ep + (color ? NorthWest : SouthWest));
            if ((_board[from] == (color|Pawn)) && !EpPinned<color>(from, to)) {
              AddMove(color, PawnCap, from, ep);
            }
          }
          if (XC(ep) < 7) {
            from = (ep + (color ? NorthEast : SouthEast));
            if ((_board[from] == (color|Pawn)) && !EpPinned<color>(from, to)) {
              AddMove(color, PawnCap, from, ep);
            }
          }
        }
        for (uint64_t mvs = _knightMoves[to]; mvs; mvs >>= 8) {
          assert(mvs & 0xFF);
          from = ((mvs & 0xFF) - 1);
          assert(IS_SQUARE(from));
          assert(from != to);
          if (_board[from] == (color|Knight)) {
            AddMove(color, KnightMove, from, to, cap);
          }
        }
        for (uint64_t mvs = _bishopMoves[to]; mvs; mvs >>= 8) {
          assert(mvs & 0xFF);
          const int end = ((mvs & 0xFF) - 1);
          const int dir = Direction(to, end);
          assert(IS_DIAG(dir));
          for (from = (to + dir);; from += dir) {
            assert(IS_SQUARE(from));
            assert(Direction(to, from) == dir);
            switch (_board[from]) {
            case (color|Pawn):
              if (cap && (color ? (from > to) : (from < to)) &&
                  (Distance(from, to) == 1))
              {
                if (YC(to) == (color ? 0 : 7)) {
                  AddMove(color, PawnCap, from, to, cap, (color|Queen));
                  AddMove(color, PawnCap, from, to, cap, (color|Rook));
                  AddMove(color, PawnCap, from, to, cap, (color|Bishop));
                  AddMove(color, PawnCap, from, to, cap, (color|Knight));
                }
                else {
                  AddMove(color, PawnCap, from, to, cap);
                }
              }
              break;
            case (color|Bishop):
              AddMove(color, BishopMove, from, to, cap);
              break;
            case (color|Queen):
              AddMove(color, QueenMove, from, to, cap);
              break;
            }
            if ((from == end) || _board[from]) {
              break;
            }
          }
        }
        for (uint64_t mvs = _rookMoves[to]; mvs; mvs >>= 8) {
          assert(mvs & 0xFF);
          const int end = ((mvs & 0xFF) - 1);
          const int dir = Direction(to, end);
          assert(IS_CROSS(dir));
          for (from = (to + dir);; from += dir) {
            assert(IS_SQUARE(from));
            assert(Direction(to, from) == dir);
            switch (_board[from]) {
            case (color|Pawn):
              if (!cap && (dir == (color ? North : South))) {
                if (Distance(from, to) == 1) {
                  if (YC(to) == (color ? 0 : 7)) {
                    AddMove(color, PawnMove, from, to, 0, (color|Queen));
                    AddMove(color, PawnMove, from, to, 0, (color|Rook));
                    AddMove(color, PawnMove, from, to, 0, (color|Bishop));
                    AddMove(color, PawnMove, from, to, 0, (color|Knight));
                  }
                  else {
                    AddMove(color, PawnMove, from, to);
                  }
                }
                else if ((Distance(from, to) == 2) &&
                         (YC(to) == (color ? 4 : 3)))
                {
                  AddMove(color, PawnLung, from, to);
                }
              }
              break;
            case (color|Rook):
              AddMove(color, RookMove, from, to, cap);
              break;
            case (color|Queen):
              AddMove(color, QueenMove, from, to, cap);
              break;
            }
            if ((from == end) || _board[from]) {
              break;
            }
          }
        }
      }
    }
    // get king moves
    from = _king[color];
    for (uint64_t mvs = _kingMoves[from]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(IS_SQUARE(to));
      assert(to != from);
      if ((to == xray[0]) || (to == xray[1]) ||
          (to == xray[2]) || (to == xray[3]) ||
          AttackedBy<!color>(to))
      {
        continue;
      }
      const int cap = _board[to];
      if (!cap) {
        AddMove(color, KingMove, from, to);
      }
      else if (COLOR(cap) != color) {
        AddMove(color, KingMove, from, to, cap);
      }
    }
    return attackers;
  }
  template<Color color>
  int GenerateMoves() {
    assert((_pieceCount[White] > 0) && (_pieceCount[White] <= 16));
    assert((_pieceCount[Black] > 0) && (_pieceCount[Black] <= 16));
    assert((_diagSliders[White] >= 0) && (_diagSliders[White] < 12));
    assert((_diagSliders[Black] >= 0) && (_diagSliders[Black] < 12));
    assert((_crossSliders[White] >= 0) && (_crossSliders[White] < 12));
    assert((_crossSliders[Black] >= 0) && (_crossSliders[Black] < 12));
    moveCount = moveIndex = 0;
    if (GetCheckEvasions<color>()) { // this also detects pinned pieces
      return moveCount;
    }
    int pieces = _pieceCount[color];
    int diagSliders = _diagSliders[color];
    int crossSliders = _crossSliders[color];
    for (int sqr = A1; sqr <= H8; ++sqr) {
      switch (_board[sqr]) {
      case (color|Pawn):
        GetPawnMoves<color>(sqr);
        pieces--;
        break;
      case (color|Knight):
        GetKnightMoves(color, sqr);
        pieces--;
        break;
      case (color|Bishop):
        GetSliderMoves(color, BishopMove, _bishopMoves[sqr], sqr);
        pieces--;
        diagSliders--;
        break;
      case (color|Rook):
        GetSliderMoves(color, RookMove, _rookMoves[sqr], sqr);
        pieces--;
        crossSliders--;
        break;
      case (color|Queen):
        GetSliderMoves(color, QueenMove, _queenMoves[sqr], sqr);
        pieces--;
        diagSliders--;
        crossSliders--;
        break;
      case (color|King):
        GetKingMoves<color>(sqr);
        pieces--;
        break;
      default:
        assert(!_board[sqr] || (COLOR(_board[sqr]) != color));
      }
#ifdef NDEBUG
      if (!pieces) {
        break;
      }
#endif
    }
    assert(pieces == 0);
    assert(diagSliders == 0);
    assert(crossSliders == 0);
    return moveCount;
  }
  template<Color color>
  uint64_t PerftSearch(const int depth) {
    GenerateMoves<color>();
    if (!child || (depth <= 1)) {
      return moveCount;
    }
    uint64_t count = 0;
    while (moveIndex < moveCount) {
      const Move& move = moves[moveIndex++];
      Exec<color>(move, *child);
      count += child->PerftSearch<!color>(depth - 1);
      Undo<color>(move);
    }
    return count;
  }
  template<Color color>
  uint64_t Perft(const int depth) {
    assert(!ply);
    assert(!parent);
    assert(child);
    assert(VerifyAttacks(true));
    GenerateMoves<color>();
    std::sort(moves, (moves + moveCount));
    uint64_t total = 0;
    if (depth > 1) {
      while (moveIndex < moveCount) {
        const Move& move = moves[moveIndex++];
        Exec<color>(move, *child);
        const uint64_t count = child->PerftSearch<!color>(depth - 1);
        Undo<color>(move);
        std::cout << move.ToString() << ' ' << count << std::endl;
        total += count;
      }
    }
    else {
      while (moveIndex < moveCount) {
        std::cout << moves[moveIndex++].ToString() << " 1 " << std::endl;
        total++;
      }
    }
    return total;
  }
private:
  Node* parent;
  Node* child;
  int ply;
  int state;
  int ep;
  char pinDir[64];
  int moveIndex;
  int moveCount;
  Move moves[MoveListSize];
};

//-----------------------------------------------------------------------------
Node _nodes[MaxPlies];

//-----------------------------------------------------------------------------
void InitNodeStack() {
  memset(_nodes, 0, sizeof(_nodes));
  for (int ply = 0; ply < MaxPlies; ++ply) {
    if (ply > 0) {
      _nodes[ply].SetParent(&(_nodes[ply - 1]));
    }
    if ((ply + 1) < MaxPlies) {
      _nodes[ply].SetChild(&(_nodes[ply + 1]));
    }
    _nodes[ply].SetPly(ply);
  }
}

//-----------------------------------------------------------------------------
uint64_t GetUnsigned64(const char*& str) {
  uint64_t result = 0;
  const char* p = NextWord(str);
  while (*p && isdigit(*p)) {
    const uint64_t prev = result;
    result = ((10 * result) + (*p++ - '0'));
    if (result < prev) {
      std::cout << "value too large: " << str;
      result = 0;
      str = p;
      break;
    }
  }
  str = p;
  return result;
}

//-----------------------------------------------------------------------------
int GetDepth(const char*& str, uint64_t &expected) {
  expected = 0;
  int depth = 0;
  const char* p = NextWord(str);
  while (*p && isdigit(*p)) {
    depth = ((10 * depth) + (*p++ - '0'));
    if (depth < 0) {
      std::cout << "invalid depth: " << str;
      str = p;
      return 0;
    }
  }
  str = p;
  if (depth && *str && isspace(*str)) {
    expected = GetUnsigned64(str);
  }
  return depth;
}

//-----------------------------------------------------------------------------
bool RunPerft(const int depth, const uint64_t leafs, double& rate) {
  std::cout << "Calculating perft to depth " << depth << std::endl;
  if (leafs) {
    std::cout << "       Expected leaf count " << leafs << std::endl;
  }
  const uint64_t start = Now();
  const uint64_t count = ((_nodes[0].ColorToMove())
      ? _nodes[0].Perft<Black>(depth)
      : _nodes[0].Perft<White>(depth));
  const double elapsed = (double)(Now() - start);
  rate = (elapsed ? (((double)count) / elapsed) : 0);
  std::cout << "Total leafs = " << count << std::endl;
  std::cout << " KLeafs/sec = " << rate << std::endl;
  if (elapsed < 1000) rate = 0;
  if (leafs && (count != leafs)) {
    std::cout << "*** FAIL ***" << std::endl;
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------
bool DoPerft(std::string fen, double& min_rate, double& max_rate,
             const int max_depth, const uint64_t max_leafs = 0)
{
  std::cout << "Loading '" << fen << "'" << std::endl;
  if (!_nodes[0].LoadFEN(fen.c_str())) {
    return false;
  }
  _nodes[0].Print();
  double rate = 0;
  const char* p = strchr(fen.c_str(), ';');
  if (!p) {
    if (!RunPerft(max_depth, max_leafs, rate)) return false;
    if (rate) {
      if (!min_rate || (rate < min_rate)) min_rate = rate;
      if (rate > max_rate) max_rate = rate;
    }
    return true;
  }
  while (p && p[1]) {
    p = NextWord(p + 1);
    if ((*p == 'D') && isdigit(p[1])) {
      uint64_t leafs = 0;
      int depth = GetDepth(++p, leafs);
      if (!depth) depth = max_depth;
      if ((!max_depth || (depth && (depth <= max_depth))) &&
          (!max_leafs || (leafs && (leafs < max_leafs))))
      {
        if (!RunPerft(depth, leafs, rate)) {
          return false;
        }
        if (rate) {
          if (!min_rate || (rate < min_rate)) min_rate = rate;
          if (rate > max_rate) max_rate = rate;
        }
      }
    }
    p = strchr(p, ';');
  }
  return true;
}

//-----------------------------------------------------------------------------
int HandleFEN(int argc, char* argv[]) {
  if (argc < 4) {
    return 1;
  }
  const int depth = atoi(argv[2]);
  if ((depth < 0) || (!depth && (argv[2][0] != 0))) {
    std::cout << "invalid depth: " << argv[2] << std::endl;
    return 2;
  }
  std::string fen = argv[3];
  for (int i = 4; i < argc; ++i) {
    fen += ' ';
    fen += argv[i];
  }
  if (fen.empty()) {
    std::cout << "empty fen string" << std::endl;
    return 2;
  }
  double min_rate = 0;
  double max_rate = 0;
  if (!DoPerft(fen, min_rate, max_rate, depth)) return 3;
  std::cout << " min KLeafs/sec = " << min_rate << std::endl;
  std::cout << " max KLeafs/sec = " << max_rate << std::endl;
  return 0;
}

//-----------------------------------------------------------------------------
int HandleEPD(int argc, char* argv[]) {
  if ((argc != 3) && (argc != 4)) {
    return 1;
  }
  uint64_t max_leafs = 0;
  if (argc == 4) {
    const char* str = argv[3];
    max_leafs = GetUnsigned64(str);
    if (!max_leafs) {
      return 1;
    }
  }
  FILE* fp = NULL;
  if (!(fp = fopen(argv[2], "r"))) {
    std::cout << "Cannot open '" << argv[2] << "': " << strerror(errno);
    return 2;
  }
  char fen[16384];
  int positions = 0;
  int result = 0;
  double min_rate = 0;
  double max_rate = 0;
  for (int line = 1; fgets(fen, sizeof(fen), fp); ++line) {
    const char* f = NextWord(fen);
    if (!*f || (*f == '#')) {
      continue;
    }
    char* p = fen;
    while (*p) ++p;
    while (p-- > f) { if (isspace(*p)) *p = 0; else break; }
    if (!DoPerft(f, min_rate, max_rate, 0, max_leafs)) {
      result = 3;
      break;
    }
    positions++;
  }
  fclose(fp);
  fp = NULL;
  std::cout << positions << " positions tested" << std::endl;
  if (result == 0) {
    std::cout << " min KLeafs/sec = " << min_rate << std::endl;
    std::cout << " max KLeafs/sec = " << max_rate << std::endl;
  }
  return result;
}

//-----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
  std::cout << "Initializing" << std::endl;
  InitDistDir();
  InitMoveMaps();
  InitNodeStack();
  int ret = 1;
  if (argc > 1) {
    if (!strcmp(argv[1], "fen")) {
      ret = HandleFEN(argc, argv);
    }
    else if (!strcmp(argv[1], "epd")) {
      ret = HandleEPD(argc, argv);
    }
  }
  if (ret == 1) {
    std::cout << "usage: " << argv[0]
              << " {fen <depth> <fen>} | {epd <file> [max_leafs]}" << std::endl;
  }
  return ret;
}
