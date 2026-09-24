#pragma once

#include "PluginManager.h"
#include "timing.h"

// A tiny prisoner in a 16x16 box, animated procedurally. He wanders around, kicks
// a ball, knocks on the walls (very rarely somebody knocks back), naps, and scratches
// tally marks on the wall. After the fifth mark he breaks out through the wall,
// finds out there is nothing to do out there either, and comes back.
class BoxDudePlugin : public Plugin
{
public:
  struct Sprite
  {
    uint8_t width;
    uint8_t height;
    const char *rows[5];
  };

  void setup() override;
  void loop() override;
  const char *getName() const override;

private:
  enum class Act
  {
    Idle,
    Walk,
    Jump,
    Sit,
    Sleep,
    Knock,
    Ball,
    Tally,
    Escape,
  };

  struct Zzz
  {
    float x;
    float y;
    int life;
  };

  NonBlockingDelay timer;
  uint8_t frame[16 * 16];

  // the dude
  int x;
  int facing;
  int lift;
  bool stepFlip;
  const Sprite *pose;
  bool outside;

  // what he is doing
  Act act;
  int tick;
  int phase;
  int phaseTick;
  int length;
  int targetX;
  int boredom;
  bool answered;

  // the room
  int tallies;
  int holeSize;
  bool hasBall;
  float ballX, ballY, ballVX, ballVY;
  Zzz zzz[2];

  // effects
  const Sprite *thought;
  int thoughtTicks;
  int thinkingDots;
  int shakeTicks;
  int flashTicks;
  int flashX, flashY;
  bool flashCeiling;
  bool flashRightWall;

  void chooseNextAct();
  void startAct(Act next);
  void nextPhase();
  bool walkTowards(int tx, int every = 3);
  bool jumpArc(int peak);
  void think(const Sprite *glyph, int ticks);
  void spawnBall();
  void updateBall();
  void updateZzz();

  void actIdle();
  void actWalk();
  void actJump();
  void actSit();
  void actSleep();
  void actKnock();
  void actBall();
  void actTally();
  void actEscape();

  void render();
  void plot(int px, int py, uint8_t brightness);
  void drawSprite(const Sprite &sprite, int left, int top, bool mirror, uint8_t head, uint8_t body);
  void drawLine(int x0, int y0, int x1, int y1, uint8_t brightness);
};
