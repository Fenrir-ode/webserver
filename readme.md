# Webserver for fenrir

## How to compile?
This project need [CMake](https://cmake.org/).

If you're on a Linux(or wsl) system, it can be compiled like this:

```
mkdir build
cd build
cmake ..
make
```

## Usage

The server will traverse the file tree of the specified folder and look for files with ".iso", ".cue", ".chdr", ".gdi", ".nrg" extension

```
FenrirServer -d <folder>
```

For example:

```bash
FenrirServer -d /mnt/d/
```

## Compatible image formats

It supports .cue/.nrg/.iso/.chd.

Compatible image format is provided by mame and libchr libraries.


## Licence
[Mongoose](https://cesanta.com/) as http server.

Toc parsing based on [MAME](https://docs.mamedev.org/).

[libchdr](https://github.com/rtissera/libchdr) for CHD supports.

are each distrubted under their own terms.


## Docker

You can use Docker to launch the server.

For example:

```ps
docker run --rm -it --init  -p 3000:3000 -v ${pwd}:/isos ghcr.io/fenrir-ode/webserver:main FenrirServer -d /isos/
```