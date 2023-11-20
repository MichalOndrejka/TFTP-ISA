# Author
- Name: Michal
- Surname: Ondrejka
- Login: xondre15
- Date: 12.11.2023

# Description
task is to implement a client and server application for file transfer via TFTP (Trivial File Transfer Protocol) exactly according to the corresponding RFC specification of the given protocol (see literature section).

# Extensions and limitiations
Tsize is not implemented

# Startup
## Download

client: ./tftp-client -h 127.0.0.1 -p 5000 -f file_download.txt -t file.txt
server: ./tftp-server -p 5000 server/

## Upload

client: ./tftp-client -h 127.0.0.1 -p 5000 -t file_upload.txt < file.txt
server: ./tftp-server -p 5000 server/

# List of submitted files
README.md
Makefile
include/tftp-client.h
include/tftp-server.h
src/tftp-client.c
src/tftp-server.c
manual.pdf
