boa
===

`boa` - a simple generic preprocessor for UNIX systems, written in C.

The concept
-----------

Instead of inventing a new preprocessor language, `boa` allows to use a scripting language for your preprocessing needs. The input file, using the simple rules, is converted on the fly into a script text and is fed to the interpreter. Output of the interpreter is the result of the preprocessing.

Explanation for the initiated. Well, that's pretty much what the PHP does with the HTML, but coded in about 500 lines of code,  without the galore of dependecies and with the choice of a better language. Example below uses the shell, but the same sh@t work for Perl, Lua and Ruby. Just replace the shell code with the Perl/Ruby/Lua one, replace the `shell` with the `perl` or `lua` or `ruby` - and you are good to go.

See "Limitations" below.

Usage
-----

    usage: boa [-l lang] [-Q @] [-d] [-o output-file] <input-file>
    
- `-l` set the language for the input. The default is the `shell`/`/bin/sh`
- `-Q` allows to change the special character. The default is `@`.
- `-d` - dump/debug mode. The script is sent to file/stdout instead of the interpreter.
- `-o` allows to specify the output file. If not specified, the output is sent to stdout.

Example
-------

Using the Linux shell as the interpreter.

Input:

    @@@ shell @@@
    @I=3
    @for X in `seq 1 $I`; do
    line number @{$X}
    @done

Running:

    ./boa shell-example.boa
    
Output:

    line number 1
    line number 2
    line number 3

With the option `-d` one can see what is actually happening behind the curtains:

    ./boa -d shell-example.boa
    I=3
    for X in `seq 1 $I`; do
    echo -n 'line number '
    echo -n $X
    echo
    done
    
    ./boa -d shell-example.boa | /bin/sh
    line number 1
    line number 2
    line number 3

The last invocation is actually what is happening inside the `boa` when called without the `-d` option.

The preprocessor language
-------------------------

[See Wiki](https://github.com/Kervius/boa/wiki/LangDef)

Currently supported interpreters
--------------------------------

- `shell` and `bash` (/bin/sh and /bin/bash)
- `perl`
- `ruby`
- `lua`
- `dummy` (simply runs the `cat` instead of the real script language interpreter; used for testing; in normal usage is equivalent to `-d` option)

Adding a new language 
---------------------

Source code modification is necessary.

Internally, language is defined with the following struct:

    struct lang_def {
            char *name;
            char *intrp[16];
            char *print_nl;
            quote_func_t quote;
            char *print_s;
            char *print_e;
            char *preamble;
            char *closure;
    };

- `name` is the name of the language.
- `intrp` is the command to invoke the language's interpreter.
- `print_nl` is the language's statement which must print single new line.
- `quote` is the pointer to function which is used to escape the text before printing. See `shell_quote`, `perl_quote` and `c_quote` for the examples.
- `print_s` is the beginning of the language's print statement.
- `print_e` is the ending of the language's print statement. The string (or content of the `@{}` element) to be printed goes between the `print_s` and `print_e`.
- `preamble` is the language's code block to be printed in the beginning of the output. E.g. includes, imports, declarations, etc, required for the printing and such.
- `closure` is the language's code block to be printed after the last line of the preprocessed line. E.g. flush/etc.

The `quote` field, the function pointer, is the only one where new C code which might be required to support a new language.

Limitations
-----------

Languages which rely on indentation for program structure (e.g. Python) are impossible to support. To support such back-end languages, the preprocessor would have to actually parse the script lines and blocks (which are currently passed through verbatim) to recognize the desired indentation level. That directly contradicts the goal of simplicity of the `boa`.

boa.pl
------

A functional, proof-of-concept `boa` implementation in Perl. It only lacks few featured which were implemented later (e.g. escaping special character).

TODO
----
- [ ] Add chdir into the directory where the input file is located. Or not?
- [ ] Add `-C <dir>` command line option to perform chdir() before processing.
