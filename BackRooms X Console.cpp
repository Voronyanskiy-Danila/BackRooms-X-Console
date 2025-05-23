#include <iostream>
#include <conio.h>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <vector>
#include <windows.h> // Добавлено для работы с цветом

using namespace std;
using namespace chrono;

// Константы и глобальные переменные
const int WIDTH = 40;
const int HEIGHT = 20;
const char WALL = '#';
const char EMPTY = ' ';
const char PLAYER = 'P';
const char BOTTLE = 'B';

// Коды цветов
const int PLAYER_COLOR = 10;    // Зелёный
const int BOTTLE_COLOR = 14;    // Жёлтый
const int WALL_COLOR = 7;       // Стандартный
const int TEXT_COLOR = 7;       // Стандартный

vector<vector<bool>> walls(HEIGHT, vector<bool>(WIDTH, false));
int playerX, playerY;
int bottleX, bottleY;
int timeBonus = 0;
int timer = 60;
bool gameOver;
steady_clock::time_point startTime;
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); // Дескриптор консоли

// Прототипы функций
void GenerateWalls();
void Setup();
void Draw();
void Input();
void Logic();

// Функция для установки цвета текста
void SetColor(int color) {
    SetConsoleTextAttribute(hConsole, color);
}

void GenerateWalls() {
    // Внешние стены
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1) {
                walls[y][x] = true;
            }
        }
    }

    // Внутренние стены
    vector<string> mazePattern = {
        "########################################",
        "#          #                #          #",
        "# ## ## # ### ### ###################  #",
        "# #   # #        # #                #  #",
        "# #   # #  ## ## # ############# #     #",
        "# #   #        # #            # # #    #",
        "# #  ##   # ## # #### ######## # #     #",
        "# #          # #            # # # #    #",
        "# ## ## ### ## ######## #### # # #     #",
        "# #   #     #              # # # #     #",
        "# #   ##### ###### ####### # # # #     #",
        "# #                     #        #     #",
        "# # ######## ########## #        #     #",
        "# #                              #     #",
        "##################### ############     #",
        "#                                      #",
        "###########  #####################     #",
        "#                               #      #",
        "#                                      #",
        "########################################"
    };

    // Генерация стен из шаблона
    for (int y = 0; y < HEIGHT; y++) {
        string line = mazePattern[y];
        while (line.size() < WIDTH) line += ' ';
        for (int x = 0; x < WIDTH; x++) {
            walls[y][x] = (line[x] == '#');
        }
    }
}

void Setup() {
    gameOver = false;
    startTime = steady_clock::now();
    GenerateWalls();

    // Генерация позиции игрока
    do {
        playerX = rand() % WIDTH;
        playerY = rand() % HEIGHT;
    } while (walls[playerY][playerX]);

    // Генерация бутылки
    do {
        bottleX = rand() % WIDTH;
        bottleY = rand() % HEIGHT;
    } while (walls[bottleY][bottleX] || (bottleX == playerX && bottleY == playerY));
}

void Draw() {
    system("cls");

    // Отрисовка поля
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (y == playerY && x == playerX) {
                SetColor(PLAYER_COLOR);
                cout << PLAYER;
            }
            else if (y == bottleY && x == bottleX) {
                SetColor(BOTTLE_COLOR);
                cout << BOTTLE;
            }
            else {
                SetColor(WALL_COLOR);
                cout << (walls[y][x] ? WALL : EMPTY);
            }
        }
        cout << endl;
    }

    // Таймер
    SetColor(TEXT_COLOR);
    auto currentTime = steady_clock::now();
    int elapsed = duration_cast<seconds>(currentTime - startTime).count();
    timer = 60 + timeBonus - elapsed;
    cout << "Time left: " << (timer > 0 ? timer : 0) << " seconds\n";
}


void Input() {
    if (_kbhit()) {
        int newX = playerX;
        int newY = playerY;

        switch (tolower(_getch())) {
        case 'a': newX--; break;
        case 'd': newX++; break;
        case 'w': newY--; break;
        case 's': newY++; break;
        case 'x': gameOver = true; break;
        }

        // Проверка коллизий
        if (newX >= 0 && newX < WIDTH &&
            newY >= 0 && newY < HEIGHT &&
            !walls[newY][newX]) {
            playerX = newX;
            playerY = newY;
        }
    }
}

void Logic() {
    // Сбор бутылок
    if (playerX == bottleX && playerY == bottleY) {
        timeBonus += 5;
        do {
            bottleX = rand() % WIDTH;
            bottleY = rand() % HEIGHT;
        } while (walls[bottleY][bottleX] || (bottleX == playerX && bottleY == playerY));
    }

    // Проверка времени
    if (timer <= 0) gameOver = true;
}

int main() {
    // Настройка кодировки консоли
    SetConsoleOutputCP(CP_UTF8);
    srand(time(0));
    Setup();

    while (!gameOver) {
        Draw();
        Input();
        Logic();
        this_thread::sleep_for(50ms);
    }

    SetColor(TEXT_COLOR);
    cout << "\nGAME OVER! Time's up!" << endl;
    return 0;
}