// 局部更新版本
#include <iostream>
#include <windows.h>
#include <conio.h>
#include <time.h>
#include <fstream>
#include <string>
#include <cstring>

#define STAGE_WIDTH 20
#define STAGE_HEIGHT 20
#define WINDOW_WIDTH 80
#define WINDOW_HEIGHT 25
#define CORNER_X 1
#define CORNER_Y 1
#define THICHKNESS 1
#define MAXLENGTH 100
#define COLOR_WALL 0X06
#define COLOR_TEXT 0X0F
#define COLOR_TEXT4END 0XEC
#define COLOR_SCORE 0X0C
#define COLOR_FRUIT 0X0C
#define COLOR_SNAKE_HEAD 0X09
#define COLOR_SNKAE_BODY 0X0A
#include <stdio.h>
#define DIFFICULTY_FACTOR 50

using namespace std;

HANDLE hBuffer[2];
int hActive = 0;
HANDLE h;

struct MyCout {
    MyCout& operator<<(const char* str) {
        DWORD w;
        WriteConsoleA(h, str, (DWORD)strlen(str), &w, NULL);
        return *this;
    }
    MyCout& operator<<(int val) {
        char buf[32];
        sprintf(buf, "%d", val);
        DWORD w;
        WriteConsoleA(h, buf, (DWORD)strlen(buf), &w, NULL);
        return *this;
    }
    MyCout& operator<<(MyCout& (*pf)(MyCout&)) {
        return pf(*this);
    }
} myCout;

MyCout& myEndl(MyCout& mc) {
    DWORD w;
    WriteConsoleA(h, "\n", 1, &w, NULL);
    return mc;
}

#define cout myCout
#define endl myEndl

const int mWidth = STAGE_WIDTH;
const int mHeight = STAGE_HEIGHT;
const char* SAVE_FILE_PATH = "snake_save.txt";

bool isFlash;
bool isFullWidth;
bool gameQuit;

int headX, headY, fruitX, fruitY, mScore;
int tailX[MAXLENGTH], tailY[MAXLENGTH];
int nTail;

string statusMessage;
int statusTicks;

enum eDirection { STOP = 0, LEFT, RIGHT, UP, DOWN };
eDirection dir;

enum GameState { STATE_START = 0, STATE_RUNNING, STATE_PAUSED, STATE_OVER };
GameState gameState;

void Prompt_info(int _x, int _y);
void showScore(int _x, int _y);
void DrawStateInfo();
void RenderFrameDoubleBuffered();

void setPos(int X, int Y)
{
    COORD pos;
    int cellWidth = isFullWidth ? 2 : 1;
    pos.X = CORNER_X + X * cellWidth + THICHKNESS;
    pos.Y = CORNER_Y + Y + THICHKNESS;
    SetConsoleCursorPosition(h, pos);
}

void SetStatus(const char* message, int ttl = 100)
{
    statusMessage = message;
    statusTicks = ttl;
}

bool IsOpposite(eDirection lhs, eDirection rhs)
{
    return (lhs == LEFT && rhs == RIGHT) ||
        (lhs == RIGHT && rhs == LEFT) ||
        (lhs == UP && rhs == DOWN) ||
        (lhs == DOWN && rhs == UP);
}

void SpawnFruit()
{
    bool occupied = false;
    do {
        occupied = false;
        fruitX = rand() % mWidth;
        fruitY = rand() % mHeight;

        if (fruitX == headX && fruitY == headY)
            occupied = true;

        for (int i = 0; i < nTail && !occupied; i++)
            if (tailX[i] == fruitX && tailY[i] == fruitY)
                occupied = true;
    } while (occupied);
}

void ResetGameData()
{
    isFlash = false;

    gameState = STATE_START;
    dir = STOP;

    headX = mWidth / 2;
    headY = mHeight / 2;
    mScore = 0;

    nTail = 0;
    for (int i = 0; i < MAXLENGTH; i++)
    {
        tailX[i] = 0;
        tailY[i] = 0;
    }

    SpawnFruit();
    SetStatus("方向键/WASD开始，K存档，L读档，X退出", 150);
}

bool SaveGame(const char* filePath)
{
    ofstream ofs(filePath);
    if (!ofs)
        return false;

    ofs << "SNAKE_SAVE_V1\n";
    ofs << headX << " " << headY << "\n";
    ofs << nTail << "\n";

    for (int i = 0; i < nTail; i++)
        ofs << tailX[i] << " " << tailY[i] << "\n";

    ofs << (int)dir << "\n";
    ofs << fruitX << " " << fruitY << "\n";
    ofs << mScore << "\n";
    ofs << (int)gameState << "\n";

    return ofs.good();
}

bool LoadGame(const char* filePath)
{
    ifstream ifs(filePath);
    if (!ifs)
        return false;

    string header;
    if (!(ifs >> header) || header != "SNAKE_SAVE_V1")
        return false;

    int tHeadX, tHeadY;
    int tTailCount;
    int tDir;
    int tFruitX, tFruitY;
    int tScore;
    int tState;
    int tTailX[MAXLENGTH], tTailY[MAXLENGTH];

    if (!(ifs >> tHeadX >> tHeadY))
        return false;
    if (!(ifs >> tTailCount))
        return false;

    if (tTailCount < 0 || tTailCount > MAXLENGTH)
        return false;

    for (int i = 0; i < tTailCount; i++)
        if (!(ifs >> tTailX[i] >> tTailY[i]))
            return false;

    if (!(ifs >> tDir))
        return false;
    if (!(ifs >> tFruitX >> tFruitY))
        return false;
    if (!(ifs >> tScore))
        return false;
    if (!(ifs >> tState))
        return false;

    if (tHeadX < 0 || tHeadX >= mWidth || tHeadY < 0 || tHeadY >= mHeight)
        return false;
    if (tFruitX < 0 || tFruitX >= mWidth || tFruitY < 0 || tFruitY >= mHeight)
        return false;
    if (tDir < STOP || tDir > DOWN)
        return false;
    if (tScore < 0)
        return false;
    if (tState < STATE_START || tState > STATE_OVER)
        return false;

    for (int i = 0; i < tTailCount; i++)
    {
        if (tTailX[i] < 0 || tTailX[i] >= mWidth || tTailY[i] < 0 || tTailY[i] >= mHeight)
            return false;
    }

    headX = tHeadX;
    headY = tHeadY;
    nTail = tTailCount;
    for (int i = 0; i < nTail; i++)
    {
        tailX[i] = tTailX[i];
        tailY[i] = tTailY[i];
    }
    for (int i = nTail; i < MAXLENGTH; i++)
    {
        tailX[i] = 0;
        tailY[i] = 0;
    }

    dir = (eDirection)tDir;
    fruitX = tFruitX;
    fruitY = tFruitY;
    mScore = tScore;

    if (tState == STATE_OVER)
        gameState = STATE_PAUSED;
    else
        gameState = (GameState)tState;

    isFlash = false;
    return true;
}

void Initial()
{
    static bool inited = false;
    if (!inited) {
        hBuffer[0] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        hBuffer[1] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        inited = true;
    }

    hActive = 0;
    h = hBuffer[hActive];
    SetConsoleActiveScreenBuffer(h);

    SetConsoleTitleW(L"Console_贪吃蛇*双缓冲版*");
    COORD dSize = { WINDOW_WIDTH, WINDOW_HEIGHT };
    SetConsoleScreenBufferSize(hBuffer[0], dSize);
    SetConsoleScreenBufferSize(hBuffer[1], dSize);

    CONSOLE_CURSOR_INFO _cursor = { 1, false };
    SetConsoleCursorInfo(hBuffer[0], &_cursor);
    SetConsoleCursorInfo(hBuffer[1], &_cursor);

    static bool seeded = false;
    if (!seeded)
    {
        srand((unsigned)time(NULL));
        seeded = true;
    }

    isFullWidth = true;
    statusTicks = 0;
    ResetGameData();
}

void ClearScreen()
{
    COORD coordScreen = { 0, 0 };
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    GetConsoleScreenBufferInfo(h, &csbi);
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacterA(h, (CHAR)' ', dwConSize, coordScreen, &cCharsWritten);
    FillConsoleOutputAttribute(h, COLOR_TEXT, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(h, coordScreen);
}

void DrawMap()
{
    SetConsoleTextAttribute(h, COLOR_WALL);
    setPos(-THICHKNESS, -THICHKNESS);
    for (int i = 0; i < mWidth + THICHKNESS * 2; i++)
        if (isFullWidth)
            cout << "[]";
        else
            cout << "#";

    for (int i = 0; i < mHeight; i++)
    {
        setPos(-THICHKNESS, i);
        for (int j = 0; j < mWidth + THICHKNESS * 2; j++)
        {
            if (j == 0 || j == mWidth + THICHKNESS)
            {
                if (isFullWidth)
                    cout << "[]";
                else
                    cout << "#";
            }
            else
            {
                if (isFullWidth)
                    cout << "  ";
                else
                    cout << " ";
            }
        }
        cout << endl;
    }

    setPos(-THICHKNESS, STAGE_HEIGHT);
    for (int i = 0; i < mWidth + THICHKNESS * 2; i++)
        if (isFullWidth)
            cout << "[]";
        else
            cout << "#";
}

void DrawLocally()
{
    DrawMap();
    Prompt_info(5, 1);

    if (!isFlash)
    {
        setPos(fruitX, fruitY);
        SetConsoleTextAttribute(h, COLOR_FRUIT);
        if (isFullWidth)
            cout << "* ";
        else
            cout << "F";
        isFlash = true;
    }
    else
    {
        setPos(fruitX, fruitY);
        SetConsoleTextAttribute(h, COLOR_FRUIT);
        if (isFullWidth)
            cout << "  ";
        else
            cout << " ";
        isFlash = false;
    }

    setPos(headX, headY);
    SetConsoleTextAttribute(h, COLOR_SNAKE_HEAD);
    if (isFullWidth)
        cout << "@ ";
    else
        cout << "O";

    for (int i = 0; i < nTail; i++)
    {
        setPos(tailX[i], tailY[i]);
        SetConsoleTextAttribute(h, COLOR_SNKAE_BODY);
        if (isFullWidth)
            cout << "o ";
        else
            cout << "o";
    }

    setPos(0, STAGE_HEIGHT + THICHKNESS * 2);
    SetConsoleTextAttribute(h, COLOR_TEXT);
    cout << "游戏得分 " << mScore;
}

void TrySetDirection(eDirection newDir)
{
    if (newDir == STOP)
        return;

    if (dir != STOP && IsOpposite(dir, newDir))
        return;

    dir = newDir;
    if (gameState == STATE_START)
        gameState = STATE_RUNNING;
}

void Input()
{
    if (!_kbhit())
        return;

    int key = _getch();
    bool isArrow = false;

    if (key == 224 || key == 0)
    {
        isArrow = true;
        key = _getch();
    }

    eDirection newDir = STOP;
    if (!isArrow)
    {
        switch (key)
        {
        case 'a':
        case 'A':
            newDir = LEFT;
            break;
        case 'd':
        case 'D':
            newDir = RIGHT;
            break;
        case 'w':
        case 'W':
            newDir = UP;
            break;
        case 's':
        case 'S':
            newDir = DOWN;
            break;
        default:
            break;
        }
    }
    else
    {
        switch (key)
        {
        case 72:
            newDir = UP;
            break;
        case 80:
            newDir = DOWN;
            break;
        case 75:
            newDir = LEFT;
            break;
        case 77:
            newDir = RIGHT;
            break;
        default:
            break;
        }
    }

    if (newDir != STOP)
    {
        if (gameState != STATE_OVER)
            TrySetDirection(newDir);
        return;
    }

    switch (key)
    {
    case ' ':
        if (gameState == STATE_RUNNING)
        {
            gameState = STATE_PAUSED;
            SetStatus("已暂停，按 Space 继续", 100);
        }
        else if (gameState == STATE_PAUSED)
        {
            gameState = STATE_RUNNING;
            SetStatus("继续游戏", 60);
        }
        break;
    case 'k':
    case 'K':
        if (SaveGame(SAVE_FILE_PATH))
            SetStatus("存档成功: snake_save.txt", 120);
        else
            SetStatus("存档失败", 120);
        break;
    case 'l':
    case 'L':
        if (LoadGame(SAVE_FILE_PATH))
            SetStatus("读档成功", 120);
        else
            SetStatus("读档失败: 文件不存在或格式错误", 150);
        break;
    case 'y':
    case 'Y':
        if (gameState == STATE_OVER)
        {
            ResetGameData();
            SetStatus("新游戏已重置", 100);
        }
        break;
    case 'x':
    case 'X':
        gameQuit = true;
        break;
    case 13:
        isFullWidth = !isFullWidth;
        break;
    default:
        break;
    }
}

void Logic()
{
    if (gameState != STATE_RUNNING || dir == STOP)
        return;

    int oldHeadX = headX;
    int oldHeadY = headY;
    int prevLastX = oldHeadX;
    int prevLastY = oldHeadY;

    if (nTail > 0)
    {
        prevLastX = tailX[nTail - 1];
        prevLastY = tailY[nTail - 1];
        int shiftLimit = nTail < MAXLENGTH ? nTail : MAXLENGTH - 1;
        for (int i = shiftLimit; i > 0; i--)
        {
            tailX[i] = tailX[i - 1];
            tailY[i] = tailY[i - 1];
        }
        tailX[0] = oldHeadX;
        tailY[0] = oldHeadY;
    }

    switch (dir)
    {
    case LEFT:
        headX--;
        break;
    case RIGHT:
        headX++;
        break;
    case UP:
        headY--;
        break;
    case DOWN:
        headY++;
        break;
    default:
        break;
    }

    if (headX < 0)
        headX = mWidth - 1;
    else if (headX >= mWidth)
        headX = 0;

    if (headY < 0)
        headY = mHeight - 1;
    else if (headY >= mHeight)
        headY = 0;

    for (int i = 0; i < nTail; i++)
    {
        if (tailX[i] == headX && tailY[i] == headY)
        {
            gameState = STATE_OVER;
            SetStatus("撞到自己，游戏结束。Y重开 / L读档恢复", 180);
            return;
        }
    }

    if (headX == fruitX && headY == fruitY)
    {
        mScore += 10;
        if (nTail < MAXLENGTH)
        {
            tailX[nTail] = prevLastX;
            tailY[nTail] = prevLastY;
            nTail++;
        }
        SpawnFruit();
    }
}

void Prompt_info(int _x, int _y)
{
    int initialX = 20, initialY = 0;

    SetConsoleTextAttribute(h, COLOR_TEXT);
    setPos(_x + initialX, _y + initialY);
    cout << "■ 游戏说明：";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    A.自撞，游戏结束";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    B.撞墙可穿越到对侧";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    C.吃食物后长度+1，得分+10";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    D.双缓冲后台构建后一次输出";
    initialY += 2;

    setPos(_x + initialX, _y + initialY);
    cout << "■ 操作说明：";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    □ 开始移动：方向键 / WASD";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    □ 暂停/继续：Space";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    □ 存档：K";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    □ 读档：L";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    □ 结束后重开：Y";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    □ 退出游戏：X";
    initialY++;
    setPos(_x + initialX, _y + initialY);
    cout << "    □ 舞台宽度切换：Enter";
    initialY += 2;

    setPos(_x + initialX, _y + initialY);
    cout << "■ 存档文件：snake_save.txt";
}

void showScore(int _x, int _y)
{
    setPos(_x + 20, _y + 15);
    SetConsoleTextAttribute(h, COLOR_TEXT);
    int s = mScore / DIFFICULTY_FACTOR;
    cout << "☆ 当前难度:     级";
    if (isFullWidth)
        setPos(_x + 27, _y + 15);
    else
        setPos(_x + 34, _y + 15);
    SetConsoleTextAttribute(h, COLOR_SCORE);
    cout << s;

    setPos(_x + 20, _y + 17);
    SetConsoleTextAttribute(h, COLOR_TEXT);
    cout << "● 当前积分:  ";
    SetConsoleTextAttribute(h, COLOR_SCORE);
    cout << mScore;
}

void DrawStateInfo()
{
    setPos(0, STAGE_HEIGHT + THICHKNESS * 3);
    SetConsoleTextAttribute(h, COLOR_TEXT);

    if (gameState == STATE_START)
        cout << "状态: 开始  按方向键/WASD启动";
    else if (gameState == STATE_RUNNING)
        cout << "状态: 运行  Space暂停  K存档  L读档  X退出";
    else if (gameState == STATE_PAUSED)
        cout << "状态: 暂停  Space继续  K存档  L读档  X退出";
    else
        cout << "状态: 结束  Y重开  L读档恢复  X退出";

    if (statusTicks > 0)
    {
        setPos(0, STAGE_HEIGHT + THICHKNESS * 4);
        SetConsoleTextAttribute(h, COLOR_SCORE);
        cout << statusMessage.c_str();
    }
}

void gameOver_info()
{
    if (gameState != STATE_OVER)
        return;

    setPos(5, 8);
    SetConsoleTextAttribute(h, COLOR_TEXT4END);
    cout << "游戏结束!!";
    setPos(3, 9);
    cout << "Y重开 / L读档恢复 / X退出";
    SetConsoleTextAttribute(h, COLOR_TEXT);
}

void RenderFrameDoubleBuffered()
{
    // 在后台缓冲区完整构建一帧，再一次性切到前台显示。
    int backIndex = 1 - hActive;
    h = hBuffer[backIndex];

    ClearScreen();
    DrawLocally();
    showScore(5, 1);
    DrawStateInfo();
    gameOver_info();

    // 单次抛出，避免前台逐字符刷新造成闪烁。
    SetConsoleActiveScreenBuffer(hBuffer[backIndex]);
    hActive = backIndex;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    gameQuit = false;
    Initial();

    while (!gameQuit)
    {
        Input();
        Logic();

        RenderFrameDoubleBuffered();

        if (statusTicks > 0)
            statusTicks--;

        int dwMilliseconds = 50;
        if (gameState == STATE_RUNNING)
        {
            dwMilliseconds = 200 / (mScore / DIFFICULTY_FACTOR + 1);
            if (dwMilliseconds < 40)
                dwMilliseconds = 40;
        }
        Sleep(dwMilliseconds);
    }

    setPos(0, STAGE_HEIGHT + THICHKNESS * 3);
    return 0;
}