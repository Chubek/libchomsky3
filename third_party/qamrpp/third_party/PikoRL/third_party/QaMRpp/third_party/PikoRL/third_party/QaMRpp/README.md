# QaMRpp: Minimal, Extensible, Header-Only Lua --- My Best AI Slop Yet!!

Have you ever wanted to embed Lua in a C++ program, say, a game engine, and you thought to yourself "I hotta link *another* library?"

Worse, when you ship your application, if you are using Sol2, would you ship Sol2 too?

QaMRpp is the bastard child of Lua and Sol2. The entire language, Lua, is embedded in a single header file, `QaMRpp.hpp`.

You can build all the runtime libraries you want inside this header file, but QaMRpp has a native extension interface, `QaMRpp-Library.{c,h}`. You don't have to link anything against anything! You write your library based on the ABI defined by `QaMRpp-Library.h`, compile it with the C file, make it a shared library, and now `QaMRpp.hpp` can load these `so/dll/dylib` files. 

You can either hard-path the shared modules, or place them inside `$HOME/.qamrpp`. This way, you can only load the library by name. A bit of 'todo', have people be able to change `$HOME/.qamrpp`.

The Lua standard library is written this way inside the `std` library. They are good practice. You can also read the documentation.

QaMRpp also accepts plugins. Look into `plugins` directory. We got CLI plugin which is used to make `cli/qamrpp-cli.cpp`. This a honest-to-God REPL. Not as shiny as those of Elixir or the new Python REPL. But it's still a REPL.

And remmeber, there is room for improvement! AOT is coming. Because at the moment, QaMRpp is only tree-walked. We can AOT this tree, and make it go wroom!

We can add translators via plugins. There's already a C translator in `plugins` directory.

#### It Shall Improve
