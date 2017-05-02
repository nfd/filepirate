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

File Pirate's reason for existence is that it is much faster than other Vim plugins which do the same thing, though it's probably less configurable.

Requirements
------------
You'll need:

1. Vim with Python 3.
2. A C compiler to build File Pirate's native library.
3. A forthright, go-get-'em attitude (actually this isn't strictly necessary or, arguably, always desirable).

Installation
------------

1. Install [Pathogen](https://github.com/tpope/vim-pathogen) (or put File Pirate in your plugin path some other way).
2. Check out the repository in your ~/.vim/bundle directory: `cd ~/.vim/bundle` followed by `git clone https://github.com/nfd/filepirate.git`.
3. Compile the native library (you'll need a C compiler): `cd ~/.vim/bundle/filepirate/plugin` followed by `make`.

You may also want to run `:Helptags` inside Vim, to generate the documentation ("help filepirate").

Usage
-----
Press &lt;Leader&gt;-t to bring up the File Pirate window. Typically the Vim leader is a backslash, so this would be \\t. Start typing a filename, and files will appear below the search term you type. To select a file, move the cursor using the up and down arrows, and press enter to load the file. When the window opens, the cursor is already positioned on the first result, so if the first match is the one you want you can just hit enter.

File Pirate doesn't rescan the directory contents each time it is opened, which is a problem if you add or remove files. To get it to rescan, press &lt;CTRL-R&gt;.

If you decide you don't actually want to load a file, press &lt;ESC&gt;&lt;ESC&gt; to close the File Pirate window.

That's about it, really.

Modal usage
-----------
As an alternative to the regular usage described above, File Pirate can also be used *modally*, just like Vim itself. To configure File Pirate for modal usage, add `let g:filepirate_is_modal=1` to your `.vimrc`.

When using File Pirate this way, things are initially the same as non-modal usage. Press &lt;Leader&gt;-t to bring up the File Pirate window and start typing a filename. When summoned, File Pirate is in *insert mode*, which means that typing letters or numbers will add them to the File Pirate search on the top line.

If you press `<ESC>`, File Pirate enters *normal mode*. In this mode, most of File Pirate's keys are unmapped, so you can do Vim things such as search the buffer, yank filenames, and so on. If you press `<Enter>` on a file name, it will be loaded as normal, and you can press ESC twice to exit as usual.

While in normal mode, pressing `i` will re-enter insert mode.

Customising the keys
--------------------
By default, File Pirate uses &lt;Leader&gt;-T to open the file picker, and &lt;CTRL-R&gt; to rescan. You can customise both of these. 

To customise the open command, first disable File Pirate's default behaviour in your .vimrc:

    let g:filepirate_map_leader=0

Then map whatever you like to invoke `"python3 filepirate_open()"`. For example, to map Ctrl-T to File Pirate, use:

    nmap <C-t> :python3 filepirate_open()<CR>

To customise the rescan command, assign the sequence you want to the global variable `filepirate_rescan`. For example, to use F5 to rescan:

    let g:filepirate_rescan="<F5>"

You can use the same technique to change the following additional key bindings, if you like:

* `g:filepirate_up`: move cursor up (default: &lt;Up&gt;).
* `g:filepirate_down`: move cursor down (default: &lt;Down&gt;).
* `g:filepirate_bs`: delete the most-recently-typed character (default: &lt;BS&gt;).
* `g:filepirate_accept`: close File Pirate, and open the file under the cursor (default: &lt;CR&gt;).
* `g:filepirate_cancel`: close File Pirate (default: &lt;Esc&gt; &lt;Esc&gt;).

Customising the keys for modal usage
------------------------------------
When `g:filepirate_is_modal` is set, File Pirate uses separate customisations for normal and insert mode. In normal mode, the customisations are as above but with `_normal` appended, and in insert mode, they are as above but with `_insert` appended.

For example, `g:filepirate_up_normal` defines the mapping for "up" in normal mode, and `g:filepirate_up_insert` defines the mapping for "up" in insert mode.

You can also completely disable keys by mapping them to the empty string, `""`. For example, you may want to completely disable cursor keys normal mode, and can do so with `let g:filepirate_up_normal=""` (and the same for `filepirate_down`).

There are several extra mappings provided for modal usage:

* `g:filepirate_enter_insert_mode`: enter insert mode (default: `i`)
* `g:filepirate_enter_normal_mode`: enter normal mode (default: `<Esc>`)
* `g:filepirate_map_extra_normal`: additional key mappings for normal mode (default: {})
* `g:filepirate_map_extra_insert`: additional key mappings for insert mode (default: {})

The final two mappings let you add customisations to File Pirate by mapping keys for specific modes. Each mapping is a (Vim) dictionary, where the dict key is the key (or key sequence) to map, and the dict value is the value to map it to. For example usage, see "Configuration examples", below.

Other customisations
--------------------
You can change the maximum number of results that File Pirate displays by setting `g:filepirate_max_results` to some integer. The default value is 10. For example, to show 20 results, put this in your .vimrc:

    let g:filepirate_max_results=20

Very large values might make File Pirate slow.

For completeness, the complete list of other customisations are:
* `g:filepirate_max_results`: number of values displayed. Default: 10
* `g:filepirate_is_modal`: whether File Pirate uses modes (see above). Default: 0

Configuration examples
----------------------

**Option 1: Nothing**

The simplest example configuration is to add nothing at all to your configuration. By default:
 * File Pirate is summoned by `<Leader>t`, which usually means `\t` unless you've remapped your leader.
 * When summoned, typing characters will immediately start searching for files, the arrow keys select, Enter chooses a file, and ESC ESC cancels.

**Option 2: Custom keys**

    let g:filepirate_map_leader=0
    nmap ff :python3 filepirate_open()<CR>
    let g:filepirate_accept="<Tab>"

This is a simple custom configuration of File Pirate. It disables the default &lt;Leader&gt;t mapping and instead maps `ff` to open File Pirate. It maps `<Tab>` to the "accept" key.

**Option 3: Modal File Pirate**

    let g:filepirate_is_modal=1
	let g:filepirate_bs_normal="<BS>"
    let g:filepirate_map_extra_normal={}
    let g:filepirate_map_extra_normal["1"]=":python3 filepirate_accept(0)<CR>"
    let g:filepirate_map_extra_normal["2"]=":python3 filepirate_accept(1)<CR>"
    let g:filepirate_map_extra_normal["3"]=":python3 filepirate_accept(2)<CR>"

This sets up File Pirate to be modal (see "Modal usage" and "Customising the keys for modal usage" above). It maps backspace to delete from the File Pirate search string even in normal mode (by default, it's not mapped).

It then sets up some extra mappings for normal mode. In normal mode only, pressing 1 will load the first file in the list, pressing '2' will load the second, and '3' will load the third.

Warning
-------
File Pirate uses a small amount of native code. Native code has dark and eldrich powers, such as the ability to crash Vim. I've tried hard to ensure it won't happen, and don't currently know of any native code bugs. Even so, save your buffers before launching File Pirate.

Reporting Bugs
--------------
Please do. Use the Github thing or email wzdd.yoho@lardcave.net.

