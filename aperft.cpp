#include <algorithm>
#include <iostream>
#include <string>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//-----------------------------------------------------------------------------
#define COLOR(x)      static_cast<Color>((x) & 1)
#define IS_COLOR(x)   (((x) == Black) || ((x) == White))
#define IS_SQUARE(x)  (((x) >= A1) && ((x) <= H8))
#define IS_PTYPE(x)   (((x) >= (White|Pawn)) && ((x) <= (Black|King)))
#define IS_PAWN(x)    ((Black|(x)) == (Black|Pawn))
#define IS_KING(x)    ((Black|(x)) == (Black|King))
#define IS_CAP(x)     (((x) >= (White|Pawn)) && ((x) <= (Black|Queen)))
#define IS_PROMO(x)   (((x) >= (White|Knight)) && ((x) <= (Black|Queen)))
#define IS_MTYPE(x)   (((x) >= KingMove) && ((x) <= PawnCap))
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
int _board[64] = {0};

//-----------------------------------------------------------------------------
enum Limits {
  MaxPlies      = 100,
  MoveListSize  = 128,
  MoveStackSize = 5532
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
  KingMove,
  CastleShort,
  CastleLong,
  KnightMove,
  BishopMove,
  RookMove,
  QueenMove,
  PawnMove,
  PawnLung,
  PawnCap
};

//-----------------------------------------------------------------------------
enum CastleRights {
  WhiteShort = 0x02,
  WhiteLong  = 0x04,
  BlackShort = 0x08,
  BlackLong  = 0x10
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
  void Clear() { desc = NoMove; }
  void Set(const int type, const int from, const int to,
           const int cap = 0, const int promo = 0)
  {
    desc = (static_cast<uint32_t>(type) |
            static_cast<uint32_t>(from  <<  FromShift) |
            static_cast<uint32_t>(to    <<    ToShift) |
            static_cast<uint32_t>(cap   <<   CapShift) |
            static_cast<uint32_t>(promo << PromoShift));
    assert(Type() == type);
    assert(From() == from);
    assert(To() == to);
    assert(Cap() == cap);
    assert(Promo() == promo);
    Validate();
  }
  void SetCapPromo(const int cap, const int promo) {
    assert(!cap || IS_CAP(cap));
    assert(!promo || IS_PROMO(promo));
    desc = ((desc & TwentyBits) | // type, from, to
            static_cast<uint32_t>(cap   <<   CapShift) |
            static_cast<uint32_t>(promo << PromoShift));
    assert(Cap() == cap);
    assert(Promo() == promo);
    Validate();
  }
  bool operator!() const { return !desc; }
  bool operator==(const Move& other) const { return (desc == other.desc); }
  bool operator!=(const Move& other) const { return (desc != other.desc); }
  operator bool() const { return desc; }
  operator uint32_t() const { return desc; }
  bool Valid() const { return (Type() && (From() != To())); }
  int Type() const { return static_cast<int>(desc & FourBits);  }
  int From() const { return static_cast<int>((desc >> FromShift) & SixBits); }
  int To() const { return static_cast<int>((desc >> ToShift) & SixBits); }
  int Cap() const { return static_cast<int>((desc >> CapShift) & FourBits); }
  int Promo() const { return static_cast<int>((desc>>PromoShift) & FourBits); }
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
  uint32_t desc;
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
  int ColorToMove() const { return COLOR(state); }
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
  bool LoadFEN(const char* fen) {
    memset(_board, 0, sizeof(_board));
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
          case 'b': _board[SQR(x, y)] = (Black|Bishop); break;
          case 'B': _board[SQR(x, y)] = (White|Bishop); break;
          case 'k': _board[SQR(x, y)] = (Black|King);   break;
          case 'K': _board[SQR(x, y)] = (White|King);   break;
          case 'n': _board[SQR(x, y)] = (Black|Knight); break;
          case 'N': _board[SQR(x, y)] = (White|Knight); break;
          case 'p': _board[SQR(x, y)] = (Black|Pawn);   break;
          case 'P': _board[SQR(x, y)] = (White|Pawn);   break;
          case 'q': _board[SQR(x, y)] = (Black|Queen);  break;
          case 'Q': _board[SQR(x, y)] = (White|Queen);  break;
          case 'r': _board[SQR(x, y)] = (Black|Rook);   break;
          case 'R': _board[SQR(x, y)] = (White|Rook);   break;
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
    p = NextSpace(p);
    p = NextWord(p);
    switch (*p) {
    case 'b': state |= Black; break;
    case 'w': state |= White; break;
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
  bool Exec(const Move& /*move*/, Node& /*dest*/) const {
    // TODO
    return false;
  }
  template<Color color>
  void Undo(const Move& /*move*/) const {
    // TODO
  }
  Move& AddMove(const Move& move) {
    assert(moveCount >= 0);
    assert(moveCount < MoveListSize);
    return (moves[moveCount++] = move);
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
            for (int promo = Queen; promo >= Knight; promo -= 2) {
              AddMove(*mv).SetCapPromo(0, (color|promo));
            }
          }
          else {
            AddMove(*mv);
            if (mv[1].Type() == PawnLung) {
              ++mv;
              if (!_board[mv->To()]) {
                AddMove(*mv);
              }
            }
          }
        }
        break;
      case PawnCap:
        if (cap && (COLOR(cap) != color)) {
          if (YC(to) == (color ? 0 : 7)) {
            for (int promo = Queen; promo >= Knight; promo -= 2) {
              AddMove(*mv).SetCapPromo(cap, (color|promo));
            }
          }
          else {
            AddMove(*mv).SetCapPromo(cap, 0);
          }
        }
        else if (!cap && ep && (to == ep)) {
          AddMove(*mv);
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
        AddMove(*mv);
      }
      else if (COLOR(cap) != color) {
        AddMove(*mv).SetCapPromo(cap, 0);
      }
    }
  }
  void GetBishopMoves(const Color color, Move* mvs[4]) {
    assert(mvs);
    int cap;
    for (int i = 0; (i < 4) && mvs[i]; ++i) {
      for (const Move* mv = mvs[i]; *mv; ++mv) {
        assert(mv->Valid());
        assert(mv->Type() == BishopMove);
        assert(_board[mv->From()] == (color|Bishop));
        if (!(cap = _board[mv->To()])) {
          AddMove(*mv);
        }
        else {
          if (COLOR(cap) != color) {
            AddMove(*mv).SetCapPromo(cap, 0);
          }
          break;
        }
      }
    }
  }
  void GetRookMoves(const Color color, Move* mvs[4]) {
    assert(mvs);
    int cap;
    for (int i = 0; (i < 4) && mvs[i]; ++i) {
      for (const Move* mv = mvs[i]; *mv; ++mv) {
        assert(mv->Valid());
        assert(mv->Type() == RookMove);
        assert(_board[mv->From()] == (color|Rook));
        if (!(cap = _board[mv->To()])) {
          AddMove(*mv);
        }
        else {
          if (COLOR(cap) != color) {
            AddMove(*mv).SetCapPromo(cap, 0);
          }
          break;
        }
      }
    }
  }
  void GetQueenMoves(const Color color, Move* mvs[8]) {
    assert(mvs);
    int cap;
    for (int i = 0; (i < 8) && mvs[i]; ++i) {
      for (const Move* mv = mvs[i]; *mv; ++mv) {
        assert(mv->Valid());
        assert(mv->Type() == QueenMove);
        assert(_board[mv->From()] == (color|Queen));
        if (!(cap = _board[mv->To()])) {
          AddMove(*mv);
        }
        else {
          if (COLOR(cap) != color) {
            AddMove(*mv).SetCapPromo(cap, 0);
          }
          break;
        }
      }
    }
  }
  template<Color color>
  void GetKingMoves(const Move* mv) {
    assert(mv);
    for (int cap; *mv; ++mv) {
      assert(mv->Valid());
      assert(_board[mv->From()] == (color|King));
      cap = _board[mv->To()];
      switch (mv->Type()) {
      case KingMove:
        if (!cap) {
          AddMove(*mv);
        }
        else if (COLOR(cap) != color) {
          AddMove(*mv).SetCapPromo(cap, 0);
        }
        break;
      case CastleShort:
        if (!cap && (state & (color ? BlackShort : WhiteShort)) &&
            !_board[color ? F8 : F1] && // TODO
            !_board[color ? G8 : G1])
        {
          assert(_board[color ? H8 : H1] == (color|Rook));
          AddMove(*mv);
        }
        break;
      case CastleLong:
        if (!cap && (state & (color ? BlackShort : WhiteShort)) &&
            !_board[color ? B8 : B1] && // TODO
            !_board[color ? C8 : C1] &&
            !_board[color ? D8 : D1])
        {
          assert(_board[color ? A8 : A1] == (color|Rook));
          AddMove(*mv);
        }
        break;
      default:
        assert(false);
      }
    }
  }
  template<Color color>
  int GenerateMoves() {
    moveCount = moveIndex = 0;
    for (int sqr = A1; sqr <= H8; ++sqr) {
      switch (_board[sqr]) {
      case (color|Pawn):   GetPawnMoves<color>(_pawnMoves[sqr][color]); break;
      case (color|Knight): GetKnightMoves(color, _knightMoves[sqr]);    break;
      case (color|Bishop): GetBishopMoves(color, _bishopMoves[sqr]);    break;
      case (color|Rook):   GetRookMoves(color, _rookMoves[sqr]);        break;
      case (color|Queen):  GetQueenMoves(color, _queenMoves[sqr]);      break;
      case (color|King):   GetKingMoves<color>(_kingMoves[sqr][color]); break;
      default:
        assert(!_board[sqr] || (COLOR(_board[sqr]) != color));
      }
    }
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
  void Perft(const int depth) {
    assert(!ply);
    assert(!parent);
    assert(child);
    GenerateMoves<color>();
    std::sort(moves, (moves + moveCount));
    if (depth > 1) {
      while (moveIndex < moveCount) {
        const Move& move = moves[moveIndex++];
        Exec<color>(move, *child);
        const uint64_t count = child->PerftSearch<!color>(depth - 1);
        Undo<color>(move);
        std::cout << move.ToString() << ' ' << count << std::endl;
      }
    }
    else {
      while (moveIndex < moveCount) {
        std::cout << moves[moveIndex++].ToString() << " 1 " << std::endl;
      }
    }
  }
private:
  Node* parent;
  Node* child;
  int ply;
  int state;
  int ep;
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
  if ((YC(from) != 0) && (YC(from) != 7)) {
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
  if (!first) mv = first;
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
  if (!first) mv = first;
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
  if (!first) mv = first;
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
int main(int argc, char* argv[])
{
  if (argc < 3) {
    std::cout << "usage: " << argv[0] << " {depth} {fen}" << std::endl;
    return 1;
  }
  const int depth = atoi(argv[1]);
  if ((depth < 0) || (!depth && (argv[1][0] != 0))) {
    std::cout << "invalid depth: " << argv[1] << std::endl;
    return 2;
  }
  std::string fen = argv[2];
  for (int i = 3; i < argc; ++i) {
    fen += ' ';
    fen += argv[i];
  }
  if (fen.empty()) {
    std::cout << "empty fen string" << std::endl;
    return 2;
  }
  std::cout << "Initializing" << std::endl;
  InitNodeStack();
  InitMoveStack();
  std::cout << "Loading '" << fen << "'" << std::endl;
  if (!_nodes[0].LoadFEN(fen.c_str())) {
    return 3;
  }
  _nodes[0].Print();
  std::cout << "Calculating perft to depth " << depth << std::endl;
  if (_nodes[0].ColorToMove()) {
    _nodes[0].Perft<Black>(depth);
  }
  else {
    _nodes[0].Perft<White>(depth);
  }
  return 0;
}

