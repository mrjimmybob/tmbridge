# tmbridge
Lightweight C bridge that receives ESC/POS print jobs over TCP and forwards them to Epson ePOS printers over HTTPS using the Epson ePOS-Print API


A lightweight, dependency-minimal ESC/POS → Epson ePOS-Print bridge written in C.

Features
--------
- Receives raw ESC/POS over TCP (port 9100 by default)
- Translates ESC/POS commands into Epson ePOS-Print XML
- Sends print jobs to Epson printers via HTTPS
- Small, portable ANSI/POSIX C codebase
- Minimal dependencies (libcurl)
- Configuration file based
