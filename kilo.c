/*** Includes ***/

#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*** Defines ***/

/**
 * KILO_VERSION macro
 *
 * Defines the current version of the Kilo text editor as a string literal.
 * This macro is used throughout the program to display or reference the
 * editor's version number.
 */
#define KILO_VERSION "0.0.1"

/**
 * CTRL_KEY macro
 *
 * Converts a character to its corresponding control key code.
 * For example, CTRL_KEY('q') produces the code for Ctrl-Q.
 *
 * @param k The character to convert.
 * @return The control key code for the given character.
 */
#define CTRL_KEY(k) ((k) & 0x1f)

/**
 * enum editorKey
 *
 * Defines special key codes for handling non-character keys in the editor.
 * These values are set to be greater than 1000 to avoid conflicts with
 * regular character input. The enum includes codes for the arrow keys:
 *   - ARROW_LEFT:  Left arrow key
 *   - ARROW_RIGHT: Right arrow key
 *   - ARROW_UP:    Up arrow key
 *   - ARROW_DOWN:  Down arrow key
 */
enum editorKey
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
};

/*** Data ***/

/**
 * struct editorConfig
 *
 * Holds the current state and configuration of the editor.
 *
 * Members:
 *   cx, cy        - Current cursor x and y position within the editor window.
 *   screenrows    - Number of rows visible in the editor window.
 *   screencols    - Number of columns visible in the editor window.
 *   orig_termios  - Original terminal settings, used to restore terminal state on exit.
 */
struct editorConfig
{
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** Terminal ***/

/**
 * Prints an error message and exits the program.
 *
 * This function clears the screen, moves the cursor to the top-left,
 * prints the error message corresponding to the provided string using perror,
 * and then exits the program with a status code of 1.
 *
 * @param s The error message to display.
 */
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

/**
 * Restores the terminal to its original mode.
 *
 * This function disables raw mode by restoring the terminal attributes
 * saved in E.orig_termios. It is typically called on program exit to
 * ensure the terminal behaves normally after the editor exits.
 */
void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

/**
 * Enables raw mode for the terminal.
 *
 * This function configures the terminal to operate in raw mode, disabling
 * canonical input, echoing, and various input/output processing features.
 * It saves the original terminal attributes in E.orig_termios and registers
 * disableRawMode() to be called automatically on program exit to restore
 * the terminal state.
 *
 * If an error occurs while getting or setting terminal attributes, the
 * function prints an error message and exits the program.
 */
void enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

/**
 * Reads a single keypress from standard input, including arrow keys.
 *
 * This function waits for a single character input from the user,
 * handling read errors appropriately. If an escape sequence is detected,
 * it interprets arrow key escape codes and returns the corresponding
 * editorKey enum value (ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT).
 * Otherwise, it returns the character read from standard input.
 *
 * @return The character read, or a special key code for arrow keys.
 */
int editorReadKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    if (c == '\x1b')
    {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        if (seq[0] == '[')
        {
            switch (seq[1])
            {
            case 'A':
                return ARROW_UP;
            case 'B':
                return ARROW_DOWN;
            case 'C':
                return ARROW_RIGHT;
            case 'D':
                return ARROW_LEFT;
            }
        }
        return '\x1b';
    }
    else
    {
        return c;
    }
}

/**
 * Retrieves the current cursor position from the terminal.
 *
 * This function sends the escape sequence "\x1b[6n" to query the terminal for the
 * current cursor position. The terminal responds with a sequence of the form
 * ESC [ rows ; cols R. The function reads this response, parses the row and column
 * values, and stores them in the provided pointers.
 *
 * @param rows Pointer to an integer where the row number will be stored.
 * @param cols Pointer to an integer where the column number will be stored.
 * @return 0 on success, -1 on failure.
 */
int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1)
    {
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

/**
 * Gets the size of the terminal window.
 *
 * This function attempts to retrieve the number of rows and columns of the terminal
 * window using the ioctl system call with TIOCGWINSZ. If successful, it stores the
 * values in the provided pointers and returns 0. If the ioctl call fails or returns
 * invalid data, it falls back to using an escape sequence to move the cursor to the
 * bottom-right corner of the terminal and then queries the cursor position to
 * determine the window size. If both methods fail, the function returns -1.
 *
 * @param rows Pointer to an integer where the number of rows will be stored.
 * @param cols Pointer to an integer where the number of columns will be stored.
 * @return 0 on success, -1 on failure.
 */
int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPosition(rows, cols);
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** Append buffer ***/

/**
 * struct abuf
 *
 * A dynamic append buffer used for efficiently building output strings.
 *
 * Members:
 *   b   - Pointer to the buffer's character data.
 *   len - Current length of the buffer (number of bytes used).
 */
struct abuf
{
    char *b;
    int len;
};

/**
 * ABUF_INIT macro
 *
 * Initializes an abuf structure with a NULL buffer and zero length.
 * Useful for creating an empty append buffer.
 */
#define ABUF_INIT {NULL, 0}

/**
 * Appends a string to the append buffer.
 *
 * This function reallocates the buffer in the abuf structure to accommodate
 * the new data, copies the specified string into the buffer, and updates
 * the buffer's length. If memory allocation fails, the function returns
 * without modifying the buffer.
 *
 * @param ab  Pointer to the append buffer structure.
 * @param s   Pointer to the string to append.
 * @param len Length of the string to append.
 */
void abAppend(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

/**
 * Frees the memory used by the append buffer.
 *
 * This function releases the memory allocated for the buffer's character data
 * in the abuf structure. It should be called when the append buffer is no longer
 * needed to avoid memory leaks.
 *
 * @param ab Pointer to the append buffer structure to free.
 */
void abFree(struct abuf *ab)
{
    free(ab->b);
}

/*** Output ***/

/**
 * Draws the rows of the editor into an append buffer.
 *
 * For each row in the editor window, this function appends a tilde (~) at the start of the line.
 * On the row one-third down the screen, it centers and displays a welcome message with the editor version.
 * All lines are cleared from the cursor to the end using an escape sequence.
 * For all but the last row, a carriage return and newline ("\r\n") are appended.
 * The number of rows drawn matches the current height of the editor window (E.screenrows).
 *
 * @param ab Pointer to the append buffer where the rows will be appended.
 */
void editorDrawRows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        if (y == E.screenrows / 3)
        {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                                      "Kilo editor -- version %s", KILO_VERSION);
            if (welcomelen > E.screencols)
                welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if (padding)
            {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--)
                abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        }
        else
        {
            abAppend(ab, "~", 1);
        }
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1)
        {
            abAppend(ab, "\r\n", 2);
        }
    }
}

/**
 * Refreshes the editor screen.
 *
 * This function creates an append buffer to batch terminal output, hides the cursor,
 * moves the cursor to the top-left, draws the editor rows (with tildes and a welcome message),
 * repositions the cursor to the current editor coordinates, and then shows the cursor again.
 * The entire buffer is written to the terminal at once to reduce flicker and improve performance.
 *
 * The cursor is positioned according to the editor's current state (E.cx, E.cy).
 * All output is freed after being written.
 */
void editorRefreshScreen()
{
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);
    editorDrawRows(&ab);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** Input ***/

/**
 * Moves the cursor in the editor based on the input key.
 *
 * This function updates the cursor position (E.cx, E.cy) according to the
 * provided key, which should be one of the arrow key codes (ARROW_UP, ARROW_DOWN,
 * ARROW_LEFT, ARROW_RIGHT) for up, down, left, and right movement, respectively.
 * The function performs boundary checks to prevent the cursor from moving outside
 * the visible window.
 *
 * @param key The key code representing the direction to move the cursor:
 *            ARROW_UP, ARROW_DOWN, ARROW_LEFT, or ARROW_RIGHT.
 */
void editorMoveCursor(int key)
{
    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
        {
            E.cx--;
        }
        break;
    case ARROW_RIGHT:
        if (E.cx != E.screencols - 1)
        {
            E.cx++;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0)
        {
            E.cy--;
        }
        break;
    case ARROW_DOWN:
        if (E.cy != E.screenrows - 1)
        {
            E.cy++;
        }
        break;
    }
}

/**
 * Processes a single keypress from the user.
 *
 * This function reads a keypress and performs the corresponding action.
 * - If Ctrl-Q is pressed, it clears the screen and exits the program.
 * - If an arrow key is pressed, it moves the cursor in the corresponding direction
 *   (up, down, left, or right) by calling editorMoveCursor().
 * Other keys are ignored.
 */
void editorProcessKeypress()
{
    int c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    }
}

/*** Init ***/

/**
 * Initializes the editor configuration.
 *
 * Sets the initial cursor position to the top-left corner (0,0).
 * Retrieves the current terminal window size and stores the number of rows and columns
 * in the global editor configuration struct E. If retrieving the window size fails,
 * the function prints an error message and exits the program.
 */
void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main()
{
    enableRawMode();
    initEditor();
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}