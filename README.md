File Pirate, a file selection tool for Vim
==========================================

I'm not good with names
-----------------------
I don't know why I called it File Pirate. It was originally called Snake-T, which isn't any better.

What does it do?
----------------
File Pirate is a file picker inspired by Textmate's "GoToFile" dialog, and by other Vim reimplementations of this feature such as the Command-T plugin. When you summon it, it pops up a Vim window and displays all files which match the search term you type. The search is performed in such a way that the file need not contain all the characters you type contiguously, but they must appear in the same order in the file name as they do in your search term.

So, for example, if you have the following files in your directory:

    boring.c
    interesting.c
    net_lardcave_SomeClass_impl.cpp

... then typing ".c" in the File Pirate window will return all of them. However, typing ".cpp", "SomeClass" or "netlard" will only return the final one.

File Pirate is better than other Vim plugins that do the same thing because it is significantly faster when working with large collections of files (10000 or more). In other ways it is probably worse, and I would encourage you to check out other plugins if File Pirate doesn't meet your needs.

Requirements
------------
You'll need:

1. Vim with Python version 2.5 or later.
2. A C compiler to build File Pirate's native library.
3. A forthright, go-get-'em attitude (actually this isn't strictly necessary or, arguably, always desirable).

Installation
------------

1. Install [Pathogen](https://github.com/tpope/vim-pathogen).
2. Check out the repository in your ~/.vim/bundle directory: `cd ~/.vim/bundle` followed by `git clone https://github.com/nfd/filepirate.git`.
3. Compile the native library (you'll need a C compiler): `cd ~/.vim/bundle/filepirate/plugin` followed by `make`.

You may also want to run `:Helptags` inside Vim, to generate the documentation ("help filepirate").

Usage
-----
Press &lt;Leader&gt;-T to bring up the File Pirate window. Typically the Vim leader is a backslash, so this would be \\t. Start typing a filename, and files will appear below the search term you type. To select a file, move the cursor using the up and down arrows, and press enter to load the file. When the window opens, the cursor is already positioned on the first result, so if the first match is the one you want you can just hit enter.

File Pirate doesn't rescan the directory contents each time it is opened, which is a problem if you add or remove files. To get it to rescan, press &lt;CTRL-R&gt;.

If you decide you don't actually want to load a file, press &lt;ESC&gt;&lt;ESC&gt; to close the File Pirate window.

That's about it, really.

Customisation
-------------
By default, File Pirate uses &lt;Leader&gt;-T to open the file picker, and &lt;CTRL-R&gt; to rescan. You can customise both of these. 

To customise the open command, first disable File Pirate's default behaviour in your .vimrc:

    let g:filepirate_map_leader=0

Then map whatever you like to invoke `"python filepirate_open()"`. For example, to map Ctrl-T to File Pirate, use:

    nmap <C-t> :python filepirate_open()<CR>

To customise the rescan command, assign the sequence you want to the global variable `filepirate_rescan`. For example, to use F5 to rescan:

    let g:filepirate_rescan="<F5>"

You can use the same technique to change the following additional key bindings, if you like:

* `g:filepirate_up`: move cursor up (default: &lt;Up&gt;).
* `g:filepirate_down`: move cursor down (default: &lt;Down&gt;).
* `g:filepirate_bs`: delete the most-recently-typed character (default: &lt;BS&gt;).
* `g:filepirate_accept`: close File Pirate, and open the file under the cursor (default: &lt;CR&gt;).
* `g:filepirate_cancel`: close File Pirate (default: &lt;Esc&gt;).

Warning
-------
File Pirate uses a small amount of native code. Native code has dark and eldrich powers, such as the ability to crash Vim. I've tried hard to ensure it won't happen, and don't currently know of any native code bugs. Even so, save your buffers before launching File Pirate.

Reporting Bugs
--------------
Please do. Use the Github thing or email wzdd.yoho@lardcave.net.

