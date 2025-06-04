/*** Includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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
 *   orig_termios - The original terminal attributes before enabling raw mode.
 *                  Used to restore the terminal state when the editor exits.
 */
struct editorConfig
{
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

/*** Output ***/

/**
 * Draws the rows of the editor.
 *
 * This function outputs a series of lines to the terminal, each starting with a tilde (~)
 * character followed by a carriage return and newline. It currently draws 24 rows,
 * serving as a placeholder for where file contents will eventually be displayed.
 * The tildes are commonly used in text editors to indicate empty lines.
 */
void editorDrawRows()
{
    int y;
    for (y = 0; y < 24; y++)
    {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

/**
 * Refreshes the editor screen.
 *
 * This function clears the terminal screen, moves the cursor to the top-left
 * corner, draws the editor rows (currently displaying tildes as placeholders),
 * and then resets the cursor position to the top-left. It is called on each
 * iteration of the main loop to update the display.
 */
void editorRefreshScreen()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
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

int main()
{
    enableRawMode();
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}