# CaptureBateC
Automatic capture of Chaturbate similar to CaptureBate, but slightly different and completely rewritten in C

[![Build Status](https://img.shields.io/travis/maki-chan/CaptureBateC/master.svg)](https://travis-ci.org/maki-chan/CaptureBateC) [![Coverity Scan Build Status](https://img.shields.io/coverity/scan/12259.svg)](https://scan.coverity.com/projects/maki-chan-capturebatec)

## Installation and Usage
The following subchapters will explain you how to setup the application to automatically capture Chaturbate streams. **Please note, that in the current state, the application is more a PoC. It does work, but there most probably will be bugs!**

### Prerequisites
You need the following libraries/applications installed on your system:
- ffmpeg
- libcurl
- PCRE2

Here is an example on how to install the needed libraries on Mac OS X (you will need [brew](https://brew.sh) for that!):

    $ brew update
    $ brew install pcre2 ffmpeg

And here is an example on how to install the needed libraries on Ubuntu (you need Ubuntu 16.04+):

    $ sudo apt update
    $ sudo apt install ffmpeg libpcre2-dev libcurl4-openssl-dev

**NOTE: Windows users should use MinGW to compile the executable. I do not provide support for Cygwin setups. If MinGW does not work, file a bug report, most probably it's my fault.**

### Compile CaptureBateC
Download or clone the repository from GitHub and change to the directory with the file `Makefile` in your shell. Then just run the following command:

    $ make

It should work out of the box, and you will get an executable named `capturebate`. You can then also try to install the file with the following command:

    $ sudo make install

This should install the executable to `/usr/local/bin` and you should be able to use it from everywhere on your system.

### Usage of CaptureBateC
The usage of CaptureBateC is simple. First, you have to specify all models you want to capture in a file called `models.txt` (one model per line; use their usernames on Chaturbate). The file has to be created at `~/.capturebate/models.txt` for all systems but Windows. On Windows, the file has to be at `%APPDATA%\CaptureBate\models.txt` (alternatively at `C:\models.txt` if your APPDATA path could not be retrieved).

Example for my own system (Arch Linux):

    $ cat ~/.capturebate/models.txt
    chroniclove
    shycloudfractals

Once you've setup the `models.txt` file, you should call the application with a path to the directory you wish the recorded files to be in. If you want to record your files to `/home/myusername/captures/` you should start the application like this:

    $ capturebate /home/myusername/captures/

CaptureBateC will then start and watch for livestreams of the chosen models all 31 seconds. If a recordable livestream is found, a new thread is started that uses ffmpeg to capture the stream. Files will be put to your record directory with the following format:

    <modelname>_<year>-<month>-<day>_<hour>-<minute>-<second>.mp4

*I highly recommend running the application in a screen or tmux session, as it is designed to run as long as it is not killed by the user. If you are brave enough, you could even run it as a service on your seedbox or server.*

## TODO
- Code cleanup
- Comments for code
