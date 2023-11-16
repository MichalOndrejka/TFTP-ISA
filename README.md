# Author
- Name: Michal
- Surname: Ondrejka
- Login: xondre15
- Date: 12.11.2023

# Description
task is to implement a client and server application for file transfer via TFTP (Trivial File Transfer Protocol) exactly according to the corresponding RFC specification of the given protocol (see literature section).

# Extensions and limitiations

# Startup
## Download

client: ./tftp-client -h 127.0.0.1 -p 5000 -f file_download.txt -t client/file.txt
server: ./tftp-server -p 5000 -f server/

## Upload

client: ./tftp-client -h 127.0.0.1 -p 5000 -t file_upload.txt < client/file.txt
server: ./tftp-server -p 5000 -f server/

# List of submitted files