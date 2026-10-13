# OS Assignment 02: Re-engineering the `ls` Utility - Report

**Name:** Huzaifa Ahmed
**Roll No:** BSDSF24M006
**Repository:** BSDSF24M006-OS-A02

## Version Summary

| Version | Feature | Branch |
|---------|---------|--------|
| v1.0.0 | Project setup, Makefile, starter code | `main` |
| v1.1.0 | Long listing (`-l`) | `feature-long-listing-v1.1.0` |
| v1.2.0 | Column display (down then across) | `feature-column-display-v1.2.0` |
| v1.3.0 | Horizontal display (`-x`) | `feature-horizontal-display-v1.3.0` |
| v1.4.0 | Alphabetical sort | `feature-alphabetical-sort-v1.4.0` |
| v1.5.0 | Colorized output | `feature-colorized-output-v1.5.0` |
| v1.6.0 | Recursive listing (`-R`) | `feature-recursive-listing-v1.6.0` |

---

## Feature 1: Project Setup (v1.0.0)

### Q1. What is the purpose of the Makefile, and how is your project built?

A Makefile describes how to build a program from its source files, so the whole build is one command, `make`, instead of typing `gcc` by hand. Make also compares file timestamps and only rebuilds what changed.

My Makefile has these parts:
- **Variables:** `CC = gcc`, `CFLAGS = -Wall -Wextra -g` (all warnings plus debug symbols), and `VERSION`, from which `SRC`, `OBJ` and `TARGET` are derived.
- **`all` / `$(TARGET)`:** links the object file into the executable `bin/ls`.
- **Pattern rule `obj/%.o: src/%.c`:** compiles each source file in `src/` into an object file in `obj/` (`gcc -c`).
- **Order-only prerequisites `| bin` and `| obj`:** create the output folders with `mkdir -p` if they do not exist, without forcing a rebuild when their timestamps change.
- **`clean`:** removes the object files and the binary.
- **`.PHONY`:** tells make that `all` and `clean` are command names, not files.

Compiling and linking are two steps (`.c` to `.o`, then `.o` to the binary), so a project with many files only needs to recompile the files that changed.

### Q2. Why does the repository use `src/`, `obj/` and `bin/`, a `.gitignore`, and one feature branch per version?

- **Folders:** separating source (`src/`), intermediate files (`obj/`) and the final program (`bin/`) keeps the repository tidy, and makes cleaning easy.
- **`.gitignore` and `.gitkeep`:** compiled files can be rebuilt from the source, so they should not be committed. `.gitignore` excludes everything inside `obj/` and `bin/`, and the empty `.gitkeep` files are the exception, so Git still keeps the empty folders.
- **Feature branches:** each feature is developed on its own branch (for example `feature-long-listing-v1.1.0`), so `main` always contains working code. When the feature is tested, it is merged into `main` and tagged (`v1.1.0`) so that every version can be checked out and built again, and a release can be published with its binary.

---

## Feature 2: Long Listing Format (v1.1.0)

### Q3. What is the difference between `stat()` and `lstat()`? Which one is appropriate for `ls -l`?

Both calls fill a `struct stat` with file information such as type, permissions, owner, size and times. They differ only for symbolic links. `stat()` follows the link and describes the target file. `lstat()` describes the link itself.

`ls -l` has to show a link as a link: type `l`, the link's own size and time, and then `name -> target`. So my `print_long_entry()` uses `lstat()` and then `readlink()` to get the target text. With `stat()`, a link would look like an ordinary file or directory, and a broken link would make the call fail.

### Q4. How do you get the file type and permissions from `st_mode`?

`st_mode` is a bit field: the upper bits hold the file type and the lower bits hold the permissions.

- **Type:** the macros `S_ISDIR()`, `S_ISLNK()`, `S_ISCHR()`, `S_ISBLK()`, `S_ISFIFO()` and `S_ISSOCK()` mask the mode with `S_IFMT` and compare it with the matching constant. My `mode_string()` uses them to choose the first character (`d`, `l`, `c`, `b`, `p`, `s` or `-`).
- **Permissions:** each of the nine characters is one bitwise AND with a mask. For example `m & S_IRUSR` is non-zero when the owner can read, so the character is `r`, otherwise `-`. The same is done with the write and execute masks for user, group and others.
- **Special bits:** `S_ISUID`, `S_ISGID` and `S_ISVTX` change the execute position to `s`/`S` or `t`/`T`.

The owner and group names come from `getpwuid()` and `getgrgid()`, and the time is formatted from `st_mtime` with `localtime()` and `strftime()`.

---

## Feature 3: Column Display (v1.2.0)

### Q5. Explain the logic for printing in "down then across" columns. Why is one simple loop not enough?

In this layout the first column is filled from top to bottom, then the second column, and so on. The terminal prints one full row at a time, so the printing order is different from the order of the array. A single loop from 0 to n-1 would print the names left to right, which is the `-x` layout.

My steps are:
1. Find the longest name (`maxlen`) while reading the directory, and set the column width to `maxlen + 2`.
2. Get the terminal width and compute `cols = width / colw`, with a minimum of 1.
3. Compute `rows = (count + cols - 1) / cols`, which is a ceiling division.
4. Use a nested loop. The outer loop goes over rows `r`, the inner loop over columns `c`, and the printed item is `names[c * rows + r]`. The loop stops for that row when the index is beyond `count`.

Example: 7 names and 3 columns give 3 rows. Row 0 prints indexes 0, 3, 6; row 1 prints 1, 4; row 2 prints 2, 5. This is why the number of rows must be known before printing.

### Q6. What does `ioctl` do here, and what happens if it fails?

`ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` asks the terminal driver for the window size and stores it in a `struct winsize`. The field `ws_col` is the number of character columns, which I use to decide how many columns of names fit on one line. This makes the output adapt when the window is resized.

If the call fails, for example when output is piped (`ls | cat`) or redirected to a file, there is no terminal to ask. My `term_width()` then returns 80. I tested this with `./bin/ls /tmp/lstest | cat`, which printed 5 columns because the column width was 16.

---

## Feature 4: Horizontal Display (v1.3.0)

### Q7. How does the program manage the display modes, and how is `-x` different from the default?

The program keeps the mode in one global variable, `display_mode`, with the values `MODE_DEFAULT`, `MODE_HORIZONTAL` and `MODE_LONG`. `main()` reads the options with `getopt()` and sets this variable. `-l` always wins: `-x` only changes the mode if it is not already `MODE_LONG`, so `-lx` and `-xl` both give the long format.

After the entries are read and sorted, `do_ls()` checks `display_mode` and calls `print_long_entry()` for each name, `print_horizontal()`, or `print_columns()`.

Both column layouts use the same array and the same column width. Only the order is different. The default layout uses the index `c * rows + r`. The `-x` layout prints `names[0]`, `names[1]`, ... in order, keeps track of the current position in the line, and starts a new line when `pos + colw` would be greater than the terminal width. Because the names are stored in an array, adding this mode only needed one new function.

---

## Feature 5: Alphabetical Sort (v1.4.0)

### Q8. Why must all entries be read into memory before sorting? What are the drawbacks for very large directories?

`readdir()` gives entries in the order chosen by the file system, one at a time, and it cannot go backwards. The name that should be printed first alphabetically could be the last one returned. So the program must read every name first, keep them in a dynamic array (`malloc` and `realloc` with doubling capacity), sort them with `qsort()`, and only then print.

Drawbacks for huge directories:
- **Memory:** it needs O(n) memory for the strings, while printing directly would need almost none.
- **Time:** sorting adds O(n log n) work.
- **Delay:** nothing appears until the whole directory has been read.

The array is also needed anyway by the column layouts, because they need the total count and the longest name before printing.

### Q9. What is the purpose and signature of the comparison function for `qsort()`?

`qsort()` works on any element type, so it takes a function pointer to a comparator: `int cmp(const void *a, const void *b)`. It must return a negative value if `a` should come first, zero if equal, and a positive value if `b` should come first.

`qsort()` passes the addresses of array elements. My elements are `char *`, so the arguments are really `char **`. My `cmp_names()` casts them with `*(const char *const *)a` to get the strings, and returns `strcmp(x, y)`. Passing `strcmp` directly would be wrong because it would compare the pointers' memory instead of the strings.

`strcmp()` compares bytes, so uppercase names sort before lowercase ones (for example `Zebra` before `apple`), and `file_number_10` comes before `file_number_2`. This is different from GNU `ls`, which uses the locale.

---

## Feature 6: Colorized Output (v1.5.0)

### Q10. How do ANSI escape codes work, and why can't `printf("%-*s")` align colored names?

ANSI escape codes are byte sequences starting with `\033[` that the terminal treats as commands instead of text. For example `\033[0;34m` selects blue, `\033[0;32m` green, `\033[0;31m` red, `\033[1;35m` pink, `\033[7m` reverse video and `\033[0m` resets the formatting. I print the code, then the name, then the reset code.

`printf("%-*s", width, str)` counts every byte of the string, including the invisible escape bytes, so a colored name would receive too little padding and the columns would not line up. To avoid this, `print_name()` prints only the colored name, and the layout functions add the spaces themselves using `colw - strlen(name)`, where `name` is the plain file name.

Colors are enabled only when `isatty(STDOUT_FILENO)` is true, so redirected output does not contain escape codes.

### Q11. How does the program detect file types and executables?

`color_for()` builds the full path and calls `lstat()`, then checks `st_mode` in this order:
- `S_ISLNK` is a symbolic link (pink),
- `S_ISDIR` is a directory (blue),
- `S_ISCHR`, `S_ISBLK`, `S_ISSOCK` or `S_ISFIFO` is a special file (reverse video),
- a name ending in `.tar`, `.gz`, `.tgz`, `.zip`, `.bz2` or `.xz`, checked by `has_suffix()`, is an archive (red),
- `st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)` being non-zero means an executable file (green).

The link check comes first so that a link is shown as a link and not as its target.

---

## Feature 7: Recursive Listing (v1.6.0)

### Q12. What is the base case of the recursion, and how does it stop?

When `-R` is set, `do_ls()` calls itself for every subdirectory it finds. The base case is a directory with no subdirectories: after printing its entries, the loop that looks for subdirectories makes no call, and the function returns. The function also returns immediately if `opendir()` fails, for example because of missing permission.

The recursion always ends because every call goes one level deeper into a finite tree and symbolic links are never followed, so the traversal cannot return to a parent directory.

### Q13. Why use `lstat()` and not `stat()` to decide whether to recurse?

`stat()` follows symbolic links, so a link pointing to a directory would be reported as a directory and the program would enter it. If the link points to one of its own parent directories, the program would keep going around the loop until the path becomes too long. A link could also take the listing outside the tree.

`lstat()` reports such a link as `S_ISLNK`, not `S_ISDIR`, so it is listed but never entered. The entries `.` and `..` are never visited either, because all names beginning with a dot are skipped.

### Q14. How is the subdirectory path built, and why is the bare name not enough?

`build_path()` joins the parent directory and the name with a `/` (and avoids a double slash if the parent already ends with one), for example `sub` and `deep` give `sub/deep`. This full path is passed to `lstat()`, to the recursive `do_ls()` call, and to the `path:` heading.

`readdir()` returns only the entry's name, not its location. `lstat()` and `opendir()` resolve a relative name from the current working directory of the program, not from the directory being listed. Passing only `deep` would look in the wrong place and fail, or open a different folder with the same name. For the same reason `print_long_entry()` and `color_for()` also build the full path first.

---

## Testing

I built the final version with `make` (no warnings) and ran these checks on a test directory containing `Zed`, `a/` (with `b/` and a symbolic link `loop` pointing back to the top directory), `lnk -> top`, `run.sh` (executable), `top` and `x.tar.gz`.

| Check | Command | Result |
|-------|---------|--------|
| Default layout | `./bin/ls dir` | Sorted columns: `Zed a lnk run.sh top x.tar.gz` |
| Horizontal | `./bin/ls -x dir` | Same names, left to right, wrapped by terminal width |
| Long format | `./bin/ls -l dir` | `total` line, permissions, links, owner, group, size, time, name, and `lnk -> top` |
| Recursive | `./bin/ls -R dir` | Heading `dir/a:` and `dir/a/b:` with their contents; the link `loop` is listed but not entered |
| Long and recursive | `./bin/ls -lR dir` | Finishes normally despite the symlink loop |
| Colors (on a terminal) | `./bin/ls dir` | Blue directory, pink link, green executable, red archive |
| Invalid option | `./bin/ls -q` | Prints the usage line and exits with status 1 |
| Missing directory | `./bin/ls /nonexistent` | `Cannot open directory : /nonexistent` |
| Empty directory | `./bin/ls emptydir` | Prints nothing, no crash |
| Piped output | `./bin/ls dir \| cat` | Plain columns without escape codes (80 column fallback) |

---

## Build and Usage

```bash
make
./bin/ls              # columns, sorted, colored
./bin/ls -l           # long listing
./bin/ls -x           # horizontal display
./bin/ls -R           # recursive
./bin/ls -lR /etc     # long format, recursive
```
