/*** Includes ***/

/*
 * These macros enable various feature test macros for the C library:
 *
 * _DEFAULT_SOURCE: Enables default features for the GNU C Library (glibc),
 *                  providing access to most POSIX and GNU extensions.
 * _BSD_SOURCE:     Provides compatibility with BSD-specific functions and
 *                  definitions. Deprecated in favor of _DEFAULT_SOURCE, but
 *                  still widely used for legacy code.
 * _GNU_SOURCE:     Enables all GNU extensions as well as POSIX and BSD features.
 *
 * Defining these macros at the top of the file ensures that the program can use
 * a wide range of system and library features beyond the standard C library.
 */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
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
 * Enumerates special key codes used by the editor to represent non-printable keys.
 * The values start at 1000 to avoid collision with standard ASCII characters.
 *
 * Members:
 *   ARROW_LEFT   - Left arrow key
 *   ARROW_RIGHT  - Right arrow key
 *   ARROW_UP     - Up arrow key
 *   ARROW_DOWN   - Down arrow key
 *   DEL_KEY      - Delete key
 *   HOME_KEY     - Home key
 *   END_KEY      - End key
 *   PAGE_UP      - Page Up key
 *   PAGE_DOWN    - Page Down key
 */
enum editorKey
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/*** Data ***/

/**
 * @struct erow
 * @brief Represents a single row of text in the editor.
 *
 * This structure holds the contents and size of a single line (row)
 * in the text editor. It is used to manage and manipulate the text
 * displayed and edited by the user.
 *
 * @var erow::size
 * The length of the text in the row (number of characters).
 *
 * @var erow::chars
 * Pointer to a dynamically allocated array of characters representing
 * the contents of the row (not null-terminated).
 */
typedef struct erow
{
    int size;
    char *chars;
} erow;

/**
 * struct editorConfig
 *
 * Holds the current state and configuration of the editor.
 *
 * Members:
 *   cx, cy        - Current cursor x and y position within the editor window.
 *   screenrows    - Number of rows visible in the editor window.
 *   screencols    - Number of columns visible in the editor window.
 *   numrows       - Number of rows of text in the editor.
 *   row           - The single row of text (erow struct) managed by the editor.
 *   orig_termios  - Original terminal settings, used to restore terminal state on exit.
 */
struct editorConfig
{
    int cx, cy;
    int screenrows;
    int screencols;
    int numrows;
    erow row;
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
 * Reads a keypress from standard input and returns it.
 *
 * This function handles both regular characters and special keys (such as arrow keys,
 * Home, End, Page Up/Down, Delete, etc.) by interpreting escape sequences sent by the terminal.
 * It blocks until a key is pressed, unless an error occurs.
 *
 * Special keys are returned as predefined constants (e.g., ARROW_UP, HOME_KEY).
 * If an unrecognized escape sequence is encountered, the escape character ('\x1b') is returned.
 *
 * @return int The character code of the key pressed, or a special key constant.
 *         On error, the function calls die("read") and does not return.
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
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '1':
                        return HOME_KEY;
                    case '3':
                        return DEL_KEY;
                    case '4':
                        return END_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                    }
                }
            }
            else
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
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O')
        {
            switch (seq[1])
            {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
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

/*** File i/o ***/

/**
 * Opens a file and reads its first line into the editor's row buffer.
 *
 * @param filename The name of the file to open.
 *
 * This function attempts to open the specified file in read mode. If the file
 * cannot be opened, it calls the die() function to handle the error. It reads
 * the first line from the file using getline(), trims any trailing newline or
 * carriage return characters, and stores the resulting string in E.row.chars.
 * The size of the line is stored in E.row.size, and E.numrows is set to 1.
 * The function frees any allocated memory for the line buffer and closes the file.
 */
void editorOpen(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("fopen");
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    linelen = getline(&line, &linecap, fp);
    if (linelen != -1)
    {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;
        E.row.size = linelen;
        E.row.chars = malloc(linelen + 1);
        memcpy(E.row.chars, line, linelen);
        E.row.chars[linelen] = '\0';
        E.numrows = 1;
    }
    free(line);
    fclose(fp);
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
 * @brief Draws each row of the editor to the append buffer.
 *
 * This function iterates over each visible row on the screen and appends the appropriate
 * content to the given append buffer. If there are no file rows to display, it shows a
 * welcome message centered on the screen. Otherwise, it displays the contents of each
 * row, truncated to the screen width if necessary. Each line is cleared to the end and
 * terminated with a newline, except for the last line.
 *
 * @param ab Pointer to the append buffer where the screen content will be written.
 */
void editorDrawRows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        if (y >= E.numrows)
        {
            if (E.numrows == 0 && y == E.screenrows / 3)
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
        }
        else
        {
            int len = E.row.size;
            if (len > E.screencols)
                len = E.screencols;
            abAppend(ab, E.row.chars, len);
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
 * Handles a single keypress event in the editor.
 *
 * This function reads a keypress and executes the appropriate action:
 * - On Ctrl-Q, clears the terminal and exits the editor.
 * - On Home or End, moves the cursor to the start or end of the line.
 * - On Page Up or Page Down, moves the cursor up or down by one screenful.
 * - On arrow keys, moves the cursor in the corresponding direction.
 * All other keys are ignored.
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

    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        E.cx = E.screencols - 1;
        break;
    case PAGE_UP:
    case PAGE_DOWN:
    {
        int times = E.screenrows;
        while (times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
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
 * Initializes the editor state.
 *
 * This function sets the initial values for the editor's cursor position (E.cx, E.cy)
 * and the number of rows (E.numrows). It also retrieves the current size of the terminal
 * window and stores the number of screen rows and columns in E.screenrows and E.screencols.
 * If retrieving the window size fails, the function terminates the program with an error message.
 */
void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.numrows = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

/**
 * @brief Entry point of the text editor program.
 *
 * Initializes the terminal in raw mode and sets up the editor state.
 * If a filename is provided as a command-line argument, opens the file in the editor.
 * Enters the main loop, which continuously refreshes the screen and processes user keypresses.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line argument strings.
 * @return int Returns 0 upon successful execution.
 */
int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
