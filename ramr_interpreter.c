// ramr.c — RAMR 0.2.1 (с таймерами и GUI Win32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <windows.h>
#include <conio.h>

#define MAX_LINE 1024
#define MAX_VARS 1024
#define MAX_FUNCTIONS 100
#define MAX_FUNC_LINES 50
#define MAX_GUI_ELEMENTS 50
#define MAX_TIMERS 20

typedef struct {
    char *key;
    char *value;
} Var;

typedef struct {
    char *text;
    int x, y;
    int width, height;
    COLORREF color;
    int font_size;
} GuiText;

typedef struct {
    char *text;
    int x, y;
    int width, height;
    char *function;
    HWND hwnd;
} GuiButton;

typedef struct {
    int width;
    int height;
    char *title;
    COLORREF bg_color;
    GuiText *texts;
    int text_count;
    GuiButton *buttons;
    int button_count;
    HWND hwnd;
    HINSTANCE hInstance;
    int gui_running;
    HFONT hFont;
} GuiState;

typedef struct {
    char *name;
    char **lines;
    int line_count;
} Function;

typedef struct {
    int active;             // 0 = неактивен, 1 = активен
    int seconds;            // через сколько секунд сработает
    time_t start_time;      // время старта
    char *function_name;    // кого вызвать
} Timer;

/* --- Глобальные --- */
static Var vars[MAX_VARS];
static int var_count = 0;
static Function functions[MAX_FUNCTIONS];
static int func_count = 0;
static int running = 1;
static int in_function_def = 0;
static char current_func_name[256] = "";
static char **function_lines = NULL;
static int function_line_count = 0;
static int skip_block = 0;
static int if_condition_met = 0;

static GuiState gui = {0};

static Timer timers[MAX_TIMERS];
static int timer_count = 0;

/* --- Прототипы --- */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void gui_run_command(void);
void gui_cleanup(void);

COLORREF get_color_from_name(const char *color_name);
char *strdup_or_null(const char *s);
void *xrealloc(void *ptr, size_t size);
void set_var(const char *key, const char *value);
const char *get_var(const char *key);
char *next_token(char **line_ptr);
char *substitute_vars(const char *input);
char *random_choice(char **items, int n);
char *random_text(int length);
char *random_digits(int length);
void setup_file_associations(void);
char **read_file_lines(const char *path, int *out_count);
void exec_block(char **lines, int start, int end);
Function* find_function(const char *name);
void exec_function(const char *name);
void load_module(const char *module_name);
void exec_line(char *line);
void update_timers(void);

/* --- Реализация вспомогательных --- */
COLORREF get_color_from_name(const char *color_name) {
    if (!color_name) return RGB(0,0,0);
    if (strcmp(color_name,"red")==0) return RGB(255,0,0);
    if (strcmp(color_name,"green")==0) return RGB(0,255,0);
    if (strcmp(color_name,"blue")==0) return RGB(0,0,255);
    if (strcmp(color_name,"white")==0) return RGB(255,255,255);
    if (strcmp(color_name,"black")==0) return RGB(0,0,0);
    if (strcmp(color_name,"yellow")==0) return RGB(255,255,0);
    if (strcmp(color_name,"lightblue")==0) return RGB(173,216,230);
    if (strcmp(color_name,"darkblue")==0) return RGB(0,0,139);
    if (strcmp(color_name,"gray")==0) return RGB(128,128,128);
    if (strcmp(color_name,"orange")==0) return RGB(255,165,0);
    if (strcmp(color_name,"pink")==0) return RGB(255,192,203);
    if (strcmp(color_name,"brown")==0) return RGB(165,42,42);
    return RGB(0,0,0);
}

char *strdup_or_null(const char *s) {
    if (!s) return NULL;
    char *r = malloc(strlen(s) + 1);
    if (r) strcpy(r, s);
    return r;
}

void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p && size != 0) {
        fprintf(stderr, "[fatal] out of memory\n");
        exit(1);
    }
    return p;
}

void set_var(const char *key, const char *value) {
    for (int i=0;i<var_count;i++) {
        if (strcmp(vars[i].key, key)==0) {
            free(vars[i].value);
            vars[i].value = strdup_or_null(value);
            return;
        }
    }
    if (var_count < MAX_VARS) {
        vars[var_count].key = strdup_or_null(key);
        vars[var_count].value = strdup_or_null(value);
        var_count++;
    } else {
        fprintf(stderr, "[warning] var storage full\n");
    }
}

const char *get_var(const char *key) {
    for (int i=0;i<var_count;i++) {
        if (strcmp(vars[i].key, key)==0) return vars[i].value;
    }
    return "";
}

char *next_token(char **line_ptr) {
    char *s = *line_ptr;
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return NULL;
    char buf[MAX_LINE]; buf[0]='\0';
    if (*s == '"') {
        s++;
        char *d = buf;
        while (*s && *s != '"') *d++ = *s++;
        *d = '\0';
        if (*s == '"') s++;
        *line_ptr = s;
        return strdup_or_null(buf);
    } else {
        char *d = buf;
        while (*s && !isspace((unsigned char)*s)) *d++ = *s++;
        *d = '\0';
        *line_ptr = s;
        return strdup_or_null(buf);
    }
}

char *substitute_vars(const char *input) {
    if (!input) return strdup_or_null("");
    char out[MAX_LINE * 2]; out[0] = '\0';
    const char *p = input;
    while (*p) {
        if (*p == '{') {
            p++;
            char name[256]; int ni=0;
            while (*p && *p != '}' && ni < 255) name[ni++] = *p++;
            name[ni] = '\0';
            if (*p == '}') p++;
            const char *val = get_var(name);
            strncat(out, val, sizeof(out)-strlen(out)-1);
        } else {
            char tmp[2] = {*p, '\0'};
            strncat(out, tmp, sizeof(out)-strlen(out)-1);
            p++;
        }
    }
    return strdup_or_null(out);
}

char *random_choice(char **items, int n) {
    if (n <= 0) return strdup_or_null("");
    int idx = rand() % n;
    return strdup_or_null(items[idx]);
}

char *random_text(int length) {
    if (length <= 0) length = 8;
    char *s = malloc(length+1);
    for (int i=0;i<length;i++) s[i] = 'a' + (rand()%26);
    s[length] = '\0';
    return s;
}

char *random_digits(int length) {
    if (length <= 0) length = 6;
    char *s = malloc(length+1);
    for (int i=0;i<length;i++) s[i] = '0' + (rand()%10);
    s[length] = '\0';
    return s;
}

void setup_file_associations() {
    HKEY hKey;
    DWORD dwDisposition;
    LONG result;
    char interpreter_path[MAX_PATH];
    GetModuleFileName(NULL, interpreter_path, MAX_PATH);

    result = RegCreateKeyEx(HKEY_CLASSES_ROOT, ".ramr", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisposition);
    if (result == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "", 0, REG_SZ, (BYTE*)"RAMRFile", strlen("RAMRFile")+1);
        RegCloseKey(hKey);
    }
    result = RegCreateKeyEx(HKEY_CLASSES_ROOT, "RAMRFile", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisposition);
    if (result == ERROR_SUCCESS) {
        RegSetValueEx(hKey, "", 0, REG_SZ, (BYTE*)"RAMR Script File", strlen("RAMR Script File")+1);
        RegCloseKey(hKey);
    }
    result = RegCreateKeyEx(HKEY_CLASSES_ROOT, "RAMRFile\\shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisposition);
    if (result == ERROR_SUCCESS) {
        char command[MAX_PATH + 20];
        snprintf(command, sizeof(command), "\"%s\" \"%%1\"", interpreter_path);
        RegSetValueEx(hKey, "", 0, REG_SZ, (BYTE*)command, strlen(command)+1);
        RegCloseKey(hKey);
    }
    result = RegCreateKeyEx(HKEY_CLASSES_ROOT, "RAMRFile\\DefaultIcon", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisposition);
    if (result == ERROR_SUCCESS) {
        char icon_path[MAX_PATH + 10];
        snprintf(icon_path, sizeof(icon_path), "\"%s\",0", interpreter_path);
        RegSetValueEx(hKey, "", 0, REG_SZ, (BYTE*)icon_path, strlen(icon_path)+1);
        RegCloseKey(hKey);
    }
}

char **read_file_lines(const char *path, int *out_count) {
    FILE *f = fopen(path, "r");
    if (!f) { *out_count = 0; return NULL; }
    char **lines = NULL; int cap=0, cnt=0;
    char buf[MAX_LINE];
    while (fgets(buf, sizeof(buf), f)) {
        size_t L = strlen(buf);
        while (L>0 && (buf[L-1]=='\n' || buf[L-1]=='\r')) buf[--L] = '\0';
        if (cnt >= cap) { cap = cap ? cap*2 : 64; lines = xrealloc(lines, cap * sizeof(char*)); }
        lines[cnt++] = strdup_or_null(buf);
    }
    fclose(f);
    *out_count = cnt;
    return lines;
}

void exec_block(char **lines, int start, int end) {
    for (int i=start;i<end && running;i++) {
        exec_line(lines[i]);
    }
}

Function* find_function(const char *name) {
    for (int i=0;i<func_count;i++) if (strcmp(functions[i].name, name)==0) return &functions[i];
    return NULL;
}

void exec_function(const char *name) {
    Function *func = find_function(name);
    if (!func) {
        printf("[error] Функция '%s' не найдена\n", name);
        return;
    }
    for (int i=0;i<func->line_count && running;i++) exec_line(func->lines[i]);
}

void load_module(const char *module_name) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.ramr", module_name);
    int line_count=0;
    char **lines = read_file_lines(filename, &line_count);
    if (lines) {
        printf("[imp] Модуль '%s' загружен (%d строк)\n", module_name, line_count);
        exec_block(lines, 0, line_count);
        for (int i=0;i<line_count;i++) free(lines[i]);
        free(lines);
    } else {
        printf("[error] Модуль '%s' не найден\n", module_name);
    }
}

/* --- GUI: добавление текста/кнопок и запуск --- */
void gui_cleanup() {
    free(gui.title);
    for (int i=0;i<gui.text_count;i++) free(gui.texts[i].text);
    free(gui.texts);
    for (int i=0;i<gui.button_count;i++) {
        free(gui.buttons[i].text);
        free(gui.buttons[i].function);
    }
    free(gui.buttons);
    if (gui.hFont) DeleteObject(gui.hFont);
    memset(&gui, 0, sizeof(gui));
}

void gui_size_command(char *args) {
    char *p = args; char *size_str = next_token(&p);
    if (!size_str) return;
    char *x_str = strtok(size_str, "x"); char *y_str = strtok(NULL, "x");
    if (x_str && y_str) { gui.width = atoi(x_str); gui.height = atoi(y_str); printf("[gui] Размер окна: %dx%d\n", gui.width, gui.height); }
    free(size_str);
}

void gui_name_command(char *args) {
    char *p=args; char *name = next_token(&p);
    if (!name) return;
    free(gui.title); gui.title = strdup_or_null(name); printf("[gui] Заголовок: %s\n", gui.title);
    free(name);
}

void gui_settings_command(char *args) {
    char *p=args; char *color = next_token(&p);
    if (!color) return;
    gui.bg_color = get_color_from_name(color); printf("[gui] Цвет фона установлен\n");
    free(color);
}

void gui_text_command(char *args) {
    char *p=args; char *text = next_token(&p); char *pos = next_token(&p); char *color = next_token(&p); char *size_str = next_token(&p);
    if (!text || !pos) { free(text); free(pos); free(color); free(size_str); return; }
    int x,y;
    if (sscanf(pos, "%d,%d", &x,&y) == 2) {
        gui.texts = xrealloc(gui.texts, (gui.text_count+1)*sizeof(GuiText));
        GuiText *new_text = &gui.texts[gui.text_count];
        memset(new_text,0,sizeof(GuiText));
        new_text->text = strdup_or_null(text); new_text->x = x; new_text->y = y;
        new_text->width = 200; new_text->height = 20;
        new_text->color = get_color_from_name(color); new_text->font_size = size_str ? atoi(size_str) : 14;
        gui.text_count++;
        printf("[gui] Текст: '%s' в (%d,%d) размер: %d\n", text, x, y, new_text->font_size);
    }
    free(text); free(pos); free(color); free(size_str);
}

void gui_button_command(char *args) {
    char *p=args; char *text = next_token(&p); char *pos = next_token(&p); char *text_color = next_token(&p); char *bg_color = next_token(&p); char *func = next_token(&p);
    if (!text || !pos) { free(text); free(pos); free(text_color); free(bg_color); free(func); return; }
    int x,y;
    if (sscanf(pos,"%d,%d",&x,&y) == 2) {
        gui.buttons = xrealloc(gui.buttons, (gui.button_count+1)*sizeof(GuiButton));
        GuiButton *new_button = &gui.buttons[gui.button_count];
        memset(new_button,0,sizeof(GuiButton));
        new_button->text = strdup_or_null(text); new_button->x = x; new_button->y = y;
        new_button->width = 120; new_button->height = 30;
        new_button->function = func ? strdup_or_null(func) : strdup_or_null("");
        new_button->hwnd = NULL;
        gui.button_count++;
        printf("[gui] Кнопка: '%s' в (%d,%d)\n", text, x, y);
    }
    free(text); free(pos); free(text_color); free(bg_color); free(func);
}

void gui_update_command() {
    printf("[gui] Окно обновлено\n");
    printf("     Размер: %dx%d\n", gui.width, gui.height);
    printf("     Заголовок: %s\n", gui.title ? gui.title : "(нет)");
    printf("     Тексты: %d\n", gui.text_count);
    printf("     Кнопки: %d\n", gui.button_count);
}

/* --- Win32 window proc --- */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            for (int i=0;i<gui.button_count;i++) {
                GuiButton *btn = &gui.buttons[i];
                btn->hwnd = CreateWindow("BUTTON", btn->text, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                         btn->x, btn->y, btn->width, btn->height, hwnd, (HMENU)(1000 + i),
                                         ((LPCREATESTRUCT)lParam)->hInstance, NULL);
                if (gui.hFont) SendMessage(btn->hwnd, WM_SETFONT, (WPARAM)gui.hFont, TRUE);
            }
            break;
        }
        case WM_COMMAND: {
            int button_id = LOWORD(wParam) - 1000;
            if (button_id >= 0 && button_id < gui.button_count) {
                GuiButton *btn = &gui.buttons[button_id];
                printf("[gui] Клик по кнопке '%s', выполняется функция '%s'\n", btn->text, btn->function);
                if (strlen(btn->function) > 0) exec_function(btn->function);
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
            if (gui.hFont) SelectObject(hdc, gui.hFont);
            for (int i=0;i<gui.text_count;i++) {
                GuiText *text = &gui.texts[i];
                SetTextColor(hdc, text->color); SetBkMode(hdc, TRANSPARENT);
                HFONT hTextFont = CreateFont(text->font_size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
                if (hTextFont) {
                    SelectObject(hdc, hTextFont);
                    TextOut(hdc, text->x, text->y, text->text, (int)strlen(text->text));
                    SelectObject(hdc, GetStockObject(SYSTEM_FONT));
                    DeleteObject(hTextFont);
                } else {
                    TextOut(hdc, text->x, text->y, text->text, (int)strlen(text->text));
                }
            }
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_CLOSE: DestroyWindow(hwnd); break;
        case WM_DESTROY: gui.gui_running = 0; PostQuitMessage(0); break;
        default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

/* --- ti.* команды: sleep, timer, time --- */
void ti_sleep_command(char *args) {
    char *p=args; char *sec_str = next_token(&p);
    if (!sec_str) return;
    double seconds = atof(sec_str);
    if (seconds > 0.0) {
        printf("[sleep] Пауза %.3f секунд...\n", seconds);
        Sleep((DWORD)(seconds * 1000.0));
        printf("[sleep] Продолжаем\n");
    }
    free(sec_str);
}

void ti_timer_command(char *args) {
    char *p=args; char *sec_str = next_token(&p); char *func_name = next_token(&p);
    if (!sec_str || !func_name) { free(sec_str); free(func_name); return; }
    int seconds = atoi(sec_str);
    if (seconds > 0) {
        if (timer_count < MAX_TIMERS) {
            timers[timer_count].active = 1;
            timers[timer_count].seconds = seconds;
            timers[timer_count].start_time = time(NULL);
            timers[timer_count].function_name = strdup_or_null(func_name);
            printf("[timer] Таймер установлен: %d сек -> функция '%s'\n", seconds, func_name);
            timer_count++;
        } else {
            printf("[timer] Нельзя создать таймер — лимит %d\n", MAX_TIMERS);
        }
    }
    free(sec_str); free(func_name);
}

void ti_time_command(char *args) {
    char *p=args; char *condition = next_token(&p); char *func_name = next_token(&p);
    if (!condition || !func_name) { free(condition); free(func_name); return; }
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    int condition_met = 0;
    if (strcmp(condition, "now") == 0) condition_met = 1;
    else if (strncmp(condition, "sec=",4)==0) condition_met = (tm_info->tm_sec == atoi(condition+4));
    else if (strncmp(condition, "min=",4)==0) condition_met = (tm_info->tm_min == atoi(condition+4));
    else if (strncmp(condition, "hour=",5)==0) condition_met = (tm_info->tm_hour == atoi(condition+5));
    else {
        // поддержка формата HH:MM
        int hh=0, mm=0;
        if (sscanf(condition, "%d:%d", &hh, &mm) == 2) {
            condition_met = (tm_info->tm_hour == hh && tm_info->tm_min == mm);
        }
    }
    if (condition_met) {
        printf("[time] Условие времени выполнено: %s -> запуск '%s'\n", condition, func_name);
        exec_function(func_name);
    }
    free(condition); free(func_name);
}

/* Обновление таймеров: вызывать периодически */
void update_timers(void) {
    time_t now = time(NULL);
    for (int i=0;i<timer_count;i++) {
        if (timers[i].active) {
            double diff = difftime(now, timers[i].start_time);
            if (diff >= timers[i].seconds) {
                if (timers[i].function_name) {
                    printf("[timer] Сработал таймер: запуск '%s'\n", timers[i].function_name);
                    exec_function(timers[i].function_name);
                }
                timers[i].active = 0;
                free(timers[i].function_name);
                timers[i].function_name = NULL;
            }
        }
    }
}

/* --- exec_line: парсинг команд --- */
void exec_line(char *line) {
    if (!line) return;
    char *s = line;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return;
    if (*s == '@') return;

    if (skip_block) {
        if (strcmp(s, "or") == 0 && !if_condition_met) skip_block = 0;
        else if (strcmp(s, "end") == 0) { skip_block = 0; if_condition_met = 0; }
        return;
    }

    if (in_function_def) {
        if (strcmp(s, "end") == 0) {
            in_function_def = 0;
            if (func_count < MAX_FUNCTIONS) {
                functions[func_count].name = strdup_or_null(current_func_name);
                functions[func_count].lines = function_lines;
                functions[func_count].line_count = function_line_count;
                func_count++;
                printf("[def] Функция '%s' определена (%d строк)\n", current_func_name, function_line_count);
            } else {
                for (int i=0;i<function_line_count;i++) free(function_lines[i]);
                free(function_lines);
            }
            function_lines = NULL;
            function_line_count = 0;
            current_func_name[0] = '\0';
        } else {
            if (function_line_count < MAX_FUNC_LINES) {
                function_lines = xrealloc(function_lines, (function_line_count+1)*sizeof(char*));
                function_lines[function_line_count++] = strdup_or_null(line);
            }
        }
        return;
    }

    char *p = s;
    char *cmd = next_token(&p);
    if (!cmd) return;

    /* print.N */
    if (strncmp(cmd, "print.", 6) == 0) {
        char *arg = next_token(&p); if (!arg) arg = strdup_or_null("");
        if (strcmp(cmd, "print.Infinity") == 0) {
            while (running) {
                char *out = substitute_vars(arg);
                printf("%s\n", out);
                free(out); fflush(stdout);
                Sleep(1000);
                if (_kbhit()) { int ch = getchar(); if (ch == 'q' || ch == 'Q') break; }
            }
            free(arg); free(cmd); return;
        } else {
            int n = atoi(cmd + 6);
            for (int i=0;i<n;i++) {
                char *out = substitute_vars(arg);
                printf("%s\n", out);
                free(out);
            }
            free(arg); free(cmd); return;
        }
    }

    if (strcmp(cmd, "print") == 0) {
        char *arg = next_token(&p); if (!arg) arg = strdup_or_null("");
        char *out = substitute_vars(arg);
        printf("%s\n", out);
        free(out); free(arg); free(cmd); return;
    }

    if (strcmp(cmd, "rprint") == 0) {
        char *arg = next_token(&p); if (!arg) arg = strdup_or_null("");
        char *out = substitute_vars(arg);
        printf("%s\n", out);
        free(out); free(arg); free(cmd); return;
    }

    if (strcmp(cmd, "type") == 0) {
        char *prompt = next_token(&p); if (!prompt) prompt = strdup_or_null("");
        char *out_prompt = substitute_vars(prompt);
        printf("%s", out_prompt); fflush(stdout);
        char buf[MAX_LINE]; if (!fgets(buf, sizeof(buf), stdin)) buf[0]='\0';
        size_t L = strlen(buf); while (L>0 && (buf[L-1]=='\n' || buf[L-1]=='\r')) buf[--L]='\0';
        set_var("last_input", buf);
        free(prompt); free(out_prompt); free(cmd); return;
    }

    if (strcmp(cmd, "save") == 0) {
        char *name = next_token(&p);
        char *value = next_token(&p);
        if (!name) { free(cmd); return; }
        if (!value) value = strdup_or_null(get_var("last_input"));
        else { char *subst_value = substitute_vars(value); free(value); value = subst_value; }
        set_var(name, value);
        free(name); free(value); free(cmd); return;
    }

    if (strcmp(cmd, "imp") == 0) {
        char *mod = next_token(&p);
        if (mod) load_module(mod);
        free(mod); free(cmd); return;
    }

    if (strcmp(cmd, "start") == 0) {
        char *mod = next_token(&p);
        if (mod) exec_function(mod);
        free(mod); free(cmd); return;
    }

    if (strcmp(cmd, "off") == 0) { char *mod = next_token(&p); free(mod); free(cmd); return; }

    if (strcmp(cmd, "query") == 0) { char *mod = next_token(&p); char *q = next_token(&p); free(mod); free(q); free(cmd); return; }

    if (strcmp(cmd, "wait()")==0 || strcmp(cmd, "wait")==0) {
        printf("Нажмите Enter чтобы продолжить...");
        fflush(stdout);
        char buf[16]; fgets(buf, sizeof(buf), stdin);
        free(cmd); return;
    }

    if (strcmp(cmd, "exit")==0) { running = 0; free(cmd); return; }
    if (strcmp(cmd, "break")==0) { running = 0; free(cmd); return; }

    if (strcmp(cmd, "and")==0) {
        char *next_cmd;
        while ((next_cmd = next_token(&p)) != NULL) {
            exec_line(next_cmd); free(next_cmd);
        }
        free(cmd); return;
    }

    if (strcmp(cmd, "def")==0) {
        char *name = next_token(&p);
        if (!name) { free(cmd); return; }
        in_function_def = 1;
        strncpy(current_func_name, name, sizeof(current_func_name)-1);
        function_lines = NULL; function_line_count = 0;
        free(name); free(cmd); return;
    }

    if (strcmp(cmd, "random.choice")==0) {
        char *items[128]; int ni=0; char *t;
        while ((t = next_token(&p)) != NULL) { items[ni++] = t; if (ni>=128) break; }
        char *res = random_choice(items, ni);
        printf("%s\n", res); free(res);
        for (int i=0;i<ni;i++) free(items[i]);
        free(cmd); return;
    }

    if (strcmp(cmd, "random.text")==0) {
        char *len_s = next_token(&p); int len = len_s ? atoi(len_s) : 8;
        char *res = random_text(len); printf("%s\n", res); free(res); free(len_s); free(cmd); return;
    }

    if (strcmp(cmd, "random.digits")==0) {
        char *len_s = next_token(&p); int len = len_s ? atoi(len_s) : 6;
        char *res = random_digits(len); printf("%s\n", res); free(res); free(len_s); free(cmd); return;
    }

    if (strcmp(cmd, "if")==0) {
        char *varname = next_token(&p); char *op = next_token(&p); char *val = next_token(&p);
        int cond = 0; const char *left = get_var(varname);
        if (op && strcmp(op,"==")==0) cond = (strcmp(left, val ? val : "") == 0);
        if (!cond) { skip_block = 1; if_condition_met = 0; } else if_condition_met = 1;
        free(varname); free(op); free(val); free(cmd); return;
    }

    if (strcmp(cmd, "or")==0) { if (skip_block && !if_condition_met) skip_block = 0; free(cmd); return; }
    if (strcmp(cmd, "end")==0) { skip_block = 0; if_condition_met = 0; free(cmd); return; }

    if (strcmp(cmd, "gui.size")==0) { gui_size_command(p); free(cmd); return; }
    if (strcmp(cmd, "gui.name")==0) { gui_name_command(p); free(cmd); return; }
    if (strcmp(cmd, "gui.settings")==0) { gui_settings_command(p); free(cmd); return; }
    if (strcmp(cmd, "gui.text")==0) { gui_text_command(p); free(cmd); return; }
    if (strcmp(cmd, "gui.button")==0) { gui_button_command(p); free(cmd); return; }
    if (strcmp(cmd, "gui.update")==0) { gui_update_command(); free(cmd); return; }
    if (strcmp(cmd, "gui.run")==0) { gui_run_command(); free(cmd); return; }

    /* ti.* */
    if (strcmp(cmd, "ti.sleep")==0) { ti_sleep_command(p); free(cmd); return; }
    if (strcmp(cmd, "ti.timer")==0) { ti_timer_command(p); free(cmd); return; }
    if (strcmp(cmd, "ti.time")==0) { ti_time_command(p); free(cmd); return; }

    /* иначе — вызов функции с именем cmd */
    exec_function(cmd);
    free(cmd);
}

/* --- GUI loop с обновлением таймеров --- */
void gui_run_command(void) {
    printf("[gui] Запуск графического интерфейса Win32\n");
    gui.hInstance = GetModuleHandle(NULL);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = gui.hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(gui.bg_color);
    wc.lpszClassName = "RAMRWindow";

    if (!RegisterClassEx(&wc)) { printf("[gui error] Не удалось зарегистрировать класс окна\n"); return; }

    gui.hFont = CreateFont(14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");

    int width = gui.width ? gui.width : 800;
    int height = gui.height ? gui.height : 600;
    const char *title = gui.title ? gui.title : "RAMR GUI";

    gui.hwnd = CreateWindowEx(0, "RAMRWindow", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, gui.hInstance, NULL);
    if (!gui.hwnd) { printf("[gui error] Не удалось создать окно\n"); return; }

    ShowWindow(gui.hwnd, SW_SHOW);
    UpdateWindow(gui.hwnd);
    gui.gui_running = 1;

    MSG msg;
    while (gui.gui_running && running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { gui.gui_running = 0; break; }
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        update_timers();     // важное: таймеры обновляются и в GUI-цикле
        Sleep(10);
    }
    printf("[gui] Графический интерфейс закрыт\n");
}

/* --- main --- */
int main(int argc, char **argv) {
    srand((unsigned)time(NULL));
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    if (argc >= 2) {
        if (strcmp(argv[1], "--register-associations")==0 || strcmp(argv[1], "-reg")==0) {
            setup_file_associations();
            printf("File associations registered successfully.\n");
            return 0;
        }
    }

    char *script = NULL;
    if (argc >= 2) script = argv[1];

    if (script) {
        int line_count = 0;
        char **lines = read_file_lines(script, &line_count);
        if (!lines) { printf("Ошибка: не удалось открыть файл '%s'\n", script); return 1; }
        for (int i=0;i<line_count && running;i++) {
            exec_line(lines[i]);
            update_timers(); // обновляем таймеры между строками скрипта
            // небольшая пауза чтобы таймеры могли "идти" при длинных скриптах
            Sleep(1);
        }
        for (int i=0;i<line_count;i++) free(lines[i]);
        free(lines);
        gui_cleanup();
        return 0;
    }

    printf("RAMR interpreter with Win32 GUI. Type 'exit' to quit.\n");
    printf("Use '--register-associations' to register .ramr file associations.\n");

    char buf[MAX_LINE];
    while (running) {
        printf("> "); fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        size_t L = strlen(buf); while (L>0 && (buf[L-1]=='\n' || buf[L-1]=='\r')) buf[--L] = '\0';
        exec_line(buf);
        update_timers();   // проверяем таймеры после каждой команды
        Sleep(1);
    }

    /* очистка памяти */
    for (int i=0;i<var_count;i++) { free(vars[i].key); free(vars[i].value); }
    for (int i=0;i<func_count;i++) {
        free(functions[i].name);
        for (int j=0;j<functions[i].line_count;j++) free(functions[i].lines[j]);
        free(functions[i].lines);
    }
    for (int i=0;i<timer_count;i++) {
        if (timers[i].function_name) free(timers[i].function_name);
    }
    gui_cleanup();
    return 0;
}
