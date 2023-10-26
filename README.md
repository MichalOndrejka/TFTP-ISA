# TFTP-ISA
task is to implement a client and server application for file transfer via TFTP (Trivial File Transfer Protocol) exactly according to the corresponding RFC specification of the given protocol (see literature section).

client: ./tftp-client -h 127.0.0.1 -p 5000 -f file.txt -t client/file.txt
server: ./tftp-server -p 5000 -f server/