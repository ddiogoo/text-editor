# Text Editor

A simple, minimal text editor built in C, inspired by kilo. It supports basic editing, syntax highlighting, and search features.

## Features

- Open and edit text files
- Syntax highlighting (for supported file types)
- Search functionality
- Keyboard navigation (arrows, Home/End, Page Up/Down)
- Clean, minimal interface
- Easy to build and run

## Getting Started

1. **Clone and navigate to the project folder:**

   ```bash
   git clone https://github.com/ddiogoo/text-editor.git && cd text-editor
   ```

2. **Compile the code:**

   ```bash
   make
   ```

3. **Run the editor:**

   ```bash
   ./kilo [filename]
   ```

   - If you provide a filename, it will open that file. Otherwise, it starts with an empty buffer.

## Usage

- Use arrow keys to move the cursor.
- Press `Ctrl+Q` to quit.
- Home/End/Page Up/Page Down keys are supported.
- More features and keybindings are described in the code comments.

## Makefile

This project uses a Makefile to manage the build process. The Makefile includes targets for compiling the source code, cleaning up object files, and running the editor.

### Variables

- `$(CC)`: The C compiler (default: `cc`). You can override it to use a different compiler.

### Flags

- `-Wall`: Enables all common warning messages.
- `-Wextra` and `-pedantic`: Enable additional warnings and strict C standard compliance.
- `-std=c99`: Use the C99 standard for modern C features.

> **Note:** If your code compiles successfully, you should not see any warnings except for possible "unused variable" warnings. If you encounter other warnings, double-check your code for accuracy.

## Contributing

Contributions are welcome! Feel free to open issues or submit pull requests.

---

*This project is for learning and demonstration purposes.*
