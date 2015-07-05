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
#ifndef WIN32
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
#define IS_MTYPE(x)   (((x) >= PawnMove) && ((x) <= CastleLong))
#define IS_COORD(x)   (((x) >= 0) && ((x) < 8))
#define IS_X(x)       (((x) >= 'a') && ((x) <= 'h'))
#define IS_Y(x)       (((x) >= '1') && ((x) <= '8'))
#define TO_X(x)       ((x) - 'a')
#define TO_Y(x)       ((x) - '1')
#define SQR(x,y)      ((x) + ((y) * 8))
#define XC(x)         ((x) % 8)
#define YC(x)         ((x) / 8)

//-----------------------------------------------------------------------------
typedef bool Color;

//-----------------------------------------------------------------------------
enum Limits {
  MaxPlies      = 100,
  MoveListSize  = 128,
  MoveStackSize = 5576
};

//-----------------------------------------------------------------------------
enum Square {
  A1,B1,C1,D1,E1,F1,G1,H1,
  A2,B2,C2,D2,E2,F2,G2,H2,
  A3,B3,C3,D3,E3,F3,G3,H3,
  A4,B4,C4,D4,E4,F4,G4,H4,
  A5,B5,C5,D5,E5,F5,G5,H5,
  A6,B6,C6,D6,E6,F6,G6,H6,
  A7,B7,C7,D7,E7,F7,G7,H7,
  A8,B8,C8,D8,E8,F8,G8,H8
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
int _dist[64][64] = {0};
int _dir[64][64] = {0};
int _board[64] = {0};
int _king[2] = {0};
int _pieceCount[2] = {0};
int _diagSliders[2] = {0};
int _crossSliders[2] = {0};

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
int Distance(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  return _dist[from][to];
}

//-----------------------------------------------------------------------------
int Direction(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  return _dir[from][to];
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
  Move& Clear() { desc = NoMove; return *this; }
  Move& Set(const int type, const int from, const int to,
            const int cap = 0, const int promo = 0)
  {
    desc = (type                  |
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
  Move& SetCapPromo(const int cap, const int promo) {
    assert(!cap || IS_CAP(cap));
    assert(!promo || IS_PROMO(promo));
    desc = ((desc & TwentyBits)   | // type, from, to
            (cap   <<   CapShift) |
            (promo << PromoShift));
    assert(Cap() == cap);
    assert(Promo() == promo);
    Validate();
    return *this;
  }
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
Move  _moveStack[MoveStackSize];
Move* _pawnMoves[64][2];
Move* _knightMoves[64];
Move* _bishopMoves[64][4];
Move* _rookMoves[64][4];
Move* _queenMoves[64][8];
Move* _kingMoves[64][2];

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
    Move** mvs;
    const Move* mv;
    for (mv = _pawnMoves[sqr][!color]; *mv; ++mv) {
      assert(mv->Valid());
      assert(mv->From() == sqr);
      assert((mv->Type() > 0) && (mv->Type() <= PawnCap));
      if ((mv->Type() == PawnCap) && (_board[mv->To()] == (color|Pawn))) {
        return true;
      }
    }
    for (mv = _knightMoves[sqr]; *mv; ++mv) {
      assert(mv->Valid());
      assert(mv->From() == sqr);
      assert(mv->Type() == KnightMove);
      if (_board[mv->To()] == (color|Knight)) {
        return true;
      }
    }
    bool kingOnly = !_diagSliders[color];
    mvs = _bishopMoves[sqr];
    for (int cap = 0, i = 0; (i < 4) && mvs[i]; ++i) {
      for (mv = mvs[i]; *mv; ++mv) {
        assert(mv->Valid());
        assert(mv->From() == sqr);
        assert(mv->Type() == BishopMove);
        switch ((cap = _board[mv->To()])) {
        case (color|Bishop):
        case (color|Queen):
          return true;
        case (color|King):
          if (_dist[sqr][mv->To()] == 1){
            return true;
          }
          break;
        }
        if (cap || kingOnly) {
          break;
        }
      }
    }
    kingOnly = !_crossSliders[color];
    mvs = _rookMoves[sqr];
    for (int cap = 0, i = 0; (i < 4) && mvs[i]; ++i) {
      for (mv = mvs[i]; *mv; ++mv) {
        assert(mv->Valid());
        switch ((cap = _board[mv->To()])) {
        case (color|Rook):
        case (color|Queen):
          return true;
        case (color|King):
          if (_dist[sqr][mv->To()] == 1){
            return true;
          }
          break;
        }
        if (cap || kingOnly) {
          break;
        }
      }
    }
    return false;
  }
  bool LoadFEN(const char* fen) {
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
    switch (move.Type()) {
    case PawnMove:
      assert(_board[move.From()] == (color|Pawn));
      assert(!_board[move.To()]);
      assert(!move.Cap());
      _board[move.From()] = 0;
      if (move.Promo()) {
        assert(IS_PROMO(move.Promo()));
        assert(COLOR(move.Promo()) == color);
        assert(YC(move.To()) == (color ? 0 : 7));
        _board[move.To()] = move.Promo();
      }
      else {
        assert(YC(move.To()) != (color ? 0 : 7));
        _board[move.To()] = (color|Pawn);
      }
      dest.state = ((state ^ 1) & _TOUCH[move.From()] & _TOUCH[move.To()]);
      dest.ep = 0;
      break;
    case PawnLung:
      assert(_board[move.From()] == (color|Pawn));
      assert(YC(move.From()) == (color ? 6 : 1));
      assert(!_board[move.To()]);
      assert(!_board[move.To() + (color ? North : South)]);
      assert(!move.Cap());
      assert(!move.Promo());
      _board[move.From()] = 0;
      _board[move.To()] = (color|Pawn);
      dest.state = ((state ^ 1) & _TOUCH[move.From()] & _TOUCH[move.To()]);
      dest.ep = (move.To() + (color ? North : South));
      break;
    case PawnCap:
      assert(_board[move.From()] == (color|Pawn));
      _board[move.From()] = 0;
      if (move.Promo()) {
        assert(IS_PROMO(move.Promo()));
        assert(COLOR(move.Promo()) == color);
        assert(YC(move.To()) == (color ? 0 : 7));
        assert(move.Cap());
        _board[move.To()] = move.Promo();
      }
      else {
        assert(YC(move.To()) != (color ? 0 : 7));
        _board[move.To()] = (color|Pawn);
        if (!move.Cap()) {
          assert(ep && (move.To() == ep));
          assert(_board[ep + (color ? North : South)] == ((!color)|Pawn));
          _board[ep + (color ? North : South)] = 0;
          _pieceCount[!color]--;
          assert(_pieceCount[!color] > 0);
        }
      }
      dest.state = ((state ^ 1) & _TOUCH[move.From()] & _TOUCH[move.To()]);
      dest.ep = 0;
      break;
    case KnightMove:
    case BishopMove:
    case RookMove:
    case QueenMove:
      assert(_board[move.From()] == (color|move.Type()));
      assert(_board[move.To()] == move.Cap());
      assert(!move.Promo());
      _board[move.From()] = 0;
      _board[move.To()] = (color|move.Type());
      dest.state = ((state ^ 1) & _TOUCH[move.From()] & _TOUCH[move.To()]);
      dest.ep = 0;
      break;
    case KingMove:
      assert(_board[move.From()] == (color|King));
      assert(_king[color] == move.From());
      assert(!move.Promo());
      _board[move.From()] = 0;
      _board[move.To()] = (color|King);
      _king[color] = move.To();
      dest.state = ((state ^ 1) & _TOUCH[move.From()] & _TOUCH[move.To()]);
      dest.ep = 0;
      break;
    case CastleShort:
      assert(move.From() == (color ? E8 : E1));
      assert(move.To() == (color ? G8 : G1));
      assert(_king[color] == move.From());
      assert(!move.Cap());
      assert(!move.Promo());
      assert(_board[color ? E8 : E1] == (color|King));
      assert(_board[color ? H8 : H1] == (color|Rook));
      assert(!_board[color ? F8 : F1]);
      assert(!_board[color ? G8 : G1]);
      assert(!AttackedBy<!color>(color ? E8 : E1));
      assert(!AttackedBy<!color>(color ? F8 : F1));
      assert(!AttackedBy<!color>(color ? G8 : G1));
      _board[color ? E8 : E1] = 0;
      _board[color ? F8 : F1] = (color|Rook);
      _board[color ? G8 : G1] = (color|King);
      _board[color ? H8 : H1] = 0;
      _king[color] = (color ? G8 : G1);
      dest.state = ((state ^ 1) & ~(color ? BlackCastle : WhiteCastle));
      dest.ep = 0;
      break;
    case CastleLong:
      assert(move.From() == (color ? E8 : E1));
      assert(move.To() == (color ? C8 : C1));
      assert(_king[color] == move.From());
      assert(!move.Cap());
      assert(!move.Promo());
      assert(_board[color ? E8 : E1] == (color|King));
      assert(_board[color ? A8 : A1] == (color|Rook));
      assert(!_board[color ? B8 : B1]);
      assert(!_board[color ? C8 : C1]);
      assert(!_board[color ? D8 : D1]);
      assert(!AttackedBy<!color>(color ? C8 : C1));
      assert(!AttackedBy<!color>(color ? D8 : D1));
      assert(!AttackedBy<!color>(color ? E8 : E1));
      _board[color ? A8 : A1] = 0;
      _board[color ? C8 : C1] = (color|King);
      _board[color ? D8 : D1] = (color|Rook);
      _board[color ? E8 : E1] = 0;
      _king[color] = (color ? C8 : C1);
      dest.state = ((state ^ 1) & ~(color ? BlackCastle : WhiteCastle));
      dest.ep = 0;
      break;
    default:
      assert(false);
    }
    if (move.Cap()) {
      _pieceCount[!color]--;
      assert(_pieceCount[!color] > 0);
      switch (move.Cap()) {
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
    switch (move.Type()) {
    case PawnMove:
    case PawnLung:
      assert(!move.Cap());
      _board[move.From()] = (color|Pawn);
      _board[move.To()] = 0;
      break;
    case PawnCap:
      _board[move.From()] = (color|Pawn);
      if (move.Cap()) {
        _board[move.To()] = move.Cap();
      }
      else {
        assert(ep && (move.To() == ep));
        assert(!_board[ep + (color ? North : South)]);
        _board[move.To()] = 0;
        _board[ep + (color ? North : South)] = ((!color)|Pawn);
        _pieceCount[!color]++;
        assert(_pieceCount[!color] <= 16);
      }
      break;
    case KnightMove:
    case BishopMove:
    case RookMove:
    case QueenMove:
      assert(_board[move.To()] == (color|move.Type()));
      _board[move.From()] = (color|move.Type());
      _board[move.To()] = move.Cap();
      break;
    case KingMove:
      assert(_board[move.To()] == (color|King));
      assert(_king[color] == move.To());
      _board[move.From()] = (color|King);
      _board[move.To()] = move.Cap();
      _king[color] = move.From();
      break;
    case CastleShort:
      assert(move.From() == (color ? E8 : E1));
      assert(move.To() == (color ? G8 : G1));
      assert(_king[color] == move.To());
      assert(!move.Cap());
      assert(!move.Promo());
      assert(!_board[color ? E8 : E1]);
      assert(!_board[color ? H8 : H1]);
      assert(_board[color ? F8 : F1] == (color|Rook));
      assert(_board[color ? G8 : G1] == (color|King));
      _board[color ? E8 : E1] = (color|King);
      _board[color ? F8 : F1] = 0;
      _board[color ? G8 : G1] = 0;
      _board[color ? H8 : H1] = (color|Rook);
      _king[color] = move.From();
      break;
    case CastleLong:
      assert(move.From() == (color ? E8 : E1));
      assert(move.To() == (color ? C8 : C1));
      assert(_king[color] == move.To());
      assert(!move.Cap());
      assert(!move.Promo());
      assert(!_board[color ? E8 : E1]);
      assert(!_board[color ? A8 : A1]);
      assert(!_board[color ? B8 : B1]);
      assert(_board[color ? C8 : C1] == (color|King));
      assert(_board[color ? D8 : D1] == (color|Rook));
      _board[color ? A8 : A1] = (color|Rook);
      _board[color ? C8 : C1] = 0;
      _board[color ? D8 : D1] = 0;
      _board[color ? E8 : E1] = (color|King);
      _king[color] = move.From();
      break;
    default:
      assert(false);
    }
    if (move.Cap()) {
      _pieceCount[!color]++;
      assert(_pieceCount[!color] <= 16);
      switch (move.Cap()) {
      case ((!color)|Bishop):
        _diagSliders[!color]++;
        assert(_diagSliders[!color] < 12);
        break;
      case ((!color)|Rook):
        _crossSliders[!color]++;
        assert(_crossSliders[!color] < 12);
        break;
      case ((!color)|Queen):
        _diagSliders[!color]++;
        _crossSliders[!color]++;
        assert(_diagSliders[!color] < 12);
        assert(_crossSliders[!color] < 12);
        break;
      }
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
  }
  void AddMove(const Color color, const Move& move) {
    assert(moveCount >= 0);
    assert(moveCount < MoveListSize);
    if (_diagSliders[!color] + _crossSliders[!color]) {
      if (atkDir[move.From()] &&
          (atkDir[move.From()] != abs(_dir[move.From()][move.To()])))
      {
        return;
      }
    }
    moves[moveCount++] = move;
  }
  void AddMove(const Color color, const Move& move,
               const int cap, const int promo)
  {
    assert(moveCount >= 0);
    assert(moveCount < MoveListSize);
    if (_diagSliders[!color] + _crossSliders[!color]) {
      if (atkDir[move.From()] &&
          (atkDir[move.From()] != abs(_dir[move.From()][move.To()])))
      {
        return;
      }
    }
    if (cap && !IS_CAP(cap)) {
      Print();
      std::cout << move.ToString() << std::endl;
    }
    (moves[moveCount++] = move).SetCapPromo(cap, promo);
  }
  template<Color color>
  void GetPawnMoves(const Move* mv) {
    assert(mv);
    for (int to, cap; *mv; ++mv) {
      assert(mv->Valid());
      assert(_board[mv->From()] == (color|Pawn));
      cap = _board[(to = mv->To())];
      switch (mv->Type()) {
      case PawnMove:
        if (!cap) {
          if (YC(to) == (color ? 0 : 7)) {
            AddMove(color, *mv, 0, (color|Queen));
            AddMove(color, *mv, 0, (color|Rook));
            AddMove(color, *mv, 0, (color|Bishop));
            AddMove(color, *mv, 0, (color|Knight));
          }
          else {
            AddMove(color, *mv);
          }
        }
        if (mv[1].Type() == PawnLung) {
          ++mv;
          if (!cap && !_board[mv->To()]) {
            AddMove(color, *mv);
          }
        }
        break;
      case PawnCap:
        if (cap && (COLOR(cap) != color)) {
          if (YC(to) == (color ? 0 : 7)) {
            AddMove(color, *mv, cap, (color|Queen));
            AddMove(color, *mv, cap, (color|Rook));
            AddMove(color, *mv, cap, (color|Bishop));
            AddMove(color, *mv, cap, (color|Knight));
          }
          else {
            AddMove(color, *mv, cap, 0);
          }
        }
        else if (!cap && ep && (to == ep) &&
                 !EpPinned<color>(mv->From(), (ep + (color ? North : South))))
        {
          AddMove(color, *mv);
        }
        break;
      default:
        assert(false);
      }
    }
  }
  void GetKnightMoves(const Color color, const Move* mv) {
    assert(mv);
    for (int cap; *mv; ++mv) {
      assert(mv->Valid());
      assert(mv->Type() == KnightMove);
      assert(_board[mv->From()] == (color|Knight));
      if (!(cap = _board[mv->To()])) {
        AddMove(color, *mv);
      }
      else if (COLOR(cap) != color) {
        AddMove(color, *mv, cap, 0);
      }
    }
  }
  void GetSliderMoves(const Color color, Move** mvs, const int count) {
    assert(mvs);
    int cap;
    for (int i = 0; (i < count) && mvs[i]; ++i) {
      for (const Move* mv = mvs[i]; *mv; ++mv) {
        assert(mv->Valid());
        assert(_board[mv->From()] == (color|mv->Type()));
        if (!(cap = _board[mv->To()])) {
          AddMove(color, *mv);
        }
        else {
          if (COLOR(cap) != color) {
            AddMove(color, *mv, cap, 0);
          }
          break;
        }
      }
    }
  }
  template<Color color>
  void GetKingMoves(const Move* mv) {
    assert(mv);
    assert(IS_SQUARE(_king[color]));
    assert(_board[_king[color]] == (color|King));
    assert(!AttackedBy<!color>(_king[color]));
    for (int cap; *mv; ++mv) {
      assert(mv->Valid());
      assert(_board[mv->From()] == (color|King));
      cap = _board[mv->To()];
      switch (mv->Type()) {
      case KingMove:
        if (!AttackedBy<!color>(mv->To())) {
          if (!cap) {
            AddMove(color, *mv);
          }
          else if (COLOR(cap) != color) {
            AddMove(color, *mv, cap, 0);
          }
        }
        break;
      case CastleShort:
        if (!cap && (state & (color ? BlackShort : WhiteShort)) &&
            !_board[color ? F8 : F1] && !AttackedBy<!color>(color ? F8 : F1) &&
            !_board[color ? G8 : G1] && !AttackedBy<!color>(color ? G8 : G1))
        {
          assert(_board[color ? H8 : H1] == (color|Rook));
          AddMove(color, *mv);
        }
        break;
      case CastleLong:
        if (!cap && (state & (color ? BlackShort : WhiteShort)) &&
            !_board[color ? B8 : B1] &&
            !_board[color ? C8 : C1] && !AttackedBy<!color>(color ? C8 : C1) &&
            !_board[color ? D8 : D1] && !AttackedBy<!color>(color ? D8 : D1))
        {
          assert(_board[color ? A8 : A1] == (color|Rook));
          AddMove(color, *mv);
        }
        break;
      default:
        assert(false);
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
    assert(_dist[from][cap] == 1);
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
    int attackers = 0;
    int squareCount = 0;
    int squares[40];
    int xrayCount = 0;
    int xray[4] = {-1,-1,-1,-1};
    if (_diagSliders[!color] + _crossSliders[!color]) {
      memset(atkDir, 0, sizeof(atkDir));
    }
    const Move* mv = _knightMoves[_king[color]];
    for (; *mv; ++mv) {
      assert(mv->Valid());
      assert(mv->Type() == KnightMove);
      assert(mv->From() == _king[color]);
      if (_board[mv->To()] == ((!color)|Knight)) {
        attackers++;
        assert(squareCount < 40);
        squares[squareCount++] = mv->To();
      }
    }
    Move** mvs = _queenMoves[_king[color]];
    for (int i = 0; (i < 8) && mvs[i]; ++i) {
      int firstPiece = 0;
      int pinnedSquare = -1;
      int newSquareCount = squareCount;
      for (mv = mvs[i]; *mv; ++mv) {
        assert(mv->Valid());
        assert(mv->Type() == QueenMove);
        assert(mv->From() == _king[color]);
        if (firstPiece) {
          assert(COLOR(firstPiece) == color);
          assert(IS_SQUARE(pinnedSquare));
          assert(_board[pinnedSquare] == firstPiece);
          const int pc = _board[mv->To()];
          if (pc) {
            if ((COLOR(pc) != color) && (pc >= Bishop) && (pc < King)) {
              const int dir = _dir[mv->From()][mv->To()];
              switch (dir) {
              case SouthWest: case SouthEast: case NorthWest: case NorthEast:
                if ((pc == ((!color)|Bishop)) || (pc == ((!color)|Queen))) {
                  atkDir[pinnedSquare] = abs(dir);
                }
                break;
              case South: case West: case East: case North:
                if ((pc == ((!color)|Rook)) || (pc == ((!color)|Queen))) {
                  atkDir[pinnedSquare] = abs(dir);
                }
                break;
              default:
                assert(false);
              }
            }
            break;
          }
        }
        else {
          assert(squareCount < 40);
          squares[squareCount++] = mv->To();
          if ((firstPiece = _board[mv->To()])) {
            if (COLOR(firstPiece) == color) {
              if (_diagSliders[!color] + _crossSliders[!color]) {
                pinnedSquare = mv->To();
                continue;
              }
            }
            break;
          }
        }
      }
      switch (firstPiece) {
      case ((!color)|Pawn):
        if (_dist[mv->From()][mv->To()] == 1) {
          if (color) {
            switch (_dir[mv->From()][mv->To()]) {
            case SouthWest: case SouthEast:
              newSquareCount = squareCount;
              attackers++;
              break;
            }
          }
          else {
            switch (_dir[mv->From()][mv->To()]) {
            case NorthWest: case NorthEast:
              newSquareCount = squareCount;
              attackers++;
              break;
            }
          }
        }
        break;
      case ((!color)|Bishop):
        switch (_dir[mv->From()][mv->To()]) {
        case SouthWest: case SouthEast: case NorthWest: case NorthEast:
          if (_dist[mv->From()][mv->To()] > 1) {
            assert(xrayCount < 4);
            xray[xrayCount++] = (mv->From() + _dir[mv->From()][mv->To()]);
          }
          assert(xrayCount < 4);
          xray[xrayCount++] = (mv->From() + _dir[mv->To()][mv->From()]);
          newSquareCount = squareCount;
          attackers++;
          break;
        }
        break;
      case ((!color)|Rook):
        switch (_dir[mv->From()][mv->To()]) {
        case South: case West: case East: case North:
          if (_dist[mv->From()][mv->To()] > 1) {
            assert(xrayCount < 4);
            xray[xrayCount++] = (mv->From() + _dir[mv->From()][mv->To()]);
          }
          assert(xrayCount < 4);
          xray[xrayCount++] = (mv->From() + _dir[mv->To()][mv->From()]);
          newSquareCount = squareCount;
          attackers++;
          break;
        }
        break;
      case ((!color)|Queen):
        if (_dist[mv->From()][mv->To()] > 1) {
          assert(xrayCount < 4);
          xray[xrayCount++] = (mv->From() + _dir[mv->From()][mv->To()]);
        }
        assert(xrayCount < 4);
        xray[xrayCount++] = (mv->From() + _dir[mv->To()][mv->From()]);
        newSquareCount = squareCount;
        attackers++;
        break;
      }
      squareCount = newSquareCount;
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
            const int from = (ep + (color ? NorthWest : SouthWest));
            if ((_board[from] == (color|Pawn)) && !EpPinned<color>(from, to)) {
              AddMove(color, Move().Set(PawnCap, from, ep));
            }
          }
          if (XC(ep) < 7) {
            const int from = (ep + (color ? NorthEast : SouthEast));
            if ((_board[from] == (color|Pawn)) && !EpPinned<color>(from, to)) {
              AddMove(color, Move().Set(PawnCap, from, ep));
            }
          }
        }
        for (mv = _knightMoves[to]; *mv; ++mv) {
          assert(mv->Valid());
          assert(mv->Type() == KnightMove);
          assert(mv->From() == to);
          if (_board[mv->To()] == (color|Knight)) {
            AddMove(color, Move().Set(KnightMove, mv->To(), to, cap));
          }
        }
        mvs = _bishopMoves[to];
        for (int i = 0; (i < 4) && mvs[i]; ++i) {
          for (mv = mvs[i]; *mv; ++mv) {
            assert(mv->Valid());
            assert(mv->Type() == BishopMove);
            assert(mv->From() == to);
            const int from = mv->To();
            switch (_board[from]) {
            case (color|Pawn):
              if (cap && (color ? (from > to) : (from < to)) &&
                  (_dist[from][to] == 1))
              {
                if (YC(to) == (color ? 0 : 7)) {
                  AddMove(color, Move().Set(PawnCap, from, to, cap,
                                            (color|Queen)));
                  AddMove(color, Move().Set(PawnCap, from, to, cap,
                                            (color|Rook)));
                  AddMove(color, Move().Set(PawnCap, from, to, cap,
                                            (color|Bishop)));
                  AddMove(color, Move().Set(PawnCap, from, to, cap,
                                            (color|Knight)));
                }
                else {
                  AddMove(color, Move().Set(PawnCap, from, to, cap));
                }
              }
              break;
            case (color|Bishop):
              AddMove(color, Move().Set(BishopMove, from, to, cap));
              break;
            case (color|Queen):
              AddMove(color, Move().Set(QueenMove, from, to, cap));
              break;
            }
            if (_board[from]) {
              break;
            }
          }
        }
        mvs = _rookMoves[to];
        for (int i = 0; (i < 4) && mvs[i]; ++i) {
          for (mv = mvs[i]; *mv; ++mv) {
            assert(mv->Valid());
            assert(mv->Type() == RookMove);
            assert(mv->From() == to);
            const int from = mv->To();
            switch (_board[from]) {
            case (color|Pawn):
              if (!cap && (_dir[from][to] == (color ? South : North))) {
                if (_dist[from][to] == 1) {
                  if (YC(to) == (color ? 0 : 7)) {
                    AddMove(color, Move().Set(PawnMove, from, to, 0,
                                              (color|Queen)));
                    AddMove(color, Move().Set(PawnMove, from, to, 0,
                                              (color|Rook)));
                    AddMove(color, Move().Set(PawnMove, from, to, 0,
                                              (color|Bishop)));
                    AddMove(color, Move().Set(PawnMove, from, to, 0,
                                              (color|Knight)));
                  }
                  else {
                    AddMove(color, Move().Set(PawnMove, from, to));
                  }
                }
                else if ((_dist[from][to] == 2) &&
                         (YC(to) == (color ? 4 : 3)))
                {
                  AddMove(color, Move().Set(PawnLung, from, to));
                }
              }
              break;
            case (color|Rook):
              AddMove(color, Move().Set(RookMove, from, to, cap));
              break;
            case (color|Queen):
              AddMove(color, Move().Set(QueenMove, from, to, cap));
              break;
            }
            if (_board[from]) {
              break;
            }
          }
        }
      }
    }
    // get king moves
    for (mv = _kingMoves[_king[color]][color]; *mv; ++mv) {
      assert(mv->Valid());
      assert(mv->From() == _king[color]);
      assert(_board[mv->From()] == (color|King));
      if (mv->Type() != KingMove) {
        continue;
      }
      const int to = mv->To();
      if ((to == xray[0]) || (to == xray[1]) ||
          (to == xray[2]) || (to == xray[3]) ||
          AttackedBy<!color>(to))
      {
        continue;
      }
      const int cap = _board[to];
      if (!cap) {
        AddMove(color, *mv);
      }
      else if (COLOR(cap) != color) {
        AddMove(color, *mv, cap, 0);
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
        GetPawnMoves<color>(_pawnMoves[sqr][color]);
        pieces--;
        break;
      case (color|Knight):
        GetKnightMoves(color, _knightMoves[sqr]);
        pieces--;
        break;
      case (color|Bishop):
        GetSliderMoves(color, _bishopMoves[sqr], 4);
        pieces--;
        diagSliders--;
        break;
      case (color|Rook):
        GetSliderMoves(color, _rookMoves[sqr], 4);
        pieces--;
        crossSliders--;
        break;
      case (color|Queen):
        GetSliderMoves(color, _queenMoves[sqr], 8);
        pieces--;
        diagSliders--;
        crossSliders--;
        break;
      case (color|King):
        assert(_king[color] == sqr);
        GetKingMoves<color>(_kingMoves[sqr][color]);
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
  int atkDir[64];
  int moveIndex;
  int moveCount;
  Move moves[MoveListSize];
};

//-----------------------------------------------------------------------------
Node  _nodes[MaxPlies];

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
Move* StackMove(int& stackPos) {
  assert(stackPos < MoveStackSize);
  return &(_moveStack[stackPos++]);
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
void StackPawnMoves(const int from, int& stackPos) {
  assert(IS_SQUARE(from));
  Move* first = NULL;
  Move* mv = NULL;
  int to = 0;
  if (color ? (YC(from) > 0) : (YC(from) < 7)) {
    if (XC(from) > 0) {
      to = (from + (color ? SouthWest : NorthWest));
      (mv = StackMove(stackPos))->Set(PawnCap, from, to);
      if (!first) first = mv;
    }
    if (XC(from) < 7) {
      to = (from + (color ? SouthEast : NorthEast));
      (mv = StackMove(stackPos))->Set(PawnCap, from, to);
      if (!first) first = mv;
    }
    to = (from + (color ? South : North));
    (mv = StackMove(stackPos))->Set(PawnMove, from, to);
    if (!first) first = mv;
    if (YC(from) == (color ? 6 : 1)) {
      (mv = StackMove(stackPos))->Set(PawnLung, from,
                                      (to + (color ? South : North)));
    }
  }
  (mv = StackMove(stackPos))->Clear();
  if (!first) first = mv;
  _pawnMoves[from][color] = first;
}

//-----------------------------------------------------------------------------
void StackKnightMoves(const int from, int& stackPos) {
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
  Move* first = NULL;
  Move* mv = NULL;
  int to = 0;
  for (int i = 0; i < 8; ++i) {
    if (GetToSquare(from, dir[i], to)) {
      (mv = StackMove(stackPos))->Set(KnightMove, from, to);
      if (!first) first = mv;
    }
  }
  (mv = StackMove(stackPos))->Clear();
  if (!first) first = mv;
  _knightMoves[from] = first;
}

//-----------------------------------------------------------------------------
void StackBishopMoves(const int from, int& stackPos) {
  assert(IS_SQUARE(from));
  const int dir[4][2] = {
    { -1, -1 },
    {  1, -1 },
    { -1,  1 },
    {  1,  1 }
  };
  Move* mv = NULL;
  int idx = 0;
  int to = 0;
  for (int i = 0; i < 4; ++i) {
    Move* first = NULL;
    for (int frm = from; GetToSquare(frm, dir[i], to); frm = to) {
      (mv = StackMove(stackPos))->Set(BishopMove, from, to);
      if (!first) first = mv;
    }
    if (first) {
      StackMove(stackPos)->Clear();
      _bishopMoves[from][idx++] = first;
    }
  }
  if (idx < 4) {
    _bishopMoves[from][idx] = NULL;
  }
}

//-----------------------------------------------------------------------------
void StackRookMoves(const int from, int& stackPos) {
  assert(IS_SQUARE(from));
  const int dir[4][2] = {
    {  0, -1 },
    { -1,  0 },
    {  1,  0 },
    {  0,  1 }
  };
  Move* mv = NULL;
  int idx = 0;
  int to = 0;
  for (int i = 0; i < 4; ++i) {
    Move* first = NULL;
    for (int frm = from; GetToSquare(frm, dir[i], to); frm = to) {
      (mv = StackMove(stackPos))->Set(RookMove, from, to);
      if (!first) first = mv;
    }
    if (first) {
      StackMove(stackPos)->Clear();
      _rookMoves[from][idx++] = first;
    }
  }
  if (idx < 4) {
    _rookMoves[from][idx] = NULL;
  }
}

//-----------------------------------------------------------------------------
void StackQueenMoves(const int from, int& stackPos) {
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
  Move* mv = NULL;
  int idx = 0;
  int to = 0;
  for (int i = 0; i < 8; ++i) {
    Move* first = NULL;
    for (int frm = from; GetToSquare(frm, dir[i], to); frm = to) {
      (mv = StackMove(stackPos))->Set(QueenMove, from, to);
      if (!first) first = mv;
    }
    if (first) {
      StackMove(stackPos)->Clear();
      _queenMoves[from][idx++] = first;
    }
  }
  if (idx < 8) {
    _queenMoves[from][idx] = NULL;
  }
}

//-----------------------------------------------------------------------------
template<Color color>
void StackKingMoves(const int from, int& stackPos) {
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
  Move* first = NULL;
  Move* mv = NULL;
  int to = 0;
  if (from == (color ? E8 : E1)) {
    (mv = StackMove(stackPos))->Set(CastleShort, from, (color ? G8 : G1));
    if (!first) first = mv;
    (mv = StackMove(stackPos))->Set(CastleLong, from, (color ? C8 : C1));
  }
  for (int i = 0; i < 8; ++i) {
    if (GetToSquare(from, dir[i], to)) {
      (mv = StackMove(stackPos))->Set(KingMove, from, to);
      if (!first) first = mv;
    }
  }
  (mv = StackMove(stackPos))->Clear();
  if (!first) first = mv;
  _kingMoves[from][color] = first;
}

//-----------------------------------------------------------------------------
void InitMoveStack() {
  memset(_moveStack,   0, sizeof(_moveStack));
  memset(_pawnMoves,   0, sizeof(_pawnMoves));
  memset(_knightMoves, 0, sizeof(_knightMoves));
  memset(_bishopMoves, 0, sizeof(_bishopMoves));
  memset(_rookMoves,   0, sizeof(_rookMoves));
  memset(_queenMoves,  0, sizeof(_queenMoves));
  memset(_kingMoves,   0, sizeof(_kingMoves));
  int stackPos = 0;
  for (int sqr = A1; IS_SQUARE(sqr); ++sqr) {
    StackPawnMoves<White>(sqr, stackPos);
    StackPawnMoves<Black>(sqr, stackPos);
    StackKnightMoves(sqr, stackPos);
    StackBishopMoves(sqr, stackPos);
    StackRookMoves(sqr, stackPos);
    StackQueenMoves(sqr, stackPos);
    StackKingMoves<White>(sqr, stackPos);
    StackKingMoves<Black>(sqr, stackPos);
  }
#ifndef NDEBUG
  std::cout << "Move stack count = " << stackPos << std::endl;
#endif
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
bool DoPerft(std::string fen,
             const int max_depth = 0,
             const uint64_t max_leafs = 0)
{
  std::cout << "Loading '" << fen << "'" << std::endl;
  if (!_nodes[0].LoadFEN(fen.c_str())) {
    return false;
  }
  _nodes[0].Print();
  const char* p = strchr(fen.c_str(), ';');
  while (p && p[1]) {
    p = NextWord(p + 1);
    if ((*p == 'D') && isdigit(p[1])) {
      uint64_t leafs = 0;
      int depth = GetDepth(++p, leafs);
      if (!depth) depth = max_depth;
      if ((!max_depth || (depth && (depth <= max_depth))) &&
          (!max_leafs || (leafs && (leafs < max_leafs))))
      {
        std::cout << "Calculating perft to depth " << depth << std::endl;
        if (leafs) {
          std::cout << "       Expected leaf count " << leafs << std::endl;
        }
        const uint64_t start = Now();
        const uint64_t count = ((_nodes[0].ColorToMove())
            ? _nodes[0].Perft<Black>(depth)
            : _nodes[0].Perft<White>(depth));
        const double elapsed = (double)(Now() - start);
        std::cout << "Total leafs = " << count << std::endl;
        std::cout << " KLeafs/sec = " << (((double)count) / elapsed)
                  << std::endl;
        if (leafs && (count != leafs)) {
          std::cout << "*** FAIL ***" << std::endl;
          return false;
        }
      }
    }
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
  for (int i = 3; i < argc; ++i) {
    fen += ' ';
    fen += argv[i];
  }
  if (fen.empty()) {
    std::cout << "empty fen string" << std::endl;
    return 2;
  }
  return (DoPerft(fen, depth) ? 0 : 3);
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
  for (int line = 1; fgets(fen, sizeof(fen), fp); ++line) {
    const char* f = NextWord(fen);
    if (!*f || (*f == '#')) {
      continue;
    }
    if (!DoPerft(f, 0, max_leafs)) {
      result = 3;
      break;
    }
    positions++;
  }
  fclose(fp);
  fp = NULL;
  std::cout << positions << " positions tested" << std::endl;
  return result;
}

//-----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
  std::cout << "Initializing" << std::endl;
  InitDistDir();
  InitNodeStack();
  InitMoveStack();
  int ret = 1;
  if (!strcmp(argv[1], "fen")) {
    ret = HandleFEN(argc, argv);
  }
  else if (!strcmp(argv[1], "epd")) {
    ret = HandleEPD(argc, argv);
  }
  if (ret == 1) {
    std::cout << "usage: " << argv[0]
              << " {fen <depth> <fen>} | {epd <file> [max_leafs]}" << std::endl;
  }
  return ret;
}
