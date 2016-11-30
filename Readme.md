

# Concurrent HTTP Proxy Server #
>CS425: Computer Networking (Project-2)
						

* Proxy server supports the GET method to serve clients requests and then connects to requested host and responds with host data.
* It can handle requests from both protocols HTTP/1.1 and HTTP/1.0.
* Multiple clients can connect and make request to it at the same time.
* Server perfectly provides appropriate Status-code and Response-Phrase values in response to errors or incorrect requests from client.
* Server is designed such that it can run continuously until an unrecoverable error occurs.
### How to use it ###

* Run makefile using command "make" or "make all".
* It creates an execuatble file `proxy`.
* Run server using command: 
```sh 
./proxy <port-number>
```


