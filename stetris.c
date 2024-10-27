#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// The game state can be used to detect what happens on the playfield
#define GAMEOVER 0
#define ACTIVE (1 << 0)
#define ROW_CLEAR (1 << 1)
#define TILE_ADDED (1 << 2)

#define NUM_PIXELS 64

#define FRAME_BUFFER_PATH "/dev/fb0"
#define FRAME_BUFFER_SIZE 128 // 64 pixels * 2 bytes

#define JOYSTICK_INPUT_PATH "/dev/input/event0"

#define COLOR_BLACK \
  (pixelColor) { .red = 0, .green = 0, .blue = 0 }
#define COLOR_PURPLE \
  (pixelColor) { .red = 31, .green = 0, .blue = 31 }

// If you extend this structure, either avoid pointers or adjust
// the game logic allocate/deallocate and reset the memory
typedef struct
{
  bool occupied;
} tile;

typedef struct
{
  unsigned int x;
  unsigned int y;
} coord;

typedef struct _rgb565 {
  uint16_t blue : 5;
  uint16_t green : 6;
  uint16_t red : 5;
} pixelColor;

typedef struct
{
  coord const grid;                     // playfield bounds
  unsigned long const uSecTickTime;     // tick rate
  unsigned long const rowsPerLevel;     // speed up after clearing rows
  unsigned long const initNextGameTick; // initial value of nextGameTick

  unsigned int tiles; // number of tiles played
  unsigned int rows;  // number of rows cleared
  unsigned int score; // game score
  unsigned int level; // game level

  tile *rawPlayfield; // pointer to raw memory of the playfield
  tile **playfield;   // This is the play field array
  unsigned int state;
  coord activeTile; // current tile

  unsigned long tick;         // incremeted at tickrate, wraps at nextGameTick
                              // when reached 0, next game state calculated
  unsigned long nextGameTick; // sets when tick is wrapping back to zero
                              // lowers with increasing level, never reaches 0
} gameConfig;

gameConfig game = {
    .grid = {8, 8},
    .uSecTickTime = 10000,
    .rowsPerLevel = 2,
    .initNextGameTick = 50,
};

pixelColor *pixelBuffer; // Color information for all 64 pixels

bool checkFramebufferId(int fd) {
  struct fb_fix_screeninfo fb_info;
  if (ioctl(fd, FBIOGET_FSCREENINFO, &fb_info) != 0) {
    perror("Failed to read fixed screen-information from framebuffer");
    return false;
  }

  if (strcmp(fb_info.id, "RPi-Sense FB") != 0) {
    fprintf(stderr, "EXCEPTION: Framebuffer ID mismatch. Found: %s\n", fb_info.id);
    return false;
  }

  return true;
}

void clearPixelGrid() {
  for (uint8_t i = 0; i < FRAME_BUFFER_SIZE; i++) {
    pixelBuffer[i] = COLOR_BLACK;
  }
}

bool checkInputDeviceName(int joy_fd) {
  char device_name[256] = "Unknown";
  if (ioctl(joy_fd, EVIOCGNAME(sizeof(device_name)), device_name) < 0) {
    perror("Failed to get device name");
    return false;
  }

  if (strcmp(device_name, "Raspberry Pi Sense HAT Joystick") != 0) {
    fprintf(stderr, "EXCEPTION: Input device ID mismatch. Found: %s\n", device_name);
    return false;
  }

  return true;
}

bool initializeSenseHat() {
  // Initialize framebuffer
  int file_descriptor = open(FRAME_BUFFER_PATH, O_RDWR);
  if (file_descriptor == -1 || !checkFramebufferId(file_descriptor)) {
    perror("Framebuffer initialization failed");
    return false;
  }

  // Memory map the framebuffer
  pixelBuffer = (pixelColor *)mmap(NULL, sizeof(pixelBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
  if (pixelBuffer == MAP_FAILED) {
    perror("Failed to memory-map framebuffer");
    close(file_descriptor);
    return false;
  }

  close(file_descriptor);

  clearPixelGrid();

  // Verify joystick ID
  int joy_fd = open(JOYSTICK_INPUT_PATH, O_RDONLY);
  if (joy_fd == -1 || !checkInputDeviceName(joy_fd)) {
    perror("Joystick initialization failed");
    return false;
  }

  close(joy_fd);

  return true;
}

// This function is called when the application exits
// Here you can free up everything that you might have opened/allocated
void freeSenseHat() {
  munmap(pixelBuffer, sizeof(pixelBuffer));
}

// This function should return the key that corresponds to the joystick press
// KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, with the respective direction
// and KEY_ENTER, when the the joystick is pressed
// !!! when nothing was pressed you MUST return 0 !!!
int readSenseHatJoystick(int joystick_fd) {
  struct pollfd poll_descriptor = {
      .fd = joystick_fd,
      .events = EV_KEY,
  };

  int poll_res = poll(&poll_descriptor, 1, 0);
  if (poll_res <= 0) {
    // printf("TIMEOUT: %i\n", poll_res);
    return 0;
  }

  struct input_event joystick_events[32];
  int num_read_bytes = read(joystick_fd, joystick_events, sizeof(struct input_event) * 32);

  if (num_read_bytes < (int)sizeof(struct input_event)) {
    // We've not read a full event. Return.
    return 0;
  }

  // Loop through events
  int key_stroke = 0;

  for (int i = 0; i < num_read_bytes / sizeof(struct input_event); i++) {
    struct input_event joystick_ev = joystick_events[i];

    if (joystick_ev.type == EV_KEY) {
      key_stroke = joystick_ev.code;
      break;
    }
  }

  if (key_stroke == KEY_UP) {
    return KEY_UP;
  }
  if (key_stroke == KEY_DOWN) {
    return KEY_DOWN;
  }
  if (key_stroke == KEY_LEFT) {
    return KEY_LEFT;
  }
  if (key_stroke == KEY_RIGHT) {
    return KEY_RIGHT;
  }
  if (key_stroke == KEY_ENTER) {
    return KEY_ENTER;
  }

  return 0;
}

// This function should render the gamefield on the LED matrix. It is called
// every game tick. The parameter playfieldChanged signals whether the game logic
// has changed the playfield
void renderSenseHatMatrix(bool const playfieldChanged) {
  if (!playfieldChanged)
    return;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      int index = row * 8 + col;

      if (game.playfield[col][row].occupied == false) {
        pixelBuffer[index] = COLOR_BLACK;
      } else {
        pixelBuffer[index] = COLOR_PURPLE;
      }
    }
  }
}

bool enableJoyStickEvents(int *joystick_fd) {
  *joystick_fd = open(JOYSTICK_INPUT_PATH, O_RDONLY);
  if (*joystick_fd == -1) {
    return false; // failed
  }

  return true; // success
}

// The game logic uses only the following functions to interact with the playfield.
// if you choose to change the playfield or the tile structure, you might need to
// adjust this game logic <> playfield interface

static inline void newTile(coord const target) {
  game.playfield[target.y][target.x].occupied = true;
}

static inline void copyTile(coord const to, coord const from) {
  memcpy((void *)&game.playfield[to.y][to.x], (void *)&game.playfield[from.y][from.x], sizeof(tile));
}

static inline void copyRow(unsigned int const to, unsigned int const from) {
  memcpy((void *)&game.playfield[to][0], (void *)&game.playfield[from][0], sizeof(tile) * game.grid.x);
}

static inline void resetTile(coord const target) {
  memset((void *)&game.playfield[target.y][target.x], 0, sizeof(tile));
}

static inline void resetRow(unsigned int const target) {
  memset((void *)&game.playfield[target][0], 0, sizeof(tile) * game.grid.x);
}

static inline bool tileOccupied(coord const target) {
  return game.playfield[target.y][target.x].occupied;
}

static inline bool rowOccupied(unsigned int const target) {
  for (unsigned int x = 0; x < game.grid.x; x++) {
    coord const checkTile = {x, target};
    if (!tileOccupied(checkTile)) {
      return false;
    }
  }
  return true;
}

static inline void resetPlayfield() {
  for (unsigned int y = 0; y < game.grid.y; y++) {
    resetRow(y);
  }
}

// Below here comes the game logic. Keep in mind: You are not allowed to change how the game works!
// that means no changes are necessary below this line! And if you choose to change something
// keep it compatible with what was provided to you!

bool addNewTile() {
  game.activeTile.y = 0;
  game.activeTile.x = (game.grid.x - 1) / 2;
  if (tileOccupied(game.activeTile))
    return false;
  newTile(game.activeTile);
  return true;
}

bool moveRight() {
  coord const newTile = {game.activeTile.x + 1, game.activeTile.y};
  if (game.activeTile.x < (game.grid.x - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}

bool moveLeft() {
  coord const newTile = {game.activeTile.x - 1, game.activeTile.y};
  if (game.activeTile.x > 0 && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}

bool moveDown() {
  coord const newTile = {game.activeTile.x, game.activeTile.y + 1};
  if (game.activeTile.y < (game.grid.y - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}

bool clearRow() {
  if (rowOccupied(game.grid.y - 1)) {
    for (unsigned int y = game.grid.y - 1; y > 0; y--) {
      copyRow(y, y - 1);
    }
    resetRow(0);
    return true;
  }
  return false;
}

void advanceLevel() {
  game.level++;
  switch (game.nextGameTick) {
  case 1:
    break;
  case 2 ... 10:
    game.nextGameTick--;
    break;
  case 11 ... 20:
    game.nextGameTick -= 2;
    break;
  default:
    game.nextGameTick -= 10;
  }
}

void newGame() {
  game.state = ACTIVE;
  game.tiles = 0;
  game.rows = 0;
  game.score = 0;
  game.tick = 0;
  game.level = 0;
  resetPlayfield();
}

void gameOver() {
  game.state = GAMEOVER;
  game.nextGameTick = game.initNextGameTick;
}

bool sTetris(int const key) {
  bool playfieldChanged = false;

  if (game.state & ACTIVE) {
    // Move the current tile
    if (key) {
      playfieldChanged = true;
      switch (key) {
      case KEY_LEFT:
        moveLeft();
        break;
      case KEY_RIGHT:
        moveRight();
        break;
      case KEY_DOWN:
        while (moveDown()) {
        };
        game.tick = 0;
        break;
      default:
        playfieldChanged = false;
      }
    }

    // If we have reached a tick to update the game
    if (game.tick == 0) {
      // We communicate the row clear and tile add over the game state
      // clear these bits if they were set before
      game.state &= ~(ROW_CLEAR | TILE_ADDED);

      playfieldChanged = true;
      // Clear row if possible
      if (clearRow()) {
        game.state |= ROW_CLEAR;
        game.rows++;
        game.score += game.level + 1;
        if ((game.rows % game.rowsPerLevel) == 0) {
          advanceLevel();
        }
      }

      // if there is no current tile or we cannot move it down,
      // add a new one. If not possible, game over.
      if (!tileOccupied(game.activeTile) || !moveDown()) {
        if (addNewTile()) {
          game.state |= TILE_ADDED;
          game.tiles++;
        } else {
          gameOver();
        }
      }
    }
  }

  // Press any key to start a new game
  if ((game.state == GAMEOVER) && key) {
    playfieldChanged = true;
    newGame();
    addNewTile();
    game.state |= TILE_ADDED;
    game.tiles++;
  }

  return playfieldChanged;
}

int readKeyboard() {
  struct pollfd pollStdin = {
      .fd = STDIN_FILENO,
      .events = POLLIN};
  int lkey = 0;

  if (poll(&pollStdin, 1, 0)) {
    lkey = fgetc(stdin);
    if (lkey != 27)
      goto exit;
    lkey = fgetc(stdin);
    if (lkey != 91)
      goto exit;
    lkey = fgetc(stdin);
  }
exit:
  switch (lkey) {
  case 10:
    return KEY_ENTER;
  case 65:
    return KEY_UP;
  case 66:
    return KEY_DOWN;
  case 67:
    return KEY_RIGHT;
  case 68:
    return KEY_LEFT;
  }
  return 0;
}

void renderConsole(bool const playfieldChanged) {
  if (!playfieldChanged)
    return;

  // Goto beginning of console
  fprintf(stdout, "\033[%d;%dH", 0, 0);
  for (unsigned int x = 0; x < game.grid.x + 2; x++) {
    fprintf(stdout, "-");
  }
  fprintf(stdout, "\n");
  for (unsigned int y = 0; y < game.grid.y; y++) {
    fprintf(stdout, "|");
    for (unsigned int x = 0; x < game.grid.x; x++) {
      coord const checkTile = {x, y};
      fprintf(stdout, "%c", (tileOccupied(checkTile)) ? '#' : ' ');
    }
    switch (y) {
    case 0:
      fprintf(stdout, "| Tiles: %10u\n", game.tiles);
      break;
    case 1:
      fprintf(stdout, "| Rows:  %10u\n", game.rows);
      break;
    case 2:
      fprintf(stdout, "| Score: %10u\n", game.score);
      break;
    case 4:
      fprintf(stdout, "| Level: %10u\n", game.level);
      break;
    case 7:
      fprintf(stdout, "| %17s\n", (game.state == GAMEOVER) ? "Game Over" : "");
      break;
    default:
      fprintf(stdout, "|\n");
    }
  }
  for (unsigned int x = 0; x < game.grid.x + 2; x++) {
    fprintf(stdout, "-");
  }
  fflush(stdout);
}

inline unsigned long uSecFromTimespec(struct timespec const ts) {
  return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  // This sets the stdin in a special state where each
  // keyboard press is directly flushed to the stdin and additionally
  // not outputted to the stdout
  // {
  //   struct termios ttystate;
  //   tcgetattr(STDIN_FILENO, &ttystate);
  //   ttystate.c_lflag &= ~(ICANON | ECHO);
  //   ttystate.c_cc[VMIN] = 1;
  //   tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
  // }

  // Allocate the playing field structure
  game.rawPlayfield = (tile *)malloc(game.grid.x * game.grid.y * sizeof(tile));
  game.playfield = (tile **)malloc(game.grid.y * sizeof(tile *));
  if (!game.playfield || !game.rawPlayfield) {
    fprintf(stderr, "ERROR: could not allocate playfield\n");
    return 1;
  }
  for (unsigned int y = 0; y < game.grid.y; y++) {
    game.playfield[y] = &(game.rawPlayfield[y * game.grid.x]);
  }

  // Reset playfield to make it empty
  resetPlayfield();
  // Start with gameOver
  gameOver();

  if (!initializeSenseHat()) {
    fprintf(stderr, "ERROR: could not initilize sense hat\n");
    return 1;
  };

  // Clear console, render first time
  // fprintf(stdout, "\033[H\033[J");
  // renderConsole(true);
  renderSenseHatMatrix(true);

  int joystick_fd;
  bool success = enableJoyStickEvents(&joystick_fd);
  if (!success) {
    fprintf(stderr, "ERROR: Failed to open joystick file descriptor\n");
    return 1;
  }

  while (true) {
    struct timeval sTv, eTv;
    gettimeofday(&sTv, NULL);

    int key = readSenseHatJoystick(joystick_fd);
    if (!key) {
      // NOTE: Uncomment the next line if you want to test your implementation with
      // reading the inputs from stdin. However, we expect you to read the inputs directly
      // from the input device and not from stdin (you should implement the readSenseHatJoystick
      // method).
      // key = readKeyboard();
    }
    if (key == KEY_ENTER)
      break;

    bool playfieldChanged = sTetris(key);
    renderConsole(playfieldChanged);
    renderSenseHatMatrix(playfieldChanged);

    // Wait for next tick
    gettimeofday(&eTv, NULL);
    unsigned long const uSecProcessTime = ((eTv.tv_sec * 1000000) + eTv.tv_usec) - ((sTv.tv_sec * 1000000 + sTv.tv_usec));
    if (uSecProcessTime < game.uSecTickTime) {
      usleep(game.uSecTickTime - uSecProcessTime);
    }
    game.tick = (game.tick + 1) % game.nextGameTick;
  }

  close(joystick_fd);

  freeSenseHat();
  free(game.playfield);
  free(game.rawPlayfield);

  return 0;
}
