/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/*** defines ***/
#define JATE_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)


/*** data ***/
struct editorConfig {
    int screenrows, screencols;
    struct termios orig_termios;
};

struct editorConfig E;


/*** terminal ***/
void die(const char * s) {
    // Clear the screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // Reposition cursor to start of the line
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}


void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}


void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}


char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    return c;
}


// Fallback method for getting screen size
int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    return 0;
}


int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}


/*** append buffer ***/

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}


void abAppend(struct abuf *ab, const char *s, int len) {
    // When writing to buffer, first reallocate
    // the character array to a larger length
    char *new = realloc(ab->b, ab->len+len);

    // realloc() returns a pointer to the start of the
    // new array on success and a null pointer on failure
    if (new == NULL)
        return;
    // then copy the new chars to the end of the buffer
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}


void abFree(struct abuf *ab) {
    free(ab->b);
}


/*** output ***/
void editorDrawRows(struct abuf *ab) {
    int y;
    for (y=0; y<E.screenrows; ++y) {
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                           "Jate editor -- version %s", JATE_VERSION);
            if (welcomelen > E.screencols)
                welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--)
                abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
        }

        // Redraw line by line
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}


void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    // Hides the cursor before refreshing
    abAppend(&ab, "\x1b[?25l", 6);
    // Reposition cursor to start of the line
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    // Reposition cursor to start of the line
    abAppend(&ab, "\x1b[H", 3);
    // Make the cursor reappear
    abAppend(&ab, "\x1b[?25h", 6);

    // Draw to screen
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/*** input ***/
void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1 )
        die("getWindowSize");
}


void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            // Clear the screen
            write(STDOUT_FILENO, "\x0b[2J", 4);
            // Reposition cursor to start of the line
            write(STDOUT_FILENO, "\x0b[H", 3);
            exit(0);
            break;
    }
}


/*** init ***/
int main() {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}