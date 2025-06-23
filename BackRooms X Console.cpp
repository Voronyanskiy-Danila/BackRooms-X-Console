#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cmath>
#include <queue>
#include <algorithm>
#include <random>

#include <conio.h>
#include <windows.h>
#include "json.hpp"

using json = nlohmann::json;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
using std::cout;
using std::endl;
using std::vector;
using std::string;

int gWidth = 40;
int gHeight = 20;
const char kWall = '#';
const char kEmpty = ' ';
const char kPlayer = 'P';

const int WINNING_BOTTLES_COUNT = 10;
bool game_won = false;

const int kPlayerColor = 10;
const int kWallColor = 7;
const int kTextColor = 7;

vector<vector<bool>> walls;
int player_x, player_y;
int time_bonus = 0;
int timer = 60;
bool game_over;
int bottles_collected = 0;
steady_clock::time_point start_time;
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

steady_clock::time_point monster_freeze_until;
steady_clock::time_point player_invisible_until;
bool monster_frozen = false;
bool player_invisible = false;

vector<string> screen_buffer;
vector<WORD> color_buffer;

class Item {
public:
    string type;
    char character;
    int color;
    int x, y;
    int effect;
    int duration;
    bool consumable;
    bool auto_use;

    Item() : type(""), character('?'), color(7), x(0), y(0), effect(0),
        duration(0), consumable(true), auto_use(false) {
    }

    void fromJson(const json& j) {
        type = j.value("type", "unknown");
        if (j.contains("character") && j["character"].is_string()) {
            string char_str = j["character"].get<string>();
            if (!char_str.empty()) {
                character = char_str[0];
            }
        }
        color = j.value("color", 7);
        effect = j.value("effect", 0);
        duration = j.value("duration", 0);
        consumable = j.value("consumable", true);
        auto_use = j.value("auto_use", false);
    }
};

vector<Item> item_templates;
vector<Item> items;
vector<Item> inventory;

class Enemy {
public:
    string name;
    char character;
    int color;
    int x, y;
    int move_counter;
    int speed;
    bool frozen;

    Enemy() : move_counter(0), speed(3), character('M'), color(12), frozen(false) {}

    void loadFromFile(const string& filename) {
        try {
            std::ifstream f(filename);
            if (!f.is_open()) {
                throw std::runtime_error("Could not open enemy file");
            }

            json data = json::parse(f);

            name = data.value("name", "Monster");
            speed = data.value("speed", 3);

            if (data.contains("character") && data["character"].is_string()) {
                string char_str = data["character"].get<string>();
                if (!char_str.empty()) {
                    character = char_str[0];
                }
            }

            color = data.value("color", 12);
        }
        catch (const std::exception& e) {
            name = "Default Monster";
            character = 'M';
            color = 12;
            speed = 3;
        }
    }

    void move() {
        if (frozen && steady_clock::now() < monster_freeze_until) {
            return;
        }
        else if (frozen) {
            frozen = false;
            color = 12;
        }

        move_counter++;
        if (move_counter < speed) {
            return;
        }
        move_counter = 0;

        if (player_invisible && steady_clock::now() < player_invisible_until) {
            moveRandomly();
            return;
        }

        int new_x = x, new_y = y;
        if (findPathToPlayer(x, y, new_x, new_y)) {
            x = new_x;
            y = new_y;
            return;
        }

        moveRandomly();
    }

private:
    void moveRandomly() {
        const int dx[] = { 0, 1, 0, -1 };
        const int dy[] = { -1, 0, 1, 0 };

        vector<int> directions = { 0, 1, 2, 3 };
        static std::random_device rd;
        static std::mt19937 g(rd());
        std::shuffle(directions.begin(), directions.end(), g);

        for (int dir : directions) {
            int nx = x + dx[dir];
            int ny = y + dy[dir];

            if (nx >= 0 && nx < gWidth && ny >= 0 && ny < gHeight && !walls[ny][nx]) {
                x = nx;
                y = ny;
                return;
            }
        }
    }

    bool findPathToPlayer(int start_x, int start_y, int& out_x, int& out_y) {
        if (player_invisible && steady_clock::now() < player_invisible_until) {
            return false;
        }

        const int dx[] = { 0, 1, 0, -1 };
        const int dy[] = { -1, 0, 1, 0 };

        vector<vector<bool>> visited(gHeight, vector<bool>(gWidth, false));
        vector<vector<std::pair<int, int>>> prev(gHeight, vector<std::pair<int, int>>(gWidth, { -1, -1 }));

        std::queue<std::pair<int, int>> q;
        q.push({ start_x, start_y });
        visited[start_y][start_x] = true;

        while (!q.empty()) {
            auto current = q.front();
            q.pop();

            if (current.first == player_x && current.second == player_y) {
                std::pair<int, int> step = current;
                while (prev[step.second][step.first] != std::pair<int, int>(start_x, start_y) &&
                    prev[step.second][step.first] != std::pair<int, int>(-1, -1)) {
                    step = prev[step.second][step.first];
                }
                out_x = step.first;
                out_y = step.second;
                return true;
            }

            for (int i = 0; i < 4; i++) {
                int nx = current.first + dx[i];
                int ny = current.second + dy[i];

                if (nx >= 0 && nx < gWidth && ny >= 0 && ny < gHeight &&
                    !walls[ny][nx] && !visited[ny][nx]) {
                    visited[ny][nx] = true;
                    prev[ny][nx] = current;
                    q.push({ nx, ny });
                }
            }
        }

        return false;
    }
};

Enemy enemy;

void LoadLevel(const string& filename);
void Setup();
void Draw();
void Input();
void Logic();
void ClearBuffers();
void UpdateBuffer();
void RenderBuffer();
void LoadItems(const string& filename, vector<Item>& templates);
void UseItem(int index);
void ApplyEffect(const Item& item);
void UseBatIfAvailable();

void SetColor(int color) {
    SetConsoleTextAttribute(hConsole, color);
}

void LoadItems(const string& filename, vector<Item>& templates) {
    templates.clear();
    try {
        std::ifstream f(filename);
        if (!f.is_open()) {
            throw std::runtime_error("Could not open items file");
        }

        json data = json::parse(f);
        if (data.is_array()) {
            for (auto& item_data : data) {
                Item item;
                item.fromJson(item_data);
                templates.push_back(item);
            }
        }
    }
    catch (const std::exception& e) {
        Item bottle;
        bottle.type = "bottle";
        bottle.character = 'B';
        bottle.color = 14;
        bottle.effect = 5;
        bottle.consumable = true;
        bottle.auto_use = true;
        templates.push_back(bottle);

        Item bat;
        bat.type = "bat";
        bat.character = '!';
        bat.color = 13;
        bat.effect = 0;
        bat.duration = 10;
        bat.consumable = true;
        bat.auto_use = false;
        templates.push_back(bat);

        Item almond_water;
        almond_water.type = "almond_water";
        almond_water.character = 'W';
        almond_water.color = 11;
        almond_water.effect = 30;
        almond_water.consumable = true;
        almond_water.auto_use = false;
        templates.push_back(almond_water);

        Item ink;
        ink.type = "ink";
        ink.character = 'I';
        ink.color = 5;
        ink.effect = 0;
        ink.duration = 5;
        ink.consumable = true;
        ink.auto_use = false;
        templates.push_back(ink);
    }
}

void LoadLevel(const string& filename) {
    try {
        std::ifstream f(filename);
        if (!f.is_open()) {
            throw std::runtime_error("Could not open level file");
        }

        json data = json::parse(f);

        gWidth = data.value("width", 40);
        gHeight = data.value("height", 20);

        walls = vector<vector<bool>>(gHeight, vector<bool>(gWidth, false));

        if (data.contains("map") && data["map"].is_array()) {
            vector<string> mapData = data["map"].get<vector<string>>();

            if (mapData.size() < static_cast<size_t>(gHeight)) {
                throw std::runtime_error("Map height doesn't match specified height");
            }

            for (int y = 0; y < gHeight; y++) {
                string row = mapData[y];
                if (row.length() < static_cast<size_t>(gWidth)) {
                    row += string(gWidth - row.length(), ' ');
                }

                for (int x = 0; x < gWidth; x++) {
                    walls[y][x] = (row[x] == '#');
                }
            }
        }

        screen_buffer = vector<string>(gHeight, string(gWidth, ' '));
        color_buffer = vector<WORD>(gHeight * gWidth, kWallColor);
    }
    catch (const std::exception& e) {
        gWidth = 40;
        gHeight = 20;
        walls = vector<vector<bool>>(gHeight, vector<bool>(gWidth, false));
        screen_buffer = vector<string>(gHeight, string(gWidth, ' '));
        color_buffer = vector<WORD>(gHeight * gWidth, kWallColor);

        for (int y = 0; y < gHeight; y++) {
            for (int x = 0; x < gWidth; x++) {
                if (y == 0 || y == gHeight - 1 || x == 0 || x == gWidth - 1) {
                    walls[y][x] = true;
                }
            }
        }
    }
}

void ClearBuffers() {
    for (auto& row : screen_buffer) {
        std::fill(row.begin(), row.end(), ' ');
    }
    std::fill(color_buffer.begin(), color_buffer.end(), kWallColor);
}

void UpdateBuffer() {
    for (int y = 0; y < gHeight; y++) {
        for (int x = 0; x < gWidth; x++) {
            screen_buffer[y][x] = walls[y][x] ? kWall : kEmpty;
            color_buffer[y * gWidth + x] = kWallColor;
        }
    }

    for (const auto& item : items) {
        if (item.y >= 0 && item.y < gHeight && item.x >= 0 && item.x < gWidth) {
            screen_buffer[item.y][item.x] = item.character;
            color_buffer[item.y * gWidth + item.x] = item.color;
        }
    }

    if (enemy.y >= 0 && enemy.y < gHeight && enemy.x >= 0 && enemy.x < gWidth) {
        screen_buffer[enemy.y][enemy.x] = enemy.character;
        color_buffer[enemy.y * gWidth + enemy.x] = enemy.frozen ? 9 : enemy.color;
    }

    if (player_y >= 0 && player_y < gHeight && player_x >= 0 && player_x < gWidth) {
        screen_buffer[player_y][player_x] = kPlayer;
        color_buffer[player_y * gWidth + player_x] = player_invisible ? 8 : kPlayerColor;
    }
}

void RenderBuffer() {
    static COORD cursor_pos = { 0, 0 };

    SetConsoleCursorPosition(hConsole, cursor_pos);

    for (int y = 0; y < gHeight; y++) {
        for (int x = 0; x < gWidth; x++) {
            SetColor(color_buffer[y * gWidth + x]);
            cout << screen_buffer[y][x];
        }
        cout << '\n';
    }

    SetColor(kTextColor);
    auto current_time = steady_clock::now();
    int elapsed = duration_cast<seconds>(current_time - start_time).count();
    timer = 60 + time_bonus - elapsed;

    string clear_line(gWidth, ' ');
    for (int i = 0; i < 8; i++) {
        cout << clear_line << '\n';
    }

    COORD status_pos = { 0, static_cast<SHORT>(gHeight) };
    SetConsoleCursorPosition(hConsole, status_pos);

    cout << "Time left: " << (timer > 0 ? timer : 0) << " seconds\n";
    cout << "Enemy: " << enemy.name << " (speed: " << enemy.speed << ")";

    if (enemy.frozen) {
        int freeze_time = duration_cast<seconds>(monster_freeze_until - current_time).count();
        if (freeze_time > 0) {
            cout << " [FROZEN: " << freeze_time << "s]";
        }
    }
    cout << "\n";

    if (player_invisible) {
        int invis_time = duration_cast<seconds>(player_invisible_until - current_time).count();
        if (invis_time > 0) {
            cout << "Player: INVISIBLE (" << invis_time << "s)\n";
        }
        else {
            cout << "\n";
        }
    }
    else {
        cout << "\n";
    }

    cout << "Bottles collected: " << bottles_collected << "\n";

    cout << "Inventory: ";
    if (inventory.empty()) {
        cout << "Empty";
    }
    else {
        for (int i = 0; i < inventory.size(); i++) {
            SetColor(inventory[i].color);
            cout << inventory[i].character;
            SetColor(kTextColor);
            cout << ":" << inventory[i].type;
            if (i < inventory.size() - 1) cout << " ";
        }
    }
    cout << "\n";

    cout << "Use items: 1-4, Exit: X";
}

void Draw() {
    ClearBuffers();
    UpdateBuffer();
    RenderBuffer();
}

void ApplyEffect(const Item& item) {
    if (item.type == "bat") {
        monster_freeze_until = steady_clock::now() + seconds(item.duration);
        enemy.frozen = true;
        enemy.color = 9;
    }
    else if (item.type == "almond_water") {
        time_bonus += item.effect;
    }
    else if (item.type == "ink") {
        player_invisible_until = steady_clock::now() + seconds(item.duration);
        player_invisible = true;
    }
}

void UseItem(int index) {
    if (index < 0 || index >= inventory.size()) return;

    Item item = inventory[index];
    ApplyEffect(item);

    if (item.consumable) {
        inventory.erase(inventory.begin() + index);
    }
}

void UseBatIfAvailable() {
    for (int i = 0; i < inventory.size(); i++) {
        if (inventory[i].type == "bat") {
            ApplyEffect(inventory[i]);
            inventory.erase(inventory.begin() + i);
            return;
        }
    }
}

void Setup() {
    game_over = false;
    game_won = false;
    bottles_collected = 0;
    inventory.clear();
    monster_frozen = false;
    player_invisible = false;
    start_time = steady_clock::now();

    LoadLevel("level.json");

    enemy.loadFromFile("enemy.json");

    LoadItems("items.json", item_templates);

    do {
        player_x = rand() % gWidth;
        player_y = rand() % gHeight;
    } while (walls[player_y][player_x]);

    items.clear();
    for (int i = 0; i < 3; i++) {
        if (!item_templates.empty()) {
            int index = rand() % item_templates.size();
            Item new_item = item_templates[index];
            bool position_ok;
            do {
                position_ok = true;
                new_item.x = rand() % gWidth;
                new_item.y = rand() % gHeight;

                if (walls[new_item.y][new_item.x]) {
                    position_ok = false;
                    continue;
                }

                if (new_item.x == player_x && new_item.y == player_y) {
                    position_ok = false;
                    continue;
                }

                for (const auto& item : items) {
                    if (item.x == new_item.x && item.y == new_item.y) {
                        position_ok = false;
                        break;
                    }
                }
            } while (!position_ok);
            items.push_back(new_item);
        }
    }

    int min_distance = 10;
    do {
        enemy.x = rand() % gWidth;
        enemy.y = rand() % gHeight;
    } while (walls[enemy.y][enemy.x] ||
        (enemy.x == player_x && enemy.y == player_y) ||
        std::any_of(items.begin(), items.end(), [&](const Item& item) {
            return item.x == enemy.x && item.y == enemy.y;
            }) ||
        (abs(enemy.x - player_x) + abs(enemy.y - player_y) < min_distance));
}

void Input() {
    if (_kbhit()) {
        char key = tolower(_getch());
        int new_x = player_x;
        int new_y = player_y;

        switch (key) {
        case 'a': new_x--; break;
        case 'd': new_x++; break;
        case 'w': new_y--; break;
        case 's': new_y++; break;
        case 'x': game_over = true; break;
        case '1':
            if (inventory.size() > 0) UseItem(0);
            break;
        case '2':
            if (inventory.size() > 1) UseItem(1);
            break;
        case '3':
            if (inventory.size() > 2) UseItem(2);
            break;
        case '4':
            if (inventory.size() > 3) UseItem(3);
            break;
        }

        if (new_x >= 0 && new_x < gWidth &&
            new_y >= 0 && new_y < gHeight &&
            !walls[new_y][new_x]) {
            player_x = new_x;
            player_y = new_y;
        }
    }
}

void Logic() {
    auto now = steady_clock::now();
    if (player_invisible && now >= player_invisible_until) {
        player_invisible = false;
    }

    for (auto it = items.begin(); it != items.end(); ) {
        if (player_x == it->x && player_y == it->y) {
            if (it->auto_use) {
                ApplyEffect(*it);
                if (it->type == "bottle") {
                    bottles_collected++;
                }
            }
            else {
                if (inventory.size() < 4) {
                    inventory.push_back(*it);
                }
            }

            it = items.erase(it);

            if (!item_templates.empty()) {
                int index = rand() % item_templates.size();
                Item new_item = item_templates[index];
                bool position_ok;
                do {
                    position_ok = true;
                    new_item.x = rand() % gWidth;
                    new_item.y = rand() % gHeight;

                    if (walls[new_item.y][new_item.x]) {
                        position_ok = false;
                        continue;
                    }

                    if (new_item.x == player_x && new_item.y == player_y) {
                        position_ok = false;
                        continue;
                    }

                    if (new_item.x == enemy.x && new_item.y == enemy.y) {
                        position_ok = false;
                        continue;
                    }

                    for (const auto& item : items) {
                        if (item.x == new_item.x && item.y == new_item.y) {
                            position_ok = false;
                            break;
                        }
                    }
                } while (!position_ok);
                items.push_back(new_item);
            }

            break;
        }
        else {
            ++it;
        }
    }

    if (bottles_collected >= WINNING_BOTTLES_COUNT) {
        game_won = true;
        game_over = true;
    }

    enemy.move();

    if (player_x == enemy.x && player_y == enemy.y) {
        if (!enemy.frozen) {
            bool has_bat = false;
            for (int i = 0; i < inventory.size(); i++) {
                if (inventory[i].type == "bat") {
                    ApplyEffect(inventory[i]);
                    inventory.erase(inventory.begin() + i);
                    has_bat = true;
                    break;
                }
            }

            if (!has_bat) {
                game_over = true;
            }
        }
    }

    auto current_time = steady_clock::now();
    int elapsed = duration_cast<seconds>(current_time - start_time).count();
    timer = 60 + time_bonus - elapsed;
    if (timer <= 0) game_over = true;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    srand(static_cast<unsigned int>(time(nullptr)));
    Setup();

    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    while (!game_over) {
        Draw();
        Input();
        Logic();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    cursorInfo.bVisible = true;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    SetColor(kTextColor);
    if (game_won) {
        cout << "\nCONGRATULATIONS! You collected " << bottles_collected
            << " bottles and won the game!" << endl;
    }
    else if (timer <= 0) {
        cout << "\nGAME OVER! Time's up!" << endl;
    }
    else {
        cout << "\nGAME OVER! The " << enemy.name << " caught you!" << endl;
    }

    cout << "Total bottles collected: " << bottles_collected << endl;

    return 0;
}