#include <stdlib.h> // Provides malloc, which is needed in init_block_map()

/***************************************************************************************************
 * DON'T REMOVE THE VARIABLES BELOW THIS COMMENT                                                   *
 **************************************************************************************************/
unsigned long long __attribute__((used)) VGAaddress = 0xc8000000; // Memory storing pixels
unsigned int __attribute__((used)) red = 0x0000F0F0;
unsigned int __attribute__((used)) green = 0x00000F0F;
unsigned int __attribute__((used)) blue = 0x000000FF;
unsigned int __attribute__((used)) white = 0x0000FFFF;
unsigned int __attribute__((used)) black = 0x0;

const unsigned char n_cols = 10; // <- This variable might change depending on the size of the game. Supported value range: [1,18]

char *won = "You Won";             // DON'T TOUCH THIS - keep the string as is
char *lost = "You Lost";           // DON'T TOUCH THIS - keep the string as is
const unsigned short height = 240; // DON'T TOUCH THIS - keep the value as is
const unsigned short width = 320;  // DON'T TOUCH THIS - keep the value as is
char font8x8[128][8];              // DON'T TOUCH THIS - this is a forward declaration
/**************************************************************************************************/

#define TRUE 1
#define FALSE 0

#define BALL_MIN_Y (0)
#define BALL_MAX_Y (233)

#define BAR_HEIGHT 45 // px
#define BAR_STEP 15   // px

#define HorizontalVelocity 2
#define DiagonalVelocity 2

typedef enum _gameState {
  Stopped = 0,
  Running = 1,
  Won = 2,
  Lost = 3,
  Exit = 4,
} GameState;

typedef enum _hitType {
  NoHit = 0,
  TopHit,
  RightHit,
  LeftHit,
  BottomHit
} HitType;

typedef enum _direction {
  DiagonalUpRight = 45,
  HorizontalRight = 90,
  DiagonalDownRight = 135,
  DiagonalDownLeft = 225,
  HorizontalLeft = 270,
  DiagonalUpLeft = 315,
  NotApplicable = 6,
  VerticalDown = 0x73,
  VerticalUp = 0x77,
} Direction;

typedef struct _block {
  unsigned char destroyed;
  unsigned char deleted;
  unsigned int x_pos;
  unsigned int y_pos;
  unsigned int color;
} Block;

/// @brief Struct to track the ball position
typedef struct _movingObject {
  signed int x_pos;
  signed int y_pos;
  unsigned int x_pos_prev;
  unsigned int y_pos_prev;
  Direction direction;
} MovingObject;

GameState currentState = Stopped;

const unsigned short BlockSize = 15; // Size of a square block in px
const unsigned short BallSize = 7;   // Size of a square ball in px

unsigned int num_blocks;

Block *block_map;
MovingObject ball = {8, 117, 8, 117, HorizontalRight}; // Initial position of ball (x = gamebar.x + 1, y = height / 2 - ballheight / 2 )
MovingObject playerBar = {0, 98, 0, 98, NotApplicable};

/***
 * Here follow the C declarations for our assembly functions
 */

void ClearScreen();
void SetPixel(unsigned int x_coord, unsigned int y_coord, unsigned int color);
void DrawBlock(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color);
void DrawBar(unsigned int y, unsigned int color);
int ReadUart();
void WriteUart(char c);
void write(char *str);

/***
 * Now follow the assembly implementations
 */

asm("ClearScreen: \n\t"
    "PUSH {R4-R7, LR} \n\t"

    "MOV R4, #0 \n\t" // x-pos
    "MOV R5, #0 \n\t" // y-pos

    "LDR R2, =white \n\t"
    "LDR R2, [R2] \n\t"

    "LDR R6, =320 \n\t" // x-max
    "LDR R7, =240 \n\t" // y-max

    // Clear Screen
    "CsInnerLoop: \n\t"
    "MOV R0, R4 \n\t" // Prepare x-pos for SetPixel
    "MOV R1, R5 \n\t" // Prepare y-pos for SetPixel

    "BL SetPixel \n\t"

    "ADD R4, R4, #1 \n\t"
    "CMP R4, R6 \n\t"

    "BLT CsInnerLoop \n\t"

    "MOV R4, #0 \n\t"     // Reset x-pos
    "ADD R5, R5, #1 \n\t" // Increment y-pos

    "CMP R5, R7 \n\t"
    "BLT CsInnerLoop \n\t"

    "POP {R4-R7, LR} \n\t"
    "BX LR");

// assumes R0 = x-coord, R1 = y-coord, R2 = colorvalue
asm("SetPixel: \n\t"
    "LDR R3, =VGAaddress \n\t"
    "LDR R3, [R3] \n\t"
    "LSL R1, R1, #10 \n\t"
    "LSL R0, R0, #1 \n\t"
    "ADD R1, R0 \n\t"
    "STRH R2, [R3,R1] \n\t"
    "BX LR");

// R0 = x, R1 = y, R2 = width, R3 = height, STACK -> color
asm("DrawBlock: \n\t"
    "PUSH {R4-R9, LR} \n\t"
    "LDR R9, [SP, #28] \n\t" // Get the color from the stack

    "MOV R4, R0 \n\t"     // R4 = x-pointer
    "MOV R5, R1 \n\t"     // R5 = y-pointer
    "ADD R6, R0, R2 \n\t" // R6 = x-pos max
    "ADD R7, R1, R3 \n\t" // R7 = y-pos max

    "MOV R8, R0 \n\t" // Save x-pos in R8 since R4 and R0 will be modified
    "MOV R2, R9 \n\t" // Move the color value to R2

    "DrawBlockLoop: \n\t"
    "MOV R0, R4 \n\t" // Set x-pos
    "MOV R1, R5 \n\t" // Set y-pos
    // We dont need to set the color since it is already set
    "BL SetPixel \n\t"

    "ADD R4, R4, #1 \n\t" // Increment x-pos
    "CMP R4, R6 \n\t"     // Compare next x-pos with x-pos max
    "BLT DrawBlockLoop \n\t"

    "MOV R4, R8 \n\t"     // Reset the x-position for the next row iteration
    "ADD R5, R5, #1 \n\t" // Increment to the next row
    "CMP R5, R7 \n\t"     // Compare next y-pos with y-pos max
    "BLT DrawBlockLoop \n\t"

    "POP {R4-R9, LR} \n\t"
    "BX LR");

asm("DrawBar: \n\t"
    "PUSH {LR} \n\t"
    "PUSH {R4, R5, R6} \n\t"

    "MOV R4, R0 \n\t" // move y-pos to R4
    "MOV R5, #0 \n\t" // x-offset for the bar

    "MOV R2, R1 \n\t" // Move the color value to R2

    "ADD R6, R4, #45 \n\t" // y-pos maximum

    "DrawBarRow: \n\t"
    "MOV R0, R5 \n\t" // set x-pos argument for SetPixel
    "MOV R1, R4 \n\t" // set y-pos argument for SetPixel
    "BL SetPixel \n\t"

    "ADD R5, R5, #1 \n\t" // move to next pixel
    "CMP R5, #9 \n\t"
    "BLT DrawBarRow \n\t"

    "MOV R5, #0 \n\t"
    "ADD R4, R4, #1 \n\t" // move to next row
    "CMP R4, R6 \n\t"     // If current y-pos (R4) < y-pos-maximum (R6), loop. Else exit the function
    "BLT DrawBarRow \n\t"

    "POP {R4, R5, R6} \n\t"
    "POP {LR} \n\t"
    "BX LR");

asm("ReadUart:\n\t"
    "LDR R1, =0xFF201000 \n\t"
    "LDR R0, [R1]\n\t"
    "BX LR");

asm("WriteUart: \n\t"
    "LDR R1, =0xff201000 \n\t"
    "STRB R0, [R1] \n\t"
    "BX LR");

void draw_ball() {
  // Reset the pixels of the previous position
  for (unsigned int i = 0; i < BallSize; i++) {
    for (unsigned int j = 0; j < BallSize; j++) {
      SetPixel(ball.x_pos_prev + i, ball.y_pos_prev + j, white);
    }
  }

  // Draw the pixels of the current position
  for (unsigned int i = 0; i < BallSize; i++) {
    for (unsigned int j = 0; j < BallSize; j++) {
      SetPixel(ball.x_pos + i, ball.y_pos + j, black);
    }
  }
}

void update_ball_position() {
  ball.x_pos_prev = ball.x_pos;
  ball.y_pos_prev = ball.y_pos;

  signed int newYpos = 0;

  switch (ball.direction) {
  case HorizontalRight:
    ball.x_pos += HorizontalVelocity;
    break;

  case HorizontalLeft:
    ball.x_pos -= HorizontalVelocity;
    break;

  case DiagonalUpRight:
    ball.x_pos += DiagonalVelocity;

    newYpos = ball.y_pos - DiagonalVelocity;

    if (newYpos <= 0) {
      ball.y_pos = 0;
    } else {
      ball.y_pos = newYpos;
    }

    break;

  case DiagonalDownRight:
    ball.x_pos += DiagonalVelocity;

    newYpos = ball.y_pos + DiagonalVelocity;

    if (newYpos + BallSize >= BALL_MAX_Y) {
      ball.y_pos = BALL_MAX_Y;
    } else {
      ball.y_pos = newYpos;
    }
    break;

  case DiagonalUpLeft:
    ball.x_pos -= DiagonalVelocity;

    newYpos = ball.y_pos - DiagonalVelocity;

    if (newYpos <= 0) {
      ball.y_pos = 0;
    } else {
      ball.y_pos = newYpos;
    }

    break;

  case DiagonalDownLeft:
    ball.x_pos -= DiagonalVelocity;

    newYpos = ball.y_pos + DiagonalVelocity;

    if (newYpos + BallSize >= BALL_MAX_Y) {
      ball.y_pos = BALL_MAX_Y;
    } else {
      ball.y_pos = newYpos;
    }

    break;

  default:
    break;
  }
}

void draw_playing_field() {
  for (unsigned int i = 0; i < num_blocks; i++) {
    DrawBlock(block_map[i].x_pos, block_map[i].y_pos, BlockSize, BlockSize, block_map[i].color);
  }
}

void update_ball_direction(HitType hit_t) {
  switch (hit_t) {
  case TopHit:
    if (ball.direction == DiagonalUpRight) {
      ball.direction = DiagonalDownRight;
    } else if (ball.direction == DiagonalUpLeft) {
      ball.direction = DiagonalDownLeft;
    }

    ball.direction = DiagonalDownRight;
    break;

  case RightHit:
    if (ball.direction == DiagonalUpRight) {
      ball.direction = DiagonalUpLeft;
    } else if (ball.direction == HorizontalRight) {
      ball.direction = HorizontalLeft;
    } else if (ball.direction == DiagonalDownRight) {
      ball.direction = DiagonalDownLeft;
    }
    break;

  case LeftHit:
    if (ball.direction == DiagonalUpLeft) {
      ball.direction = DiagonalUpRight;
    } else if (ball.direction == DiagonalDownRight) {
      ball.direction = DiagonalDownLeft;
    }

  case BottomHit:
    if (ball.direction == DiagonalDownRight) {
      ball.direction = DiagonalUpRight;
    } else if (ball.direction == DiagonalDownLeft) {
      ball.direction = DiagonalUpLeft;
    }

    break;

  default:
    break;
  }
}

// mode == 0 -> check for a corner hit
// mode == 1 -> check for single or double block hit
char verify_hit(int x, int y, char mode) {
  char result = 0;

  switch (mode) {
  case 0:
    if (x && y) {
      result = 1; // Corner hit
    }
    break;
  case 1:
    if (x && y) {
      result = 1; // Single block hit
    } else if (x || y) {
      result = 2; // Double block hit
    }
    break;
  }

  return result;
}

char corner_hit(Block *block, int ball_x_pos, int ball_y_pos) {
  char x_hit = 0;
  char y_hit = 0;

  if (ball_x_pos >= block->x_pos && ball_x_pos <= block->x_pos + BlockSize) {
    x_hit = 1;
  }
  if (ball_y_pos <= block->y_pos + BlockSize && ball_y_pos >= block->y_pos) {
    y_hit = 1;
  }

  return verify_hit(x_hit, y_hit, 0);
}

char top_left_corner_hit(Block *block) {
  return corner_hit(block, ball.x_pos, ball.y_pos);
}

char top_right_corner_hit(Block *block) {
  return corner_hit(block, ball.x_pos + BallSize, ball.y_pos);
}

char bottom_right_corner_hit(Block *block) {
  return corner_hit(block, ball.x_pos + BallSize, ball.y_pos + BallSize);
}

char bottom_left_corner_hit(Block *block) {
  return corner_hit(block, ball.x_pos, ball.y_pos + BallSize);
}

char side_hit(Block *block, int ball_x_pos_1, int ball_y_pos_1, int ball_x_pos_2, int ball_y_pos_2) {
  char first_corner_hit = corner_hit(block, ball_x_pos_1, ball_y_pos_1);
  char second_corner_hit = corner_hit(block, ball_x_pos_2, ball_y_pos_2);

  return verify_hit(first_corner_hit, second_corner_hit, 1);
}

char top_hit(Block *block) {
  return side_hit(block, ball.x_pos, ball.y_pos, ball.x_pos + BallSize, ball.y_pos);
}

char right_hit(Block *block) {
  return side_hit(block, ball.x_pos + BallSize, ball.y_pos, ball.x_pos + BallSize, ball.y_pos + BallSize);
}

char bottom_hit(Block *block) {
  return side_hit(block, ball.x_pos, ball.y_pos + BallSize, ball.x_pos + BallSize, ball.y_pos + BallSize);
}

char left_hit(Block *block) {
  return side_hit(block, ball.x_pos, ball.y_pos, ball.x_pos, ball.y_pos + BallSize);
}

char checkBlockCollision(Block *block) {
  char hit_top = top_hit(block);
  char hit_right = right_hit(block);
  char hit_bottom = bottom_hit(block);
  char hit_left = left_hit(block);

  if (hit_right) {
    return RightHit;
  } else if (hit_top) {
    return TopHit;
  } else if (hit_left) {
    return LeftHit;
  }

  else if (hit_bottom) {
    return BottomHit;
  }

  return NoHit;
}

void check_block_hit() {
  for (unsigned int i = 0; i < num_blocks; i++) {
    if (block_map[i].destroyed == 1) {
      continue; // Skip destroyed blocks
    }

    HitType hit = checkBlockCollision(&block_map[i]);

    if (hit != 0) {
      block_map[i].destroyed = 1;
      block_map[i].color = white;

      update_ball_direction(hit);
    }
  }
}

void check_bar_hit() {
  for (int i = 0; i < BallSize; i++) {
    if (ball.y_pos + i < playerBar.y_pos + 15 && ball.y_pos + i >= playerBar.y_pos) {
      ball.direction = DiagonalUpRight;
      return;
    } else if (ball.y_pos + i < playerBar.y_pos + 30 && ball.y_pos + i >= playerBar.y_pos + 15) {
      ball.direction = HorizontalRight;
      return;
    } else if (ball.y_pos + i <= playerBar.y_pos + 45 && ball.y_pos + i >= playerBar.y_pos + 30) {
      ball.direction = DiagonalDownRight;
      return;
    }
  }
}

void update_game_state() {
  if (currentState != Running) {
    return;
  }

  if (ball.x_pos > 320) {
    currentState = Won;
    return;
  }

  if (ball.x_pos < 6) {
    currentState = Lost;
    return;
  }

  update_ball_position();

  if ((ball.y_pos == BALL_MIN_Y && ball.direction <= 180) || (ball.y_pos == BALL_MAX_Y && ball.direction >= 180)) {
    ball.direction += 45;
    return;
  } else if ((ball.y_pos == BALL_MIN_Y && ball.direction >= 180) || (ball.y_pos == BALL_MAX_Y && ball.direction <= 180)) {
    ball.direction -= 45;
    return;
  }

  if (ball.x_pos >= 163) { // x = 164 earliset possible hit for n_cols = 10
    check_block_hit();
  } else if (ball.x_pos <= 10) {
    check_bar_hit();
  }
}

void update_bar_state() {
  unsigned int word = ReadUart();
  unsigned char rdy = (word >> 8) & 0xff;

  if (rdy != 0x80) {
    return; // UART not ready, skip current iteration.
  }

  playerBar.y_pos_prev = playerBar.y_pos;

  unsigned char byte = word & 0xff;

  if (byte == VerticalUp) {
    if (playerBar.y_pos < BAR_STEP) {
      playerBar.y_pos = 0;
    } else {
      playerBar.y_pos -= BAR_STEP;
    }

  } else if (byte == VerticalDown) {
    unsigned int step_limit = height - BAR_HEIGHT - BAR_STEP;

    if (playerBar.y_pos > step_limit) {
      playerBar.y_pos = height - BAR_HEIGHT;
    } else {
      playerBar.y_pos += BAR_STEP;
    }
  }

  // We don't need to check remanining bytes since the update function
  // runs once every play() loop. This will clear the buffer.

  // Additionally, we only want to apply one step every cycle.
  // If not, the player can teleport the playerbar in one cycle rather
  // than stepping it.
}

void write(char *str) {
  char byte;

  while ((byte = *str) != 0x00) {
    WriteUart(byte);
    str++;
  }
}

void play() {
  ClearScreen();
  // HINT: This is the main game loop
  while (1) {
    update_game_state();
    update_bar_state();
    if (currentState != Running) {
      break;
    }
    draw_playing_field();
    draw_ball();

    DrawBar(playerBar.y_pos_prev, white); // Clear the previous position
    DrawBar(playerBar.y_pos, black);
  }

  if (currentState == Won) {
    write(won);
    write("\n");
  } else if (currentState == Lost) {
    write(lost);
    write("\n");
  } else if (currentState == Exit) {
    return;
  }
  currentState = Stopped;
}

void reset() {
  // Hint: This is draining the UART buffer
  int remaining = 0;
  do {
    unsigned long long out = ReadUart();
    if (!(out & 0x8000)) {
      // not valid - abort reading
      continue;
    }
    remaining = (out & 0xFF0000) >> 4;
  } while (remaining > 0);

  ClearScreen();

  // Reset ball state
  ball.x_pos = 8;
  ball.x_pos_prev = 8;
  ball.y_pos = 117;
  ball.y_pos_prev = 117;
  ball.direction = HorizontalRight; // Assuming HorizontalRight is part of your direction enum

  // Reset player bar state
  playerBar.x_pos = 0;
  playerBar.x_pos_prev = 0;
  playerBar.y_pos = 98;
  playerBar.y_pos_prev = 98;
}

void wait_for_start() {
  // TODO: Implement waiting behaviour until the user presses either w/s

  unsigned int word;
  unsigned char rdy;
  unsigned char byte;

  while (1) {
    word = ReadUart();
    rdy = (word >> 8) & 0xff;

    if (rdy != 0x80) {
      continue; // UART not ready, skip current iteration.
    }

    byte = word & 0xff;

    if (byte == VerticalUp || byte == VerticalDown) {
      currentState = Running;
      return;
    }
  }
}

void init_block_map() {
  num_blocks = n_cols * (height / BlockSize);
  block_map = (Block *)malloc(num_blocks * sizeof(Block));

  unsigned int x_pos = width - n_cols * BlockSize; // Horizontal starting position of the playing field
  unsigned int y_pos = 0;
  unsigned int block_index = 0;

  for (unsigned int y = y_pos; y < height; y += BlockSize) {
    for (unsigned int x = x_pos; x < width; x += BlockSize) {
      if (block_index >= num_blocks)
        break;

      block_map[block_index].x_pos = x;
      block_map[block_index].y_pos = y;

      // Alternate color in a chequered pattern.
      switch ((x / BlockSize + y / BlockSize) % 3) {
      case 0:
        block_map[block_index].color = red;
        break;
      case 1:
        block_map[block_index].color = green;
        break;
      case 2:
        block_map[block_index].color = blue;
        break;
      }

      block_map[block_index].deleted = 0;
      block_map[block_index].destroyed = 0;
      block_index++;
    }
  }
}

int main(int argc, char *argv[]) {
  ClearScreen();

  while (1) {
    init_block_map();
    draw_playing_field();
    draw_ball();
    DrawBar(playerBar.y_pos, black);

    wait_for_start();
    play();
    reset();
    if (currentState == Exit) {
      break;
    }
  }
  return 0;
}

// THIS IS FOR THE OPTIONAL TASKS ONLY

// HINT: How to access the correct bitmask
// sample: to get character a's bitmask, use
// font8x8['a']
char font8x8[128][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0000 (nul)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0001
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0002
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0003
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0004
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0005
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0006
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0007
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0008
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0009
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000A
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000B
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000C
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000D
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000E
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000F
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0010
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0011
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0012
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0013
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0014
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0015
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0016
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0017
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0018
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0019
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001A
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001B
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001C
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001D
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001E
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001F
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0020 (space)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // U+0021 (!)
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0022 (")
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // U+0023 (#)
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // U+0024 ($)
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // U+0025 (%)
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // U+0026 (&)
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0027 (')
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // U+0028 (()
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // U+0029 ())
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // U+002A (*)
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // U+002B (+)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // U+002C (,)
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // U+002D (-)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // U+002E (.)
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // U+002F (/)
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // U+0030 (0)
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // U+0031 (1)
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // U+0032 (2)
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // U+0033 (3)
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // U+0034 (4)
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // U+0035 (5)
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // U+0036 (6)
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // U+0037 (7)
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // U+0038 (8)
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // U+0039 (9)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // U+003A (:)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // U+003B (;)
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // U+003C (<)
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // U+003D (=)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // U+003E (>)
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // U+003F (?)
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // U+0040 (@)
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // U+0041 (A)
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // U+0042 (B)
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // U+0043 (C)
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // U+0044 (D)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // U+0045 (E)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // U+0046 (F)
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // U+0047 (G)
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // U+0048 (H)
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+0049 (I)
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // U+004A (J)
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // U+004B (K)
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // U+004C (L)
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // U+004D (M)
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // U+004E (N)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // U+004F (O)
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // U+0050 (P)
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // U+0051 (Q)
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // U+0052 (R)
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // U+0053 (S)
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+0054 (T)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // U+0055 (U)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // U+0056 (V)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // U+0057 (W)
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // U+0058 (X)
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // U+0059 (Y)
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // U+005A (Z)
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, // U+005B ([)
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, // U+005C (\)
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, // U+005D (])
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // U+005E (^)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // U+005F (_)
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0060 (`)
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, // U+0061 (a)
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, // U+0062 (b)
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, // U+0063 (c)
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00}, // U+0064 (d)
    {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00}, // U+0065 (e)
    {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00}, // U+0066 (f)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // U+0067 (g)
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, // U+0068 (h)
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+0069 (i)
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, // U+006A (j)
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, // U+006B (k)
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+006C (l)
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // U+006D (m)
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, // U+006E (n)
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, // U+006F (o)
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, // U+0070 (p)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, // U+0071 (q)
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, // U+0072 (r)
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, // U+0073 (s)
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, // U+0074 (t)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, // U+0075 (u)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // U+0076 (v)
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, // U+0077 (w)
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, // U+0078 (x)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // U+0079 (y)
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, // U+007A (z)
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, // U+007B ({)
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // U+007C (|)
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, // U+007D (})
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+007E (~)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // U+007F
};