# clwrapper #

`clwrapper` is a simple `cc.exe` which calls into the compiler that
ships with Visual Studio.  The aim is to make it easier to share
Makefiles between Unix and Windows.

## Rationale ##

The Microsoft C/C++ compiler is tedious to invoke.

There are a few options, none of them good:

  1. Create and maintain project and solution files, then either:
     a. Build from the VS IDE
     b. Invoke MSBuild from the command line

  2. Run a specialized command prompt (?!) and invoke cl.exe, which
     has command line option syntax that is inconsistent with other
     mainstream compilers. 

  3. Use Cygwin or MinGW, which is not the most "native-feeling" compiler
     on Windows.

Therefore, this project: Invoke the MS compiler, but give it a familiar
Unix-like command line interface.  No need to come up with "project
files" and "solution files".  No need to use a specialized Start Menu
shortcut to invoke it from a specialized command prompt.  Just use `cc`,
mostly the same way you would at a Unix machine.

## Building ##

Type `nmake` from the Visual Studio Command Prompt (Accessible from
Start).

Note that `cc.exe`, the resulting binary, will not need to be run
from the special VS command prompt; it will search for the VS binaries
and Windows SDK on its own.

## Options ##

`cc` will accept:

   `-V major.minor`

      By default, `clwrapper` will search for installed Visual Studios
      in the registry and pick the highest one.  This value picks a
      specific version.  I have variously seen this work with `9.0` (2008),
      `10.0` (2010), `11.0` (2012), `14.0` (2015).

   `-sdkversion major.minor`

      Version of the Windows SDK to use.
      By default, `clwrapper` will try the highest version it finds.

   `-m32`
   `-mamd64`
   `-mwoa`

      Build for x86, amd64, or NT on ARM, respectively.
      Some of these will vary based on which compilers you've installed
      or what version of Visual Studio is being used.

   `-static-crt`

      Link CRT statically.  This wrapper will always use multi-threaded
      CRT, keeping with modern assumptions that threads are a fact of life.

   `-O[0-9s]`
   `-Wall`
   `-Werror`
   `-o`
   `-c`
   `-D`*MACRO[=value]*
   `-I`*incdir*
   `-L`*libdir*
   `-l`*lib*
   `-shared`

      Semantics for the above similar to `gcc`.

## clwrapper-lib.exe ##

`clwrapper-lib` is a small hack to use this project's "Visual Studio-seeking"
code to find lib.exe and create static libraries.  It supports the following
from `cc.exe`:

   `-V major.minor`
   `-sdkversion major.minor`
   `-m*`

Other options are assumed to be passed directly to lib.exe.

## Bugs and what's missing ##

* A bunch of PE file specific options are missing.  For example MinGW can
specify DllMain with `-Wl,-e,EntryPoint`.

* Would be wise to have a pass-through to give additional options directly to
cl.  Sometimes this actually works by accident, but an "official" hook might
be beter.

* Windows software is completely confused about command line passing, escaping,
and where the boundaries of `argv` lie.  I haven't given any special thought
to this problem.  Cases like `-DFoo="long expression"` probably don't work.
