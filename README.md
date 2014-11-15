bob
===

Bob is a new build tool that follows the same philosophy as Make did when it was first designed, while not taking its design. As such it does not take along its syntax, its quirks, its portability issues nor does it support the tools that are built upon Make.

Bob is based on the premise that building software really isn't that hard, and that the description to your computer should be about as long as the description to a knowledgeable software engineer. If the software engineer can understand it from 20 lines of description, the computer should not need much more than that - certainly not more than 200 as an absolute upper limit. 

An additional limit is that the total build time for a giant project should not exceed a realistic amount of time for a developer to wait. As software engineers there is little we value more than time and responsiveness, and Bob aims to do the least amount of work in the quickest possible way. Testruns on a software stack with 7000 source files result in it determining what to build within 3.4 seconds total, with rules complex enough to do full dependency analysis and that target multiple outputs, including checking for past outputs with a total of 250000 files checked.

Installing / Bootstrapping
==========================

To compile Bob, you need to have a working Bob install. As this is very unpractical for anybody starting to use it, there is a build.sh script that creates the same output. Run the build.sh script to create a 'bob' executable in the bin folder.

To install bob, first compile it with the build.sh script and then run

    sudo bin/bob install

to install it to /usr/bin/bob. If you already have a working bob but want to upgrade (or downgrade) then run 

    sudo bob install

to install that version. 

Design
======

Build tools vary in their design approach. Many choose to take the "obvious" approach of describing the projects and files used in your software and then explicitly define what rules to use for each project. This is the most versatile solution that allows for exceptions and cornercases at every point - which is also their weakness, they will allow you to make mistakes and have quirks everywhere unless you yourself prevent it. The second approach to consider is to describe what things need to run before the full build is done and to apply heuristics to skip parts of such a build. Tools like Ant and CCache advocate this, which gives a stable build that is faster than using a shellscript, but that will not necessarily be as efficient as a full parallel build, nor scaleable as it will always run across all software parts. The third approach, the one used by Make, is to define rules to derive what uses what, and then to use those rules with the outputs that you desire to find out what inputs are needed. This has the unfortunate downside that to define the outputs you must list all things you want to build, or derive it from your inputs by hand and then reverse-engineer what source files led to a given output file being desired. A more superficial comment is that Makefiles are hard to write as they have very exact whitespace requirements.

For Bob we've decided to take a similar approach to Make, except reversed. The base design of make - using files as dependencies, using file-update-markers as signals to rebuild and to construct a DAG that indicates the full build process, to get a parallel and quick build - is kept. The changes are in how to specify it and the file formats, plus the side conditions. The main differences are:

- Rules are generative (forward) rather than derivative (backward).
- The % operator that allowed for a single point of variation is taken out and replaced with full regular expression support, as well as instancing. 
- Build command console outputs is repeated if the step was successful, but with visible output. This ensures that you can use warnings as useful notices, as every build will keep the warnings.
- Files that are to be created in the build count as actual full files for the derivation of what is built. This ensures that a second run cannot build more, and that every run will build everything that could be built.
- The build is run until there is no command ready for execution. This is the logical equivalent of "make -k" for useful compiler errors, but does not run every command if it knows that there are invalid inputs to it.
- Build are by default fully parallel (number of threading units plus one) rather than serial.
- It will always traverse up directories to find the "project root" or "build root" and build from there. This means you can always build from subdirectories.

Rulefiles
=========

The syntax for rulefiles looks a lot like makefiles intentionally. The major differences are that the % operator does not exist anymore, and rules are written with " => " rather than a colon. For a simple example, let's consider the typical Hello World program:

    [ hello.c ]
    #include <stdio.h>
    int main() {
      printf("Hello World!\n");
    }

To compile this, you would compile hello.c into an object file, and link the object file into an executable. The rulefile looks a lot like your manual instructions:

    [ Rulefile ]
    hello\.c => hello.o
      gcc -o $(OUTPUT) -c $(INPUTS)

    hello\.o => hello
      gcc -o $(OUTPUT) -c $(INPUTS)

    hello => all

You can use "$@" as an alias for $(OUTPUT) and "$^" as an alias for $(INPUTS). Note that there is no $< ; the reason for this will be apparent later. The "all" rule is the default target that is built if you do not specify what to build.

Of course, this does not scale to larger projects as you explicitly specify what files to build. Let's simplify this rule file and make it generic:

    [ Rulefile ]
    (.*)\.c => \1.o
      gcc -o $(OUTPUT) -c $(INPUTS)

    .*\.o => hello
      gcc -o $(OUTPUT) -c $(INPUTS)

    hello => all

 
The first rule now has a regular expression on the left that matches all C files, and compiles each to their name with a .o at the end. The second rule takes all files that end with .o and links them to hello. This will automatically take any files in any subdirectories and use them. The rules combine based on the first output they specify, so the first rule will generate multiple object files and the second rule will create a single "hello" executable. Now, let's make a debug and a release build for this:

    [ Rulefile ]
    CCFLAGS-Debug=-O0 -g
    CCFLAGS-Release=-O3

    each BUILDTYPE: Debug Release
    (.*)\.c => $(BUILDTYPE)/\1.o
      gcc $(CCFLAGS-$(BUILDTYPE)) -o $(OUTPUT) -c $(INPUTS)

    $(BUILDTYPE)/.*\.o => $(BUILDTYPE)/hello
      gcc $(CCFLAGS-$(BUILDTYPE)) -o $(OUTPUT) -c $(INPUTS)

    $(BUILDTYPE)/hello => all

    endeach

Now we get two output executables, one compiled as debug without optimization, the second compiled with maximum optimization. This is called "instantiating" - you create a copy of each rule in the "each" block for every option in the each. You can repeat this trick and also instantiate on another variable with its own option set to get all possible combinations of them. Note that the "all" target is instantiated twice so both buildtype's "hello" executable is added to the generic "all" output.

Let's generalize this rulefile to build a set of projects. We're beyond the simple "hello" target now and we're making a single rulefile that will build all projects in each their own directory:

    [ Rulefile ]
    CCFLAGS-Debug=-O0 -g
    CCFLAGS-Release=-O3

    each BUILDTYPE: Debug Release
    (.*)\.c => $(BUILDTYPE)/\1.o
      gcc $(CCFLAGS-$(BUILDTYPE)) -o $(OUTPUT) -c $(INPUTS)

    $(BUILDTYPE)/([^/]*)/.*\.o => $(BUILDTYPE)/bin/\1
      gcc $(CCFLAGS-$(BUILDTYPE)) -o $(OUTPUT) -c $(INPUTS)

    $(BUILDTYPE)/bin/.* => all

    endeach

Functions
=========

Given a number of subdirectories, this will compile each of them into objects and then link each set of objects into their own binaries. The all target builds all binaries for both build types. In order to allow more creative variable use, there are some functions for use:

      $(filter Linux, SomeFileForLinux.cpp SomeFileForWindows.cpp) # this filters out the Linux files

Filter is used for when you want to add an exception to a rule. You can use it for filtering out files that do not need to be built or taken into account.

      $(subst a(.*),b\1,anything everything) # this replaces 'anything' with 'bnything' and leaves 'everything' intact

Subst replaces single words that match a given regular expression with a replacement expression, and leaves the rest intact.

      $(rep_subst (.*),\1 $(\1_DEPS), Hello)

Rep\_subst does the same thing that subst does, except that it repeats until the list does not change anymore (or a maximum of 500 times). This is an advanced option to allow you to traverse dependency chains for include paths or linker inputs.

Special inputs and outputs
==========================

There are some options for handling special cases, such as 

    (.*)\.c => \1.o [\1.d]

files that are generated by a rule but that are not "desired" outputs to rebuild (such as cache files or dependency files)

    (.*)\.c [\1.h] => \1.o

Additional inputs that also contribute to this file, so that when those additional files change the rule is rerun.

    .*/Generated/.*\.h => GeneratedFiles

    (.*)\.c <GeneratedFiles> => \1.o

In some cornercases it is not possible to determine all possible relations, such as with generated header files. In this case it's possible to force-order the use of the generated file after the creation of the file by a "build-before" target between pointy brackets. First make a rule that depends on all generated header files that outputs into a tag file, and then make all further compilations depend on the pseudotarget with pointy brackets. This makes the build not execute until after GeneratedFiles is done, but will not retrigger your build if no actual dependencies change.

### Importing GCC generated dependencies

There is a possibility to import dependency files that match a certain pattern:

    depfiles .*\.d

This imports the dependency files (such as generated by GCC with "-MMD") for source/header dependency information. 

### Splitting up a large rulefile

It is possible to put your rules or variable definitions into multiple files:

    include depfile

This includes a specific file as if it is part of the rulefile.

### Location-specific target choice

The default build target is "all". If you want to build a given target instead for a given subdirectory, you can override it with a "target.bob" file that contains the name of the replacement target.


