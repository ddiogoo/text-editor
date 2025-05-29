# Text Editor

This is a simple text editor built with C. It implements all basic features of a text editor as well as some other features like syntax highlighting and search feature.

## Getting Started

1. Clone and navigate to the project folder:
   ```bash
   git clone https://github.com/ddiogoo/text-editor.git && cd text-editor
    ```

2. Compile the code:
    ```bash
    make
    ```

3. Run the editor:
    ```bash
    ./kilo
    ```

## Makefile

This project uses a Makefile to manage the build process. The Makefile includes targets for compiling the source code, cleaning up object files, and running the editor.

### Variables

- `$(CC)`: This variable represents the C compiler used for building the project. By default, it expands to `cc`, but you can override it if you want to use a different compiler.

### Flags

- `-Wall`: Enables all the commonly used warning messages. This helps catch questionable code, such as using variables before they are initialized.
- `-Wextra` and `-pedantic`: These flags enable additional warnings and enforce strict compliance with the C standard. They help ensure code quality and portability.
- `-std=c99`: Specifies that the C99 standard should be used. This allows for more modern C features, such as declaring variables anywhere within a function.

> **Note:** If your code compiles successfully, you should not see any warnings except for possible "unused variable" warnings in some cases. If you encounter other warnings, double-check your code for accuracy.

## References

- [Software Flow Control - Wikipedia](https://en.wikipedia.org/wiki/Software_flow_control)
- [Confusion about raw vs. cooked terminal modes? - Stack Overflow](https://stackoverflow.com/questions/13104460/confusion-about-raw-vs-cooked-terminal-modes)
