#include "plugins/BoxDudePlugin.h"

namespace
{
using Sprite = BoxDudePlugin::Sprite;

constexpr unsigned long TICK_MS = 90;

// The walls are the outermost ring of the display; the dude lives inside it.
constexpr int FLOOR = 14;
constexpr int MIN_X = 2;  // leftmost center column of a 3px wide pose
constexpr int MAX_X = 13; // rightmost
constexpr int OUTSIDE_X = 18;
constexpr int MAX_TALLIES = 5;

constexpr uint8_t WALL = 28;
constexpr uint8_t WALL_FLASH = 170;
constexpr uint8_t TALLY = 70;
constexpr uint8_t BALL = 150;
constexpr uint8_t HEAD = 255;
constexpr uint8_t BODY = 160;
constexpr uint8_t THOUGHT = 110;

// Poses face right and are mirrored when he looks left. 'o' is the head, '#' the body.
const Sprite STAND = {3, 4, {".o.", "###", ".#.", "#.#"}};
const Sprite STEP = {3, 4, {".o.", "###", ".#.", ".#."}};
const Sprite ARMS_UP = {3, 4, {"#o#", ".#.", ".#.", "#.#"}};
const Sprite KNOCK_BACK = {3, 4, {".o.", "##.", ".#.", "#.#"}};
const Sprite KNOCK_HIT = {3, 4, {".o.", ".##", ".#.", "#.#"}};
const Sprite PUSH = {3, 4, {"..o", ".##", ".#.", "#.."}};
const Sprite SIT = {3, 3, {".o.", "###", ".##"}};
const Sprite LIE = {4, 1, {"o###"}};

// Thought glyphs, drawn above his head.
const Sprite QUESTION = {3, 5, {"##.", "..#", ".#.", "...", ".#."}};
const Sprite EXCLAIM = {1, 5, {"#", "#", "#", ".", "#"}};
const Sprite HEART = {3, 3, {"#.#", "###", ".#."}};
const Sprite ZED = {3, 3, {"##.", ".#.", ".##"}};

int sign(int value)
{
  return (value > 0) - (value < 0);
}
} // namespace

void BoxDudePlugin::setup()
{
  Screen.clear();
  timer.forceReady();

  x = 7;
  facing = 1;
  lift = 0;
  stepFlip = false;
  pose = &STAND;
  outside = false;

  boredom = 0;
  tallies = 0;
  holeSize = 0;
  hasBall = false;
  for (Zzz &z : zzz)
  {
    z.life = 0;
  }

  thought = nullptr;
  thoughtTicks = 0;
  thinkingDots = 0;
  shakeTicks = 0;
  flashTicks = 0;
  flashCeiling = false;
  flashRightWall = false;

  startAct(Act::Idle);
}

void BoxDudePlugin::loop()
{
  if (!timer.isReady(TICK_MS))
  {
    return;
  }

  switch (act)
  {
  case Act::Idle:
    actIdle();
    break;
  case Act::Walk:
    actWalk();
    break;
  case Act::Jump:
    actJump();
    break;
  case Act::Sit:
    actSit();
    break;
  case Act::Sleep:
    actSleep();
    break;
  case Act::Knock:
    actKnock();
    break;
  case Act::Ball:
    actBall();
    break;
  case Act::Tally:
    actTally();
    break;
  case Act::Escape:
    actEscape();
    break;
  }

  tick++;
  phaseTick++;
  updateBall();
  updateZzz();
  if (thoughtTicks > 0 && --thoughtTicks == 0)
  {
    thought = nullptr;
  }

  render();
}

const char *BoxDudePlugin::getName() const
{
  return "Box Dude";
}

// --- behaviour -------------------------------------------------------------

void BoxDudePlugin::chooseNextAct()
{
  if (tallies >= MAX_TALLIES)
  {
    startAct(Act::Escape);
    return;
  }

  // the more bored he gets, the more he naps, knocks and counts days
  const struct
  {
    Act act;
    int weight;
  } options[] = {
      {Act::Idle, 3},
      {Act::Walk, 4},
      {Act::Jump, 2},
      {Act::Ball, max(0, 4 - boredom / 25)},
      {Act::Sit, 1 + boredom / 40},
      {Act::Knock, boredom / 30},
      {Act::Sleep, boredom / 35},
      {Act::Tally, boredom > 75 ? 1 : 0},
  };

  int total = 0;
  for (const auto &option : options)
  {
    total += option.weight;
  }

  int pick = random(total);
  for (const auto &option : options)
  {
    pick -= option.weight;
    if (pick < 0)
    {
      startAct(option.act);
      return;
    }
  }
}

void BoxDudePlugin::startAct(Act next)
{
  act = next;
  tick = 0;
  phase = 0;
  phaseTick = 0;
  lift = 0;
  thinkingDots = 0;
  answered = false;
  pose = &STAND;

  switch (act)
  {
  case Act::Idle:
    length = random(20, 50);
    break;
  case Act::Walk:
    do
    {
      targetX = random(MIN_X, MAX_X + 1);
    } while (abs(targetX - x) < 3);
    break;
  case Act::Sit:
    length = random(60, 130);
    break;
  case Act::Sleep:
    length = random(110, 180);
    break;
  case Act::Ball:
    length = random(120, 220);
    break;
  case Act::Knock:
    facing = random(2) ? 1 : -1;
    targetX = facing > 0 ? MAX_X : MIN_X;
    break;
  case Act::Tally:
    // four upright marks side by side, the fifth one strikes them through
    targetX = tallies < 4 ? constrain(4 + 2 * tallies, MIN_X, MAX_X) : 7;
    break;
  default:
    break;
  }
}

void BoxDudePlugin::nextPhase()
{
  phase++;
  phaseTick = 0;
}

bool BoxDudePlugin::walkTowards(int tx, int every)
{
  if (x == tx)
  {
    pose = &STAND;
    return true;
  }

  facing = sign(tx - x);
  if (phaseTick % every == 0)
  {
    x += facing;
    stepFlip = !stepFlip;
  }
  pose = stepFlip ? &STEP : &STAND;
  return false;
}

// Parabolic hop over phaseTick; returns true once he has landed.
bool BoxDudePlugin::jumpArc(int peak)
{
  int duration = peak * 2 + 2;
  float p = static_cast<float>(phaseTick) / duration;
  lift = static_cast<int>(roundf(peak * 4.0f * p * (1.0f - p)));
  pose = lift > 0 ? &ARMS_UP : &STAND;
  return phaseTick >= duration;
}

void BoxDudePlugin::think(const Sprite *glyph, int ticks)
{
  thought = glyph;
  thoughtTicks = ticks;
}

void BoxDudePlugin::actIdle()
{
  // taps his foot
  pose = (tick / 5) % 4 == 3 ? &STEP : &STAND;
  if (tick % 2 == 0)
  {
    boredom = min(100, boredom + 1);
  }
  if (tick == 10 && random(4) == 0)
  {
    facing = -facing; // looks around
  }
  if (tick >= length)
  {
    chooseNextAct();
  }
}

void BoxDudePlugin::actWalk()
{
  if (walkTowards(targetX))
  {
    chooseNextAct();
  }
}

void BoxDudePlugin::actJump()
{
  // a bored dude jumps hard enough to hit the ceiling
  bool frustrated = boredom > 50;
  int peak = frustrated ? 10 : 3;

  bool landed = jumpArc(peak);
  if (frustrated && phaseTick == peak + 1) // top of the arc, head against the ceiling
  {
    flashCeiling = true;
    flashTicks = 3;
    shakeTicks = 3;
    think(&EXCLAIM, 12);
  }
  if (landed)
  {
    if (frustrated)
    {
      boredom -= 10;
    }
    chooseNextAct();
  }
}

void BoxDudePlugin::actSit()
{
  pose = &SIT;
  if (tick % 2 == 0)
  {
    boredom = min(100, boredom + 1);
  }
  // "..." appearing one dot at a time
  if (tick > 15)
  {
    thinkingDots = ((tick - 15) / 10) % 4;
  }
  if (tick >= length)
  {
    chooseNextAct();
  }
}

void BoxDudePlugin::actSleep()
{
  pose = tick < 12 ? &SIT : &LIE;

  if (tick >= 12 && tick % 18 == 0)
  {
    for (Zzz &z : zzz)
    {
      if (z.life <= 0)
      {
        z = {static_cast<float>(x + facing), static_cast<float>(FLOOR - 3), 40};
        break;
      }
    }
  }

  if (tick >= length)
  {
    boredom = max(0, boredom - 50);
    think(&EXCLAIM, 8);
    chooseNextAct();
  }
}

void BoxDudePlugin::actKnock()
{
  const int wallX = facing > 0 ? 15 : 0;

  switch (phase)
  {
  case 0: // walk up to the wall
    if (walkTowards(targetX))
    {
      nextPhase();
    }
    break;

  case 1: // knock-knock-knock ... knock-knock-knock
  {
    static const int knocks[] = {0, 4, 8, 20, 24, 28};
    pose = &KNOCK_BACK;
    for (int knockAt : knocks)
    {
      if (phaseTick == knockAt || phaseTick == knockAt + 1)
      {
        pose = &KNOCK_HIT;
      }
      if (phaseTick == knockAt)
      {
        flashX = wallX;
        flashY = FLOOR - 2;
        flashTicks = 2;
      }
    }
    if (phaseTick >= 40)
    {
      answered = random(100) < 8;
      think(&QUESTION, 34);
      nextPhase();
    }
    break;
  }

  case 2: // listen
    pose = &STAND;
    if (answered && (phaseTick == 12 || phaseTick == 16 || phaseTick == 20))
    {
      // somebody on the other side knocks back
      flashX = wallX;
      flashY = FLOOR - 4;
      flashTicks = 2;
    }
    if (phaseTick >= 36)
    {
      if (answered)
      {
        think(&HEART, 30);
        boredom = max(0, boredom - 40);
        startAct(Act::Jump);
      }
      else
      {
        boredom = min(100, boredom + 5);
        chooseNextAct();
      }
    }
    break;
  }
}

void BoxDudePlugin::actBall()
{
  if (!hasBall)
  {
    spawnBall();
  }

  int ballColumn = static_cast<int>(roundf(ballX));
  bool nearBall = abs(ballColumn - x) <= 1 && ballY >= FLOOR - 2;

  if (nearBall && phaseTick > 6)
  {
    // kick it towards the middle of the box
    facing = ballX < 8 ? 1 : -1;
    ballVX = facing * (0.45f + random(40) / 100.0f);
    ballVY = -(0.8f + random(60) / 100.0f);
    pose = &KNOCK_HIT;
    phaseTick = 0;
  }
  else if (phaseTick > 2)
  {
    walkTowards(constrain(ballColumn, MIN_X, MAX_X), 2);
  }

  if (tick % 3 == 0)
  {
    boredom = max(0, boredom - 1);
  }
  if (tick >= length)
  {
    chooseNextAct();
  }
}

void BoxDudePlugin::actTally()
{
  switch (phase)
  {
  case 0:
    if (walkTowards(targetX))
    {
      nextPhase();
    }
    break;

  case 1: // jump up and scratch a mark into the wall at the top of the arc
  {
    bool landed = jumpArc(7);
    if (phaseTick == 8)
    {
      tallies++;
      flashX = tallies < 5 ? 4 + 2 * (tallies - 1) : 7;
      flashY = 3;
      flashTicks = 2;
    }
    if (landed)
    {
      nextPhase();
    }
    break;
  }

  case 2: // admire the count
    pose = &STAND;
    if (phaseTick >= 20)
    {
      chooseNextAct();
    }
    break;
  }
}

void BoxDudePlugin::actEscape()
{
  switch (phase)
  {
  case 0: // realises something
    pose = &ARMS_UP;
    if (phaseTick == 0)
    {
      think(&EXCLAIM, 16);
    }
    if (phaseTick >= 16)
    {
      nextPhase();
    }
    break;

  case 1:
    if (walkTowards(MAX_X, 2))
    {
      nextPhase();
    }
    break;

  case 2: // pushes against the right wall until it gives
    facing = 1;
    pose = &PUSH;
    if (phaseTick % 12 == 0)
    {
      shakeTicks = 2;
      flashRightWall = true;
      flashTicks = 2;
    }
    if (phaseTick == 24 || phaseTick == 40 || phaseTick == 56)
    {
      holeSize++;
    }
    if (phaseTick >= 64)
    {
      nextPhase();
    }
    break;

  case 3: // out he goes
    if (walkTowards(OUTSIDE_X))
    {
      outside = true;
      nextPhase();
    }
    break;

  case 4: // an empty box, the marks slowly fade
    if (phaseTick % 30 == 29 && tallies > 0)
    {
      tallies--;
    }
    if (phaseTick >= 170)
    {
      outside = false;
      nextPhase();
    }
    break;

  case 5: // turns out the outside is boring too
    if (walkTowards(10))
    {
      think(&QUESTION, 30);
      nextPhase();
    }
    break;

  case 6: // the wall grows back
    pose = &STAND;
    facing = 1;
    if (phaseTick % 15 == 14 && holeSize > 0)
    {
      holeSize--;
    }
    if (holeSize == 0 && phaseTick > 30)
    {
      tallies = 0;
      boredom = 0;
      chooseNextAct();
    }
    break;
  }
}

// --- props -----------------------------------------------------------------

void BoxDudePlugin::spawnBall()
{
  hasBall = true;
  ballX = random(3, 13);
  ballY = 1;
  ballVX = 0;
  ballVY = 0;
}

void BoxDudePlugin::updateBall()
{
  if (!hasBall)
  {
    return;
  }

  ballVY += 0.12f;
  ballX += ballVX;
  ballY += ballVY;

  if (ballX < 1 || ballX > 14)
  {
    ballX = constrain(ballX, 1.0f, 14.0f);
    ballVX = -ballVX * 0.8f;
  }
  if (ballY < 1)
  {
    ballY = 1;
    ballVY = -ballVY * 0.6f;
  }
  if (ballY > FLOOR)
  {
    ballY = FLOOR;
    ballVY = fabsf(ballVY) < 0.35f ? 0 : -ballVY * 0.55f;
    ballVX *= 0.9f;
  }
}

void BoxDudePlugin::updateZzz()
{
  for (Zzz &z : zzz)
  {
    if (z.life > 0)
    {
      z.life--;
      z.y -= 0.18f;
      z.x += 0.08f * facing;
    }
  }
}

// --- drawing ---------------------------------------------------------------

void BoxDudePlugin::plot(int px, int py, uint8_t brightness)
{
  px += shakeTicks > 0 ? ((tick & 1) ? 1 : -1) : 0;
  if (px < 0 || px > 15 || py < 0 || py > 15)
  {
    return;
  }
  uint8_t &pixel = frame[py * 16 + px];
  pixel = max(pixel, brightness);
}

void BoxDudePlugin::drawSprite(const Sprite &sprite,
                               int left,
                               int top,
                               bool mirror,
                               uint8_t head,
                               uint8_t body)
{
  for (int row = 0; row < sprite.height; row++)
  {
    for (int col = 0; col < sprite.width; col++)
    {
      char c = sprite.rows[row][mirror ? sprite.width - 1 - col : col];
      if (c != '.')
      {
        plot(left + col, top + row, c == 'o' ? head : body);
      }
    }
  }
}

void BoxDudePlugin::drawLine(int x0, int y0, int x1, int y1, uint8_t brightness)
{
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true)
  {
    plot(x0, y0, brightness);
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy)
    {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx)
    {
      err += dx;
      y0 += sy;
    }
  }
}

void BoxDudePlugin::render()
{
  memset(frame, 0, sizeof(frame));

  bool flashing = flashTicks > 0;

  // walls, with the hole he breaks into the lower right
  for (int i = 0; i < 16; i++)
  {
    plot(i, 0, flashing && flashCeiling ? WALL_FLASH : WALL);
    plot(i, 15, WALL);
    plot(0, i, WALL);
    bool isHole = i >= 15 - holeSize && i < 15;
    if (!isHole)
    {
      plot(15, i, flashing && flashRightWall ? WALL_FLASH : WALL);
    }
  }

  // tally marks
  for (int i = 0; i < min(tallies, 4); i++)
  {
    drawLine(4 + 2 * i, 2, 4 + 2 * i, 4, TALLY);
  }
  if (tallies >= 5)
  {
    drawLine(3, 4, 11, 2, TALLY);
  }

  if (hasBall)
  {
    plot(static_cast<int>(roundf(ballX)), static_cast<int>(roundf(ballY)), BALL);
  }

  int top = FLOOR - lift - pose->height + 1;
  if (!outside)
  {
    int left = x - pose->width / 2;
    drawSprite(*pose, left, top, facing < 0, HEAD, BODY);
  }

  if (thought && !outside)
  {
    int left = constrain(x - thought->width / 2, 1, 15 - thought->width);
    drawSprite(*thought, left, max(1, top - thought->height - 1), false, THOUGHT, THOUGHT);
  }
  int dotsLeft = constrain(x - 2, 1, 10);
  for (int i = 0; i < thinkingDots; i++)
  {
    plot(dotsLeft + 2 * i, top - 2, THOUGHT);
  }

  for (const Zzz &z : zzz)
  {
    if (z.life > 0)
    {
      uint8_t fade = static_cast<uint8_t>(min(THOUGHT, static_cast<uint8_t>(z.life * 4)));
      drawSprite(ZED, static_cast<int>(z.x), static_cast<int>(z.y), false, fade, fade);
    }
  }

  // a single knocked wall pixel lights up
  if (flashing && !flashCeiling && !flashRightWall)
  {
    plot(flashX, flashY, WALL_FLASH);
  }

  if (flashTicks > 0 && --flashTicks == 0)
  {
    flashCeiling = false;
    flashRightWall = false;
  }
  if (shakeTicks > 0)
  {
    shakeTicks--;
  }

  Screen.setRenderBuffer(frame, true);
}
