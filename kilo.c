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
 * CTRL_KEY macro
 *
 * Converts a character to its corresponding control key code.
 * For example, CTRL_KEY('q') produces the code for Ctrl-Q.
 *
 * @param k The character to convert.
 * @return The control key code for the given character.
 */
#define CTRL_KEY(k) ((k) & 0x1f)

/*** Data ***/

/**
 * struct editorConfig
 *
 * Stores the editor's configuration and state.
 *
 * Members:
 *   screenrows    - Number of rows in the editor window.
 *   screencols    - Number of columns in the editor window.
 *   orig_termios  - The original terminal attributes before enabling raw mode.
 *                   Used to restore the terminal state when the editor exits.
 */
struct editorConfig
{
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
 * Reads a single keypress from standard input.
 *
 * This function waits for a single character input from the user,
 * handling read errors appropriately. It is used to capture raw
 * keyboard input in the editor, including control characters.
 *
 * @return The character read from standard input.
 */
char editorReadKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    return c;
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
 * For each row in the editor window, this function appends a tilde (~) at the start of the line,
 * followed by an escape sequence to clear the line from the cursor to the end. For all but the
 * last row, it also appends a carriage return and newline ("\r\n"). The number of rows drawn
 * matches the current height of the editor window (E.screenrows). This is a placeholder for
 * where file contents will eventually be displayed.
 *
 * @param ab Pointer to the append buffer where the rows will be appended.
 */
void editorDrawRows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        abAppend(ab, "~", 1);
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
 * This function constructs an output buffer, hides the cursor, moves the cursor to the
 * top-left corner, draws the editor rows (currently displaying tildes as placeholders),
 * then repositions the cursor to the top-left and shows the cursor again. The entire
 * buffer is written to the terminal in a single write operation for performance.
 *
 * The function uses an append buffer to minimize flicker and reduce the number of
 * system calls by batching all output before displaying it.
 */
void editorRefreshScreen()
{
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);
    editorDrawRows(&ab);
    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** Input ***/

/**
 * Processes a single keypress from the user.
 *
 * This function reads a keypress and performs the corresponding action.
 * Currently, it handles quitting the editor when Ctrl-Q is pressed by
 * clearing the screen and exiting the program.
 */
void editorProcessKeypress()
{
    char c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/*** Init ***/

/**
 * Initializes the editor configuration.
 *
 * This function retrieves the size of the terminal window and stores
 * the number of rows and columns in the global editor configuration struct E.
 * If retrieving the window size fails, the function prints an error message
 * and exits the program.
 */
void initEditor()
{
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