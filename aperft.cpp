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
enum Color {
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
  WhiteShort = 0x1,
  WhiteLong  = 0x2,
  BlackShort = 0x4,
  BlackLong  = 0x8
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
  void Set(const Move& other, const int cap, const int promo) {
    other.Validate();
    desc = ((other.desc & TwentyBits) | // type, from, to
            static_cast<uint32_t>(cap   <<   CapShift) |
            static_cast<uint32_t>(promo << PromoShift));
    assert(Type() == other.Type());
    assert(From() == other.From());
    assert(To() == other.To());
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
  void Exec(const Move& move, Node& dest) const {
    // TODO
  }
  void Undo(const Move& move) const {
    // TODO
  }
  Move NextMove() {
    assert(moveCount >= 0);
    assert(moveIndex <= moveCount);
    if (moveIndex < moveCount) {
      return moves[moveIndex++];
    }
    return Move();
  }
  int GenerateMoves() {
    moveCount = moveIndex = 0;
    // TODO
    return moveCount;
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
Move  _moveStack[MoveStackSize];
Move* _pawnMoves[64][2];
Move* _knightMoves[64];
Move* _bishopMoves[64][4];
Move* _rookMoves[64][4];
Move* _queenMoves[64][8];
Move* _kingMoves[64][2];

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
  assert(IS_COLOR(color));
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
      (mv = StackMove(stackPos))->Set(PawnLung, from, to);
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
  if (!idx) {
    (mv = StackMove(stackPos))->Clear();
    _bishopMoves[from][0] = mv;
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
  if (!idx) {
    (mv = StackMove(stackPos))->Clear();
    _rookMoves[from][0] = mv;
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
  if (!idx) {
    (mv = StackMove(stackPos))->Clear();
    _queenMoves[from][0] = mv;
  }
}

//-----------------------------------------------------------------------------
template<Color color>
void StackKingMoves(const int from, int& stackPos) {
  assert(IS_COLOR(color));
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
  std::cout << "Calculating perft to depth " << depth << std::endl;
  // TODO
  return 0;
}

