# big4f

This is a small, relatively platform independent commandline tool to pack and unpack BIG files, a file format used by many EA games, such as Command & Conquer Generals, Battle for Middle Earth ...

The goal was to create a tool that can be used in batch scripts, to fasten the workflow of packing modded files.

No external files or libraries beyond gcc via MINGW in MSYS2, or just a normal linux, are required to compile or run this tool.

## usage

The usage is simple:

    big4f x bigfile.big outdir

extracts `bigfile.big` into `outdir`. As does using `e`, instead of `x`.

    big4f 4 indir bigfile.big

packs all files from `indir` into `bigfile.big`, with a `BIG4` header. Respectively

    big4f f indir bigfile.big

does the same, but creates a BIG file with a `BIGF` header.

I wouldn't advise creating BIG files over 2GB.

Please create an issue if you encounter problems with this tool. I've tested it to the best of my knowledge.

## download

Windows users can download the latest version from the releases page:

https://github.com/withmorten/big4f/releases

The Windows version was compiled with:

    gcc -O3 -s -static -o big4f.exe main.c bigfile.c util.c

Linux users can just compile it themselves with:

    gcc -O3 -s -static -o big4f main.c bigfile.c util.c

Please let me know if there are any problems doing this.

## thanks

These two sites were used as reference of the BIG file format:

http://wiki.xentax.com/index.php/EA_BIG_BIGF_Archive
http://wiki.xentax.com/index.php/EA_VIV_BIG4
