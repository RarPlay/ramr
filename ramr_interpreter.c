// ramr_interpreter.c // Простой интерпретатор RAMR на C — версия минимального рабочего прототипа. // Поддерживает базовые команды: print, rprint, type, save, if/or/end, // print.<n>, print.Infinity, wait(), random.choice/text/digits, imp/start/off/query, // def (заготовка), exit, break, and (последовательный блок), @ комментарии. // // Как компилировать: // gcc -std=c11 -O2 -o ramr_interpreter ramr_interpreter.c // Как запускать: // ./ramr_interpreter            (в интерактивном режиме) // ./ramr_interpreter script.ramr (выполнить файл)

#include <stdio.h> #include <stdlib.h> #include <string.h> #include <time.h> #include <ctype.h> #include <unistd.h> #include <sys/select.h>

#define MAX_LINE 1024 #define MAX_VARS 1024

typedef struct { char *key; char *value; } Var;

static Var vars[MAX_VARS]; static int var_count = 0; static int running = 1;

// Утилиты работы с памятью char *strdup_or_null(const char *s) { if (!s) return NULL; char *r = strdup(s); return r; }

void set_var(const char *key, const char *value) { for (int i = 0; i < var_count; ++i) { if (strcmp(vars[i].key, key) == 0) { free(vars[i].value); vars[i].value = strdup_or_null(value); return; } } if (var_count < MAX_VARS) { vars[var_count].key = strdup_or_null(key); vars[var_count].value = strdup_or_null(value); var_count++; } }

const char *get_var(const char *key) { for (int i = 0; i < var_count; ++i) { if (strcmp(vars[i].key, key) == 0) return vars[i].value; } return ""; }

// Простейший разбор строки: извлечь токен или строковый литерал в кавычках char *next_token(char **line_ptr) { char *s = *line_ptr; if (!s) return NULL; while (*s && isspace((unsigned char)*s)) s++; if (*s == '\0') return NULL; char buf[MAX_LINE]; buf[0] = '\0'; if (*s == '"') { s++; char *d = buf; while (*s && *s != '"') { *d++ = *s++; } *d = '\0'; if (*s == '"') s++; *line_ptr = s; return strdup_or_null(buf); } else { char *d = buf; while (*s && !isspace((unsigned char)*s)) { *d++ = *s++; } *d = '\0'; *line_ptr = s; return strdup_or_null(buf); } }

// Заменить {var} в строке на значение char *substitute_vars(const char *input) { char out[MAX_LINE * 2]; out[0] = '\0'; const char *p = input; while (*p) { if (*p == '{') { p++; char name[256]; int ni = 0; while (*p && *p != '}' && ni < 255) name[ni++] = *p++; name[ni] = '\0'; if (*p == '}') p++; const char *val = get_var(name); strncat(out, val, sizeof(out)-strlen(out)-1); } else { char tmp[2] = {*p, '\0'}; strncat(out, tmp, sizeof(out)-strlen(out)-1); p++; } } return strdup_or_null(out); }

// Random helpers char *random_choice(char **items, int n) { if (n <= 0) return strdup_or_null(""); int idx = rand() % n; return strdup_or_null(items[idx]); }

char *random_text(int length) { if (length <= 0) length = 8; char *s = malloc(length + 1); for (int i = 0; i < length; ++i) s[i] = 'a' + (rand() % 26); s[length] = '\0'; return s; }

char *random_digits(int length) { if (length <= 0) length = 6; char *s = malloc(length + 1); for (int i = 0; i < length; ++i) s[i] = '0' + (rand() % 10); s[length] = '\0'; return s; }

// Проверка наличия ввода в stdin (неблокирующая) int stdin_has_data(void) { struct timeval tv = {0, 0}; fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds); return select(STDIN_FILENO+1, &fds, NULL, NULL, &tv) > 0; }

// Выполнение одной строки RAMR void exec_line(char *line);

// Выполнить блок (например, внутри if/or/and) void exec_block(char **lines, int start, int end) { for (int i = start; i < end && running; ++i) { exec_line(lines[i]); } }

// Чтение файла в массив строк (возвращает количество строк) char **read_file_lines(const char *path, int *out_count) { FILE f = fopen(path, "r"); if (!f) { out_count = 0; return NULL; } char **lines = NULL; int cap = 0, cnt = 0; char buf[MAX_LINE]; while (fgets(buf, sizeof(buf), f)) { size_t L = strlen(buf); while (L > 0 && (buf[L-1] == '\n' || buf[L-1] == '\r')) { buf[--L] = '\0'; } if (cnt >= cap) { cap = cap ? cap2 : 64; lines = realloc(lines, cap * sizeof(char)); } lines[cnt++] = strdup_or_null(buf); } fclose(f); *out_count = cnt; return lines; }

// Главная логика выполнения строки void exec_line(char *line) { if (!line) return; // Пропускаем комментарии и пустые строки char *s = line; while (*s && isspace((unsigned char)*s)) s++; if (*s == '\0') return; if (*s == '@') return; // комментарий

// Стабильно обнаруживаем конструкции типа print.5 "text" и print.Infinity
char *p = s;
char *cmd = next_token(&p);
if (!cmd) return;

// Обработка print.<n> и print.Infinity
if (strncmp(cmd, "print.", 6) == 0) {
    char *arg = next_token(&p);
    if (!arg) arg = strdup_or_null("");
    if (strcmp(cmd, "print.Infinity") == 0) {
        // бесконечный вывод: печатаем и следим за stdin для остановки
        while (running) {
            char *out = substitute_vars(arg);
            printf("%s\n", out);
            free(out);
            fflush(stdout);
            sleep(1);
            if (stdin_has_data()) {
                // читаем строку и если она содержит "break" - выходим
                char buf[256]; if (fgets(buf, sizeof(buf), stdin)) {
                    if (strstr(buf, "break") != NULL) break;
                }
            }
        }
        free(arg); free(cmd);
        return;
    } else {
        // parse number after dot
        int n = atoi(cmd + 6);
        for (int i = 0; i < n; ++i) {
            char *out = substitute_vars(arg);
            printf("%s\n", out);
            free(out);
        }
        free(arg); free(cmd);
        return;
    }
}

if (strcmp(cmd, "print") == 0) {
    char *arg = next_token(&p);
    if (!arg) arg = strdup_or_null("");
    char *out = substitute_vars(arg);
    printf("%s\n", out);
    free(out); free(arg); free(cmd);
    return;
}

if (strcmp(cmd, "rprint") == 0) {
    char *arg = next_token(&p);
    if (!arg) arg = strdup_or_null("");
    // rprint делает подстановку {var}
    char *out = substitute_vars(arg);
    printf("%s\n", out);
    free(out); free(arg); free(cmd);
    return;
}

if (strcmp(cmd, "type") == 0) {
    char *prompt = next_token(&p);
    if (!prompt) prompt = strdup_or_null("");
    printf("%s", prompt);
    fflush(stdout);
    char buf[MAX_LINE];
    if (!fgets(buf, sizeof(buf), stdin)) buf[0] = '\0';
    size_t L = strlen(buf); while (L>0 && (buf[L-1]=='\n' || buf[L-1]=='\r')) buf[--L] = '\0';
    // Первая не должна автоматически сохраняться — чтобы следовать синтаксису нужно save
    // Но для удобства вернём в переменную last_input
    set_var("last_input", buf);
    free(prompt); free(cmd);
    return;
}

if (strcmp(cmd, "save") == 0) {
    char *name = next_token(&p);
    char *rest = next_token(&p);
    if (!name) { free(cmd); return; }
    if (!rest) rest = strdup_or_null(get_var("last_input"));
    set_var(name, rest);
    free(name); free(rest); free(cmd);
    return;
}

if (strcmp(cmd, "imp") == 0) {
    char *mod = next_token(&p);
    printf("[imp] Модуль '%s' (заглушка)\n", mod ? mod : "(none)");
    free(mod); free(cmd);
    return;
}
if (strcmp(cmd, "start") == 0) {
    char *mod = next_token(&p);
    printf("[start] Запуск модуля '%s' (заглушка)\n", mod ? mod : "(none)");
    free(mod); free(cmd);
    return;
}
if (strcmp(cmd, "off") == 0) {
    char *mod = next_token(&p);
    printf("[off] Отключение модуля '%s' (заглушка)\n", mod ? mod : "(none)");
    free(mod); free(cmd);
    return;
}

if (strcmp(cmd, "query") == 0) {
    char *mod = next_token(&p);
    char *q = next_token(&p);
    printf("[query] Запрос к '%s': %s (заглушка)\n", mod ? mod : "(none)", q ? q : "");
    free(mod); free(q); free(cmd);
    return;
}

if (strcmp(cmd, "wait()") == 0 || strcmp(cmd, "wait") == 0) {
    printf("[wait] Нажмите Enter чтобы продолжить..."); fflush(stdout);
    char buf[16]; fgets(buf, sizeof(buf), stdin);
    free(cmd);
    return;
}

if (strcmp(cmd, "exit") == 0) {
    running = 0; free(cmd); return;
}

if (strcmp(cmd, "break") == 0) {
    // используется внутри блоков: чтобы упростить — переведём в exit для непродвинутой версии
    running = 0; free(cmd); return;
}

if (strcmp(cmd, "and") == 0) {
    // читаем блок до end и выполняем последовательно
    // здесь упрощенно: далее строки уже должны быть прочитаны внешним циклом
    free(cmd); return;
}

if (strcmp(cmd, "def") == 0) {
    // заготовка определения функции — в этой версии сохраняем имя и не выполняем
    char *name = next_token(&p);
    printf("[def] Определена функция '%s' (заглушка)\n", name ? name : "(none)");
    free(name); free(cmd);
    return;
}

if (strcmp(cmd, "random.choice") == 0) {
    // соберём оставшиеся токены как варианты
    char *items[128]; int ni = 0;
    char *t;
    while ((t = next_token(&p)) != NULL) {
        items[ni++] = t;
        if (ni >= 128) break;
    }
    char *res = random_choice(items, ni);
    printf("%s\n", res);
    free(res);
    for (int i = 0; i < ni; ++i) free(items[i]);
    free(cmd);
    return;
}

if (strcmp(cmd, "random.text") == 0) {
    char *len_s = next_token(&p);
    int len = len_s ? atoi(len_s) : 8;
    char *res = random_text(len);
    printf("%s\n", res);
    free(res); free(len_s); free(cmd);
    return;
}

if (strcmp(cmd, "random.digits") == 0) {
    char *len_s = next_token(&p);
    int len = len_s ? atoi(len_s) : 6;
    char *res = random_digits(len);
    printf("%s\n", res);
    free(res); free(len_s); free(cmd);
    return;
}

if (strcmp(cmd, "if") == 0) {
    // формат: if <var> == "value"
    char *varname = next_token(&p);
    char *op = next_token(&p);
    char *val = next_token(&p);
    int cond = 0;
    const char *left = get_var(varname);
    if (op && strcmp(op, "==") == 0) {
        cond = (strcmp(left, val ? val : "") == 0);
    }
    // Если условие ложно, то нужно пропустить блок до 'or' или 'end'
    // В этой простейшей реализации мы не можем просматривать внешние строки,
    // поэтому мы читаем следующие строки из stdin, исполняя или пропуская их,
    // пока не дойдём до 'or' или 'end'.
    char buf[MAX_LINE];
    while (fgets(buf, sizeof(buf), stdin)) {
        size_t L = strlen(buf); while (L>0 && (buf[L-1]=='\n' || buf[L-1]=='\r')) buf[--L] = '\0';
        char tmp[MAX_LINE]; strncpy(tmp, buf, sizeof(tmp));
        char *tp = tmp;
        char *tk = next_token(&tp);
        if (tk && strcmp(tk, "or") == 0) { free(tk); break; }
        if (tk && strcmp(tk, "end") == 0) { free(tk); break; }
        free(tk);
        if (cond) {
            exec_line(buf);
        }
    }
    free(varname); free(op); free(val); free(cmd);
    return;
}

// Если не распознано — выводим как неизвестную команду
printf("[unknown] %s\n", cmd);
free(cmd);

}

int main(int argc, char **argv) { srand((unsigned)time(NULL)); char *script = NULL; if (argc >= 2) script = argv[1];

char **lines = NULL; int line_count = 0;
if (script) {
    lines = read_file_lines(script, &line_count);
    for (int i = 0; i < line_count && running; ++i) {
        exec_line(lines[i]);
    }
    // очистка
    for (int i = 0; i < line_count; ++i) free(lines[i]); free(lines);
    return 0;
}

printf("RAMR interpreter (C prototype). Type 'exit' to quit.\n");
char buf[MAX_LINE];
while (running) {
    printf("> "); fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) break;
    size_t L = strlen(buf); while (L>0 && (buf[L-1]=='\n' || buf[L-1]=='\r')) buf[--L] = '\0';
    exec_line(buf);
}

// cleanup vars
for (int i = 0; i < var_count; ++i) {
    free(vars[i].key); free(vars[i].value);
}
return 0;

}

