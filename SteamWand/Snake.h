#pragma once
#include "Dcs.h"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <windows.h>
#include <conio.h>

const int COLS = 20;
const int ROWS = 20;

void draw(const char board[ROWS][COLS], int score, int length, bool gameOver);

struct Segment {
    int x, y;
    int lifetime;
};

struct Apple {
    int x, y;
};

class SnakeGame {
public:
    SnakeGame() : world(1024) {}

    void run() {
        CONSOLE_CURSOR_INFO cursor = { 1, FALSE };
        SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor);
        srand((unsigned)time(nullptr));

        while (true) {
            setup();
            play();

            while (true) {
                if (_kbhit()) {
                    char key = _getch();
                    if (key == 'r' || key == 'R') {
                        break;
                    }
                    if (key == 'q' || key == 'Q') {
                        return;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

private:
    World world;
    int dirX, dirY, nextX, nextY;
    int score, length, headX, headY;
    Atom appleAtom;

    void setup() {
        world = World(1024);
        dirX = 1;
        dirY = 0;
        nextX = 1;
        nextY = 0;
        score = 0;
        length = 3;
        headX = COLS / 2;
        headY = ROWS / 2;
        appleAtom = Atom::invalid();

        for (int i = 0; i < length; i++) {
            world.add<Segment>({ headX - i, headY, length - i });
        }
        spawnApple();
    }

    void spawnApple() {
        while (true) {
            int posX = rand() % COLS;
            int posY = rand() % ROWS;
            bool isOccupied = false;

            for (auto& segment : world.iter<Segment>()) {
                if (segment.x == posX && segment.y == posY) {
                    isOccupied = true;
                    break;
                }
            }

            if (!isOccupied) {
                Apple* appleData = world.get<Apple>(appleAtom);
                if (appleData) {
                    appleData->x = posX;
                    appleData->y = posY;
                }
                else {
                    appleAtom = world.add<Apple>({ posX, posY });
                }
                return;
            }
        }
    }

    void play() {
        while (true) {
            while (_kbhit()) {
                char key = _getch();

                if (key == 'w' && dirY == 0) {
                    nextX = 0;
                    nextY = -1;
                }
                else if (key == 's' && dirY == 0) {
                    nextX = 0;
                    nextY = 1;
                }
                else if (key == 'a' && dirX == 0) {
                    nextX = -1;
                    nextY = 0;
                }
                else if (key == 'd' && dirX == 0) {
                    nextX = 1;
                    nextY = 0;
                }
                else if (key == 'q') {
                    return;
                }
            }

            dirX = nextX;
            dirY = nextY;

            headX = (headX + dirX + COLS) % COLS;
            headY = (headY + dirY + ROWS) % ROWS;

            bool selfHit = false;
            for (auto& segment : world.iter<Segment>()) {
                if (segment.lifetime > 0 && segment.x == headX && segment.y == headY) {
                    selfHit = true;
                    break;
                }
            }

            if (selfHit) {
                buildAndDraw(true);
                return;
            }

            // Check if snake found apple
            Apple* appleData = world.get<Apple>(appleAtom);
            if (appleData && appleData->x == headX && appleData->y == headY) {
                score += 10;
                length++;
                spawnApple();
            }

            // Decrement lifetime for all tail segments
            for (auto& segment : world.iter<Segment>()) {
                segment.lifetime--;
            }

            // Remove tail segments that have expired
            auto& slab = world.get_slab<Segment>();
            for (uint32_t i = 0; i < slab.next_idx; ++i) {
                Atom handle = { i };
                if (slab.data[i].lifetime <= 0) {
                    world.queue_free<Segment>(handle);
                }
            }

            world.add<Segment>({ headX, headY, length });

            buildAndDraw(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(130));

            world.cleanup();
        }
    }

    void buildAndDraw(bool gameOver) {
        char board[ROWS][COLS];
        memset(board, '.', sizeof(board));

        for (auto& segment : world.iter<Segment>()) {
            board[segment.y][segment.x] = '#';
        }

        board[headY][headX] = '@';

        Apple* appleData = world.get<Apple>(appleAtom);
        if (appleData) {
            board[appleData->y][appleData->x] = 'O';
        }

        draw(board, score, length, gameOver);
    }
};

// Unrelated function for drawing in terminal
void draw(const char board[ROWS][COLS], int score, int length, bool gameOver) {
    const int PRINT_W = COLS + 2;
    const int PRINT_H = ROWS + 3;
    CHAR_INFO buffer[PRINT_H][PRINT_W];

    const WORD GREEN = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    const WORD CYAN = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    const WORD RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
    const WORD WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    for (int x = 0; x < PRINT_W; x++) {
        buffer[0][x].Char.AsciiChar = '-';
        buffer[0][x].Attributes = GREEN;
    }

    for (int y = 0; y < ROWS; y++) {
        int bufferY = y + 1;

        buffer[bufferY][0].Char.AsciiChar = '|';
        buffer[bufferY][0].Attributes = GREEN;

        for (int x = 0; x < COLS; x++) {
            int bufferX = x + 1;
            char cellChar = board[y][x];
            WORD cellColor = GREEN;

            if (cellChar == '@') {
                cellColor = CYAN;
            }
            else if (cellChar == 'O') {
                cellColor = RED;
            }

            buffer[bufferY][bufferX].Char.AsciiChar = cellChar;
            buffer[bufferY][bufferX].Attributes = cellColor;
        }

        buffer[bufferY][COLS + 1].Char.AsciiChar = '|';
        buffer[bufferY][COLS + 1].Attributes = GREEN;
    }

    for (int x = 0; x < PRINT_W; x++) {
        buffer[ROWS + 1][x].Char.AsciiChar = '-';
        buffer[ROWS + 1][x].Attributes = GREEN;
    }

    char status[128];
    if (gameOver) {
        snprintf(status, sizeof(status), " GAME OVER! Score: %d  R: Restart Q: Quit", score);
    }
    else {
        snprintf(status, sizeof(status), " Score: %-4d Length: %-3d  WASD: Move Q: Quit", score, length);
    }

    int statusLen = (int)strlen(status);
    for (int x = 0; x < PRINT_W; x++) {
        char c = (x < statusLen) ? status[x] : ' ';
        buffer[ROWS + 2][x].Char.AsciiChar = c;
        buffer[ROWS + 2][x].Attributes = WHITE;
    }

    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD bufSize = { (SHORT)PRINT_W, (SHORT)PRINT_H };
    COORD bufOrigin = { 0, 0 };
    SMALL_RECT region = { 0, 0, (SHORT)(PRINT_W - 1), (SHORT)(PRINT_H - 1) };

    WriteConsoleOutputA(console, &buffer[0][0], bufSize, bufOrigin, &region);
}