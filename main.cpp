#include <iostream>
#include <ncurses.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

using namespace std;

const int HEIGHT = 21;
const int WIDTH = 41;
const int TICK = 250000;

enum CellType {
    EMPTY = 0, WALL = 1, IMMUNE_WALL = 2,
    SNAKE_HEAD = 3, SNAKE_BODY = 4,
    GROWTH_ITEM = 5, POISON_ITEM = 6,
    GATE = 7
};

enum Direction { UP, DOWN, LEFT, RIGHT };

struct Position {
    int y, x;
    Position(int y = 0, int x = 0) : y(y), x(x) {}
    bool operator==(const Position& other) const {
        return y == other.y && x == other.x;
    }
};

class Snake {
public:
    vector<Position> body;
    Direction dir;

    Snake(Position start) {
        dir = RIGHT;
        body.push_back(start);
        body.push_back(Position(start.y, start.x - 1));
        body.push_back(Position(start.y, start.x - 2));
    }

    Position next_head() const {
        Position head = body.front();
        switch (dir) {
            case UP: return Position(head.y - 1, head.x);
            case DOWN: return Position(head.y + 1, head.x);
            case LEFT: return Position(head.y, head.x - 1);
            case RIGHT: return Position(head.y, head.x + 1);
        }
        return head;
    }

    void move(Position next, bool grow = false) {
        body.insert(body.begin(), next);
        if (!grow) body.pop_back();
    }

    bool is_collision(const Position& pos) const {
        for (const auto& part : body) {
            if (part == pos) return true;
        }
        return false;
    }

    int length() const {
        return body.size();
    }
};

int map[HEIGHT][WIDTH];
vector<Position> items;
vector<Position> gates;
int growthCount = 0;
int poisonCount = 0;
int gateUseCount = 0;
int maxLength = 3;

struct Mission {
    int targetLength;
    int growthTarget;
    int poisonTarget;
    int gateTarget;
};

vector<Mission> missions = {
    {7,  4, 2, 1},
    {10, 5, 3, 2},
    {12, 7, 5, 3},
    {15, 8, 5, 4}
};

void show_game_over() {
    clear();
    string msg = "Game Over!";
    int y = HEIGHT / 2;
    int x = (WIDTH - msg.size()) / 2;
    mvprintw(y, x, "%s", msg.c_str());
    mvprintw(y + 1, x, "Press any key to exit...");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    endwin();
}

void show_mission_complete(int stageNum) {
    clear();
    string msg = "Stage " + to_string(stageNum) + " Clear!";
    int y = HEIGHT / 2;
    int x = (WIDTH - msg.size()) / 2;
    mvprintw(y, x, "%s", msg.c_str());
    mvprintw(y + 1, x, "Press any key to continue...");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE); // 다시 비동기 입력 모드로 설정
}

void draw_score_and_mission(int stageNum, int currentLength, Mission mission) {
    int base_y = 3, base_x = WIDTH + 3;

    mvprintw(base_y - 1, base_x, " < Stage %d >", stageNum);
    
    // [1] 스코어보드 WINDOW
    int sb_height = 7, sb_width = 25;
    static WINDOW* score_win = nullptr;
    if (!score_win) score_win = newwin(sb_height, sb_width, base_y, base_x);
    werase(score_win);
    box(score_win, 0, 0);
    int y = 1;
    mvwprintw(score_win, y++, 2, "< Score Board >");
    mvwprintw(score_win, y++, 2, "B: %2d /%2d (CURR / MAX)", currentLength, maxLength);
    mvwprintw(score_win, y++, 2, "+: %2d", growthCount);
    mvwprintw(score_win, y++, 2, "-: %2d", poisonCount);
    mvwprintw(score_win, y++, 2, "G: %2d", gateUseCount);
    wrefresh(score_win);

    // [2] 미션 WINDOW
    int ms_height = 7, ms_width = 25;
    static WINDOW* mission_win = nullptr;
    if (!mission_win) mission_win = newwin(ms_height, ms_width, base_y + sb_height + 1, base_x);
    werase(mission_win);
    box(mission_win, 0, 0);
    y = 1;
    mvwprintw(mission_win, y++, 2, "< Mission >");
    mvwprintw(mission_win, y++, 2, "B: %2d (%c)", mission.targetLength, (currentLength >= mission.targetLength) ? 'v' : ' ');
    mvwprintw(mission_win, y++, 2, "+: %2d (%c)", mission.growthTarget, (growthCount >= mission.growthTarget) ? 'v' : ' ');
    mvwprintw(mission_win, y++, 2, "-: %2d (%c)", mission.poisonTarget, (poisonCount >= mission.poisonTarget) ? 'v' : ' ');
    mvwprintw(mission_win, y++, 2, "G: %2d (%c)", mission.gateTarget, (gateUseCount >= mission.gateTarget) ? 'v' : ' ');
    wrefresh(mission_win);

}

void draw_map(Snake& snake, Mission mission, int stageNum) {
    for (size_t i = 0; i < snake.body.size(); ++i) {
        int y = snake.body[i].y;
        int x = snake.body[i].x;
        map[y][x] = (i == 0) ? SNAKE_HEAD : SNAKE_BODY;
    }

    for (int i = 0; i < HEIGHT; ++i) {
        move(i, 0);
        for (int j = 0; j < WIDTH; ++j) {
            switch (map[i][j]) {
                case EMPTY: printw(" "); break;
                case WALL: printw("W"); break;
                case IMMUNE_WALL: printw("I"); break;
                case SNAKE_HEAD: printw("@"); break;
                case SNAKE_BODY: printw("o"); break;
                case GROWTH_ITEM: printw("+"); break;
                case POISON_ITEM: printw("-"); break;
                case GATE: printw("G"); break;
            }
        }
    }

    refresh();
    draw_score_and_mission(stageNum, snake.length(), mission);
}

void init_map(int stage) {
    for (int i = 0; i < HEIGHT; ++i) {
        for (int j = 0; j < WIDTH; ++j) {
            map[i][j] = EMPTY;
        }
    }

    // 공통 외벽
    for (int i = 0; i < HEIGHT; ++i) {
        map[i][0] = map[i][WIDTH - 1] = WALL;
    }
    for (int j = 0; j < WIDTH; ++j) {
        map[0][j] = map[HEIGHT - 1][j] = WALL;
    }

    // 모서리는 IMMUNE_WALL로
    map[0][0] = map[0][WIDTH - 1] = IMMUNE_WALL;
    map[HEIGHT - 1][0] = map[HEIGHT - 1][WIDTH - 1] = IMMUNE_WALL;

    // 스테이지별 내부 구조 추가
    if (stage == 0) {
        // stage 1: 없음 (기본 외벽)
    } else if (stage == 1) {
        for (int i = 5; i < 16; ++i) map[i][25] = WALL;
    } else if (stage == 2) {
        for (int j = 10; j < 20; ++j) map[10][j] = WALL;
        for (int i = 6; i < 15; ++i) map[i][30] = WALL;
    } else if (stage == 3) {
        for (int j = 10; j < 31; ++j) map[10][j] = WALL;
        for (int i = 5; i < 10; ++i) map[i][30] = WALL;
        for (int i = 11; i < 16; ++i) map[i][10] = WALL;
    }
}

void clear_snake_on_map(Snake& snake) {
    for (const auto& part : snake.body) {
        map[part.y][part.x] = EMPTY;
    }
}

void spawn_items_balanced() {
    int total_items = 0, growth = 0, poison = 0;
    for (auto& p : items) {
        if (map[p.y][p.x] == GROWTH_ITEM || map[p.y][p.x] == POISON_ITEM)
            map[p.y][p.x] = EMPTY;
    }
    items.clear();

    while (total_items < 3) {
        int y = rand() % (HEIGHT - 2) + 1;
        int x = rand() % (WIDTH - 2) + 1;
        if (map[y][x] != EMPTY) continue;
        int type = (growth <= poison) ? GROWTH_ITEM : POISON_ITEM;
        if (type == GROWTH_ITEM) growth++; else poison++;
        map[y][x] = type;
        items.push_back(Position(y, x));
        total_items++;
    }
}

void spawn_gates() {
    gates.clear();
    vector<Position> candidates;
    for (int i = 0; i < HEIGHT; ++i) {
        for (int j = 0; j < WIDTH; ++j) {
            if (map[i][j] == WALL) {
                if ((i == 0 && j == 0) || (i == 0 && j == WIDTH - 1) ||
                    (i == HEIGHT - 1 && j == 0) || (i == HEIGHT - 1 && j == WIDTH - 1)) continue;
                candidates.push_back(Position(i, j));
            }
        }
    }
    while (gates.size() < 2 && !candidates.empty()) {
        int idx = rand() % candidates.size();
        Position p = candidates[idx];
        candidates.erase(candidates.begin() + idx);
        gates.push_back(p);
        map[p.y][p.x] = GATE;
    }
}

Direction get_exit_direction(Position gate, Direction in_dir) {
    if (gate.y == 0) return DOWN;
    if (gate.y == HEIGHT - 1) return UP;
    if (gate.x == 0) return RIGHT;
    if (gate.x == WIDTH - 1) return LEFT;
    vector<Direction> priority = {in_dir, static_cast<Direction>((in_dir + 1) % 4), static_cast<Direction>((in_dir + 3) % 4), static_cast<Direction>((in_dir + 2) % 4)};
    for (Direction d : priority) {
        Position np;
        switch (d) {
            case UP: np = Position(gate.y - 1, gate.x); break;
            case DOWN: np = Position(gate.y + 1, gate.x); break;
            case LEFT: np = Position(gate.y, gate.x - 1); break;
            case RIGHT: np = Position(gate.y, gate.x + 1); break;
        }
        if (map[np.y][np.x] == EMPTY) return d;
    }
    return in_dir;
}

int main() {
    srand(time(0));
    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    for (int stage = 0; stage < 4; ++stage) {
        Snake snake(Position(5, 5));
        init_map(stage);
        spawn_gates();
        spawn_items_balanced();
        growthCount = poisonCount = gateUseCount = 0;
        maxLength = 3;
        int tick_count = 0;
        Mission mission = missions[stage];

        while (true) {
            int key = getch();
            switch (key) {
                case KEY_UP: if (snake.dir == DOWN) { show_game_over(); return 0; } else if (snake.dir != UP) snake.dir = UP; break;
                case KEY_DOWN: if (snake.dir == UP) { show_game_over(); return 0; } else if (snake.dir != DOWN) snake.dir = DOWN; break;
                case KEY_LEFT: if (snake.dir == RIGHT) { show_game_over(); return 0; } else if (snake.dir != LEFT) snake.dir = LEFT; break;
                case KEY_RIGHT: if (snake.dir == LEFT) { show_game_over(); return 0; } else if (snake.dir != RIGHT) snake.dir = RIGHT; break;
            }
            Position next = snake.next_head();
            if (map[next.y][next.x] == WALL || map[next.y][next.x] == IMMUNE_WALL || snake.is_collision(next)) {
                show_game_over(); return 0;
            }
            clear_snake_on_map(snake);
            if (map[next.y][next.x] == GROWTH_ITEM) {
                map[next.y][next.x] = EMPTY;
                growthCount++;
                if (snake.length() == maxLength) maxLength++;
                snake.move(next, true);
            } else if (map[next.y][next.x] == POISON_ITEM) {
                if (snake.length() <= 3) { show_game_over(); return 0; }
                map[next.y][next.x] = EMPTY;
                poisonCount++;
                snake.move(next);
                snake.body.pop_back();
            } else if (map[next.y][next.x] == GATE) {
                gateUseCount++;
                Position in_gate = next;
                Position out_gate = (in_gate == gates[0]) ? gates[1] : gates[0];
                Direction exit_dir = get_exit_direction(out_gate, snake.dir);
                Position exit_pos;
                switch (exit_dir) {
                    case UP: exit_pos = Position(out_gate.y - 1, out_gate.x); break;
                    case DOWN: exit_pos = Position(out_gate.y + 1, out_gate.x); break;
                    case LEFT: exit_pos = Position(out_gate.y, out_gate.x - 1); break;
                    case RIGHT: exit_pos = Position(out_gate.y, out_gate.x + 1); break;
                }
                snake.dir = exit_dir;
                snake.move(exit_pos);
            } else {
                snake.move(next);
            }
            if (++tick_count % 30 == 0) spawn_items_balanced();
            draw_map(snake, mission, stage + 1);
            if (snake.length() >= mission.targetLength && growthCount >= mission.growthTarget && poisonCount >= mission.poisonTarget && gateUseCount >= mission.gateTarget) {
                show_mission_complete(stage + 1);
                break;
            }
            usleep(TICK);
        }
    }

    clear();
    string msg1 = "!!  Congraturations   !!";
    string msg2 = "!! All Stages Cleared !!";
    int y = HEIGHT / 2;
    int x = (WIDTH - msg2.size()) / 2;
    mvprintw(y, x, "%s", msg1.c_str());
    mvprintw(y+1, x, "%s", msg2.c_str());
    mvprintw(y+2, x, "Press any key to exit...");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    endwin();
    return 0;
}