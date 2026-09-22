



// ============================================================
// Flappy Stick 2.0 - M5StickS3
// One-button Flappy Bird-style game
//
// IN GAME:
//   A or B = flap
//
// MENUS:
//   A = start / retry
//   B = sound on / off
//
// REQUIRED LIBRARIES:
//   M5Unified
//   Preferences
// ============================================================

#include <M5Unified.h>
#include <Preferences.h>

M5Canvas canvas(&M5.Display);
Preferences prefs;

// ------------------------------------------------------------
// RGB565 helper
// ------------------------------------------------------------

#define RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

// ------------------------------------------------------------
// Screen
// ------------------------------------------------------------

const int SCREEN_W = 240;
const int SCREEN_H = 135;

const int GROUND_H = 10;
const int GROUND_Y = SCREEN_H - GROUND_H;

const int BIRD_X = 50;
const int BIRD_R = 6;

const int PIPE_W = 26;
const int SPACING = 95;
const int NUM_PIPES = 3;

// ------------------------------------------------------------
// Physics
// ------------------------------------------------------------

const float GRAVITY = 0.32f;
const float FLAP_POWER = -4.0f;

const uint32_t DEAD_DELAY = 700;

// ------------------------------------------------------------
// Colours
// ------------------------------------------------------------

const uint16_t SKIES[] = {
  RGB565(100, 190, 255),   // Day
  RGB565(255, 150, 100),   // Sunset
  RGB565(15, 20, 60)       // Night
};

const uint16_t CLOUD_COLORS[] = {
  RGB565(255, 255, 255),
  RGB565(255, 210, 180),
  RGB565(60, 70, 110)
};

const uint16_t PIPE_COLOR = RGB565(60, 180, 60);
const uint16_t PIPE_DARK  = RGB565(30, 120, 30);

const uint16_t DIRT      = RGB565(222, 184, 135);
const uint16_t DIRT_DARK = RGB565(170, 130, 80);
const uint16_t GRASS     = RGB565(80, 200, 60);

const uint16_t PANEL_BG = RGB565(30, 30, 40);
const uint16_t WING     = RGB565(255, 170, 0);

// ------------------------------------------------------------
// Game states
// ------------------------------------------------------------

enum State {
  WAITING,
  PLAYING,
  DEAD
};

State gameState = WAITING;

// ------------------------------------------------------------
// Game variables
// ------------------------------------------------------------

float birdY = 0;
float birdV = 0;

float pipeX[NUM_PIPES];

int gapY[NUM_PIPES];
int gapH[NUM_PIPES];

bool passed[NUM_PIPES];

int score = 0;
int best = 0;

bool newBest = false;
bool muted = false;
bool bestJinglePlayed = false;

uint32_t deadAt = 0;
uint32_t milestoneAt = 0;

float groundOff = 0;

// ------------------------------------------------------------
// Clouds
// ------------------------------------------------------------

float cloudX[3] = {
  30,
  110,
  190
};

const int cloudY[3] = {
  18,
  40,
  12
};

const float cloudSpeed[3] = {
  0.25f,
  0.40f,
  0.30f
};

// ------------------------------------------------------------
// Sound system
// ------------------------------------------------------------

struct Beep {
  uint16_t freq;
  uint16_t ms;
};

// Flap
const Beep SND_FLAP[] = {
  {600, 25},
  {900, 35}
};

// Score
const Beep SND_SCORE[] = {
  {1100, 40},
  {1500, 70}
};

// Every 10 points
const Beep SND_LEVEL[] = {
  {800, 70},
  {1000, 70},
  {1200, 70},
  {1600, 150}
};

// Death
const Beep SND_DIE[] = {
  {500, 100},
  {420, 100},
  {340, 100},
  {260, 100},
  {150, 250}
};

// New best
const Beep SND_BEST[] = {
  {800, 90},
  {1000, 90},
  {1200, 90},
  {0, 40},
  {1200, 90},
  {1600, 250}
};

// Menu blip
const Beep SND_BLIP[] = {
  {900, 40}
};

const Beep* curSound = nullptr;

int curLen = 0;
int curIdx = 0;
int curPriority = 0;

uint32_t soundNextAt = 0;

// ------------------------------------------------------------
// Start a sound
// ------------------------------------------------------------

void playSound(const Beep* s, int len, int priority) {

  if (muted)
    return;

  // Don't interrupt a more important sound
  if (curSound && priority < curPriority)
    return;

  curSound = s;
  curLen = len;
  curIdx = 0;
  curPriority = priority;
  soundNextAt = 0;
}

// ------------------------------------------------------------
// Sound macro
// ------------------------------------------------------------

#define PLAY_SOUND(snd, pri) \
  playSound(snd, sizeof(snd) / sizeof(snd[0]), pri)

// ------------------------------------------------------------
// Update sound
// ------------------------------------------------------------

void updateSound() {

  if (!curSound)
    return;

  if (millis() < soundNextAt)
    return;

  if (curIdx >= curLen) {

    curSound = nullptr;
    curLen = 0;
    curIdx = 0;

    return;
  }

  Beep n = curSound[curIdx++];

  if (n.freq) {
    M5.Speaker.tone(n.freq, n.ms);
  }

  soundNextAt = millis() + n.ms;
}

// ------------------------------------------------------------
// Create a new pipe
// ------------------------------------------------------------

void newPipe(int i, float x) {

  pipeX[i] = x;

  // Gap gets smaller as score increases
  gapH[i] = max(42, 52 - score / 3);

  gapY[i] = random(
    12,
    GROUND_Y - gapH[i] - 12
  );

  passed[i] = false;
}

// ------------------------------------------------------------
// Reset game
// ------------------------------------------------------------

void resetGame() {

  birdY = GROUND_Y / 2.0f;
  birdV = 0;

  score = 0;

  newBest = false;
  bestJinglePlayed = false;

  for (int i = 0; i < NUM_PIPES; i++) {

    newPipe(
      i,
      SCREEN_W + 40 + i * SPACING
    );
  }
}

// ------------------------------------------------------------
// Flap
// ------------------------------------------------------------

void flap() {

  birdV = FLAP_POWER;

  PLAY_SOUND(SND_FLAP, 0);
}

// ------------------------------------------------------------
// Player dies
// ------------------------------------------------------------

void die() {

  if (gameState == DEAD)
    return;

  gameState = DEAD;

  deadAt = millis();

  newBest = score > best;

  if (newBest) {

    best = score;

    prefs.putInt("best", best);
  }

  if (birdY > GROUND_Y - BIRD_R) {

    birdY = GROUND_Y - BIRD_R;
  }

  // Small hop if we hit a pipe
  birdV =
    (birdY < GROUND_Y - BIRD_R - 1)
    ? -2.5f
    : 0;

  PLAY_SOUND(SND_DIE, 3);
}

// ------------------------------------------------------------
// Game update
// ------------------------------------------------------------

void stepGame() {

  // Gravity
  birdV += GRAVITY;
  birdY += birdV;

  // Ceiling
  if (birdY < BIRD_R) {

    birdY = BIRD_R;
    birdV = 0;
  }

  // Ground
  if (birdY > GROUND_Y - BIRD_R) {

    birdY = GROUND_Y - BIRD_R;

    die();

    return;
  }

  // Game speed increases with score
  float speed =
    fminf(
      3.2f,
      2.2f + score * 0.04f
    );

  groundOff += speed;

  // Pipes
  for (int i = 0; i < NUM_PIPES; i++) {

    pipeX[i] -= speed;

    // Pipe went off-screen
    if (pipeX[i] < -PIPE_W - 4) {

      float farthest = pipeX[0];

      for (int j = 1; j < NUM_PIPES; j++) {

        farthest =
          fmaxf(
            farthest,
            pipeX[j]
          );
      }

      newPipe(
        i,
        farthest + SPACING
      );
    }

    // Passed pipe
    if (
      !passed[i] &&
      pipeX[i] + PIPE_W <
      BIRD_X - BIRD_R
    ) {

      passed[i] = true;

      score++;

      // Every 10 points
      if (score % 10 == 0) {

        milestoneAt = millis();

        PLAY_SOUND(
          SND_LEVEL,
          2
        );
      }
      else {

        PLAY_SOUND(
          SND_SCORE,
          1
        );
      }
    }

    // Collision
    bool overlapX =
      (BIRD_X + BIRD_R - 1 >
       pipeX[i] - 2)
      &&
      (BIRD_X - BIRD_R + 1 <
       pipeX[i] + PIPE_W + 2);

    bool hitPipe =
      overlapX &&
      (
        birdY - BIRD_R + 1 <
        gapY[i]
        ||
        birdY + BIRD_R - 1 >
        gapY[i] + gapH[i]
      );

    if (hitPipe) {

      die();

      return;
    }
  }
}

// ------------------------------------------------------------
// Dead update
// ------------------------------------------------------------

void stepDead() {

  if (birdY < GROUND_Y - BIRD_R) {

    birdV += GRAVITY;

    birdY += birdV;

    if (birdY > GROUND_Y - BIRD_R) {

      birdY =
        GROUND_Y - BIRD_R;
    }
  }
}

// ------------------------------------------------------------
// Scenery update
// ------------------------------------------------------------

void stepScenery() {

  for (int i = 0; i < 3; i++) {

    cloudX[i] -= cloudSpeed[i];

    if (cloudX[i] < -40) {

      cloudX[i] =
        SCREEN_W + 40;
    }
  }

  // Ground moves on title screen
  if (gameState == WAITING) {

    groundOff += 1.0f;
  }
}

// ============================================================
// DRAWING
// ============================================================

// ------------------------------------------------------------
// Center text
// ------------------------------------------------------------

void centerText(
  const String& s,
  int y,
  uint16_t color
) {

  canvas.setTextDatum(
    MC_DATUM
  );

  canvas.setTextColor(
    color
  );

  canvas.drawString(
    s,
    SCREEN_W / 2,
    y
  );
}

// ------------------------------------------------------------
// Draw cloud
// ------------------------------------------------------------

void drawCloud(
  int x,
  int y,
  uint16_t c
) {

  canvas.fillCircle(
    x,
    y,
    8,
    c
  );

  canvas.fillCircle(
    x + 10,
    y - 4,
    10,
    c
  );

  canvas.fillCircle(
    x + 22,
    y,
    8,
    c
  );

  canvas.fillRect(
    x,
    y,
    22,
    8,
    c
  );
}

// ------------------------------------------------------------
// Draw pipe
// ------------------------------------------------------------

void drawPipe(int i) {

  int x =
    (int)pipeX[i];

  int topH =
    gapY[i];

  int botY =
    gapY[i] + gapH[i];

  // Top pipe
  if (topH > 0) {

    canvas.fillRect(
      x,
      0,
      PIPE_W,
      topH,
      PIPE_COLOR
    );

    canvas.drawRect(
      x,
      0,
      PIPE_W,
      topH,
      PIPE_DARK
    );
  }

  // Bottom pipe
  if (botY < GROUND_Y) {

    canvas.fillRect(
      x,
      botY,
      PIPE_W,
      GROUND_Y - botY,
      PIPE_COLOR
    );

    canvas.drawRect(
      x,
      botY,
      PIPE_W,
      GROUND_Y - botY,
      PIPE_DARK
    );
  }

  // Top cap
  canvas.fillRect(
    x - 2,
    topH - 7,
    PIPE_W + 4,
    7,
    PIPE_COLOR
  );

  canvas.drawRect(
    x - 2,
    topH - 7,
    PIPE_W + 4,
    7,
    PIPE_DARK
  );

  // Bottom cap
  canvas.fillRect(
    x - 2,
    botY,
    PIPE_W + 4,
    7,
    PIPE_COLOR
  );

  canvas.drawRect(
    x - 2,
    botY,
    PIPE_W + 4,
    7,
    PIPE_DARK
  );
}

// ------------------------------------------------------------
// Draw ground
// ------------------------------------------------------------

void drawGround() {

  canvas.fillRect(
    0,
    GROUND_Y,
    SCREEN_W,
    GROUND_H,
    DIRT
  );

  // Grass
  canvas.fillRect(
    0,
    GROUND_Y,
    SCREEN_W,
    3,
    GRASS
  );

  int off =
    (int)groundOff % 16;

  for (
    int x = -off;
    x < SCREEN_W;
    x += 16
  ) {

    canvas.fillRect(
      x,
      GROUND_Y + 5,
      8,
      2,
      DIRT_DARK
    );
  }
}

// ------------------------------------------------------------
// Draw bird
// ------------------------------------------------------------

void drawBird() {

  int by =
    (int)birdY;

  // Body
  canvas.fillCircle(
    BIRD_X,
    by,
    BIRD_R,
    TFT_YELLOW
  );

  // Wing
  int wy;

  if (birdV < -1.0f) {

    wy = by - 3;
  }
  else if (birdV > 1.5f) {

    wy = by + 3;
  }
  else {

    wy = by + 1;
  }

  canvas.fillCircle(
    BIRD_X - 2,
    wy,
    3,
    WING
  );

  // Eye
  if (gameState == DEAD) {

    // X eyes
    canvas.drawLine(
      BIRD_X + 1,
      by - 4,
      BIRD_X + 4,
      by - 1,
      TFT_BLACK
    );

    canvas.drawLine(
      BIRD_X + 4,
      by - 4,
      BIRD_X + 1,
      by - 1,
      TFT_BLACK
    );
  }
  else {

    canvas.fillCircle(
      BIRD_X + 2,
      by - 2,
      2,
      TFT_WHITE
    );

    canvas.drawPixel(
      BIRD_X + 3,
      by - 2,
      TFT_BLACK
    );
  }

  // Beak
  canvas.fillTriangle(
    BIRD_X + 5,
    by - 1,

    BIRD_X + 5,
    by + 3,

    BIRD_X + 10,
    by + 1,

    RGB565(255, 130, 0)
  );
}

// ------------------------------------------------------------
// Draw sky
// ------------------------------------------------------------

void drawSky() {

  // Change sky depending on score
  int theme =
    (score / 15) % 3;

  canvas.fillScreen(
    SKIES[theme]
  );

  // Clouds
  for (int i = 0; i < 3; i++) {

    drawCloud(
      (int)cloudX[i],
      cloudY[i],
      CLOUD_COLORS[theme]
    );
  }
}

// ------------------------------------------------------------
// Draw score
// ------------------------------------------------------------

void drawScore() {

  canvas.setTextDatum(
    TC_DATUM
  );

  canvas.setTextColor(
    TFT_WHITE
  );

  canvas.setTextSize(2);

  canvas.drawString(
    String(score),
    SCREEN_W / 2,
    5
  );

  canvas.setTextSize(1);
}

// ------------------------------------------------------------
// Draw WAITING screen
// ------------------------------------------------------------

void drawWaiting() {

  drawSky();

  // Decorative bird
  birdY =
    GROUND_Y / 2.0f +
    sinf(millis() * 0.004f) * 4.0f;

  birdV = 0;

  drawBird();

  drawGround();

  // Title panel
  canvas.fillRoundRect(
    38,
    30,
    164,
    62,
    8,
    PANEL_BG
  );

  canvas.drawRoundRect(
    38,
    30,
    164,
    62,
    8,
    TFT_WHITE
  );

  canvas.setTextSize(2);

  centerText(
    "FLAPPY STICK",
    42,
    TFT_WHITE
  );

  canvas.setTextSize(1);

  centerText(
    "A = START",
    65,
    TFT_YELLOW
  );

  centerText(
    "B = SOUND",
    78,
    muted ? TFT_RED : TFT_GREEN
  );

  centerText(
    muted ? "SOUND: OFF" : "SOUND: ON",
    105,
    TFT_WHITE
  );

  canvas.setTextSize(1);
}

// ------------------------------------------------------------
// Draw playing screen
// ------------------------------------------------------------

void drawPlaying() {

  drawSky();

  // Pipes
  for (int i = 0; i < NUM_PIPES; i++) {

    drawPipe(i);
  }

  drawGround();

  drawBird();

  drawScore();
}

// ------------------------------------------------------------
// Draw game over
// ------------------------------------------------------------

void drawDead() {

  drawSky();

  for (int i = 0; i < NUM_PIPES; i++) {

    drawPipe(i);
  }

  drawGround();

  drawBird();

  // Dark panel
  canvas.fillRoundRect(
    35,
    24,
    170,
    87,
    8,
    PANEL_BG
  );

  canvas.drawRoundRect(
    35,
    24,
    170,
    87,
    8,
    TFT_WHITE
  );

  canvas.setTextSize(2);

  centerText(
    "GAME OVER",
    34,
    TFT_RED
  );

  canvas.setTextSize(1);

  centerText(
    "SCORE: " + String(score),
    61,
    TFT_WHITE
  );

  centerText(
    "BEST: " + String(best),
    75,
    TFT_YELLOW
  );

  if (newBest) {

    centerText(
      "NEW BEST!",
      89,
      TFT_GREEN
    );
  }
  else {

    centerText(
      "A = RETRY",
      89,
      TFT_WHITE
    );
  }

  canvas.setTextSize(1);
}

// ------------------------------------------------------------
// Draw everything
// ------------------------------------------------------------

void drawGame() {

  switch (gameState) {

    case WAITING:
      drawWaiting();
      break;

    case PLAYING:
      drawPlaying();
      break;

    case DEAD:
      drawDead();
      break;
  }
}

// ============================================================
// INPUT
// ============================================================

// ------------------------------------------------------------
// Button A
// ------------------------------------------------------------

void buttonA() {

  if (gameState == WAITING) {

    resetGame();

    gameState = PLAYING;

    PLAY_SOUND(
      SND_BLIP,
      0
    );

    return;
  }

  if (gameState == PLAYING) {

    flap();

    return;
  }

  if (gameState == DEAD) {

    if (millis() - deadAt >= DEAD_DELAY) {

      resetGame();

      gameState = PLAYING;

      PLAY_SOUND(
        SND_BLIP,
        0
      );
    }
  }
}

// ------------------------------------------------------------
// Button B
// ------------------------------------------------------------

void buttonB() {

  if (gameState == PLAYING) {

    flap();

    return;
  }

  // Toggle sound
  muted = !muted;

  prefs.putBool(
    "muted",
    muted
  );

  if (!muted) {

    PLAY_SOUND(
      SND_BLIP,
      0
    );
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  auto cfg =
    M5.config();

  M5.begin(cfg);

  // Display
  M5.Display.setRotation(1);

  // Create canvas
  canvas.createSprite(
    SCREEN_W,
    SCREEN_H
  );

  canvas.setTextFont(1);
  canvas.setTextSize(1);

  // Random seed
  randomSeed(
    millis()
  );

  // Preferences
  prefs.begin(
    "flappystick",
    false
  );

  best =
    prefs.getInt(
      "best",
      0
    );

  muted =
    prefs.getBool(
      "muted",
      false
    );

  // Initial game
  resetGame();

  gameState =
    WAITING;
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  M5.update();

  // ----------------------------------------------------------
  // Buttons
  // ----------------------------------------------------------

  if (M5.BtnA.wasPressed()) {

    buttonA();
  }

  if (M5.BtnB.wasPressed()) {

    buttonB();
  }

  // ----------------------------------------------------------
  // Game logic
  // ----------------------------------------------------------

  if (gameState == PLAYING) {

    stepGame();
  }
  else if (gameState == DEAD) {

    stepDead();
  }

  stepScenery();

  // ----------------------------------------------------------
  // Sound
  // ----------------------------------------------------------

  updateSound();

  // ----------------------------------------------------------
  // Drawing
  // ----------------------------------------------------------

  drawGame();

  canvas.pushSprite(
    0,
    0
  );

  // ----------------------------------------------------------
  // Frame rate
  // ----------------------------------------------------------

  delay(16);
}