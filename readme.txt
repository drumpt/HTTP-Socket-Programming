README
20170178 Changhun Kim

In order to implement http socket server and client, I considered the following.
- To deal with multiples clients, server uses fork when accept is executed.
- To handle bad request, I checked if request consists of header and body, if method is valid, if filename starts with “/”, if request line ends with “HTTP/1.0\r\n” (since we only consider HTTP 1.0), and there is valid Content-Length information.
- To solve memory leak problem, I freed most of dynamic allocated variables.
- I made temp file and unliked it when all its contents are transmitted to read some message from stdin in client with POST method.
- I recalled send function until all of read buffer is sent since there are some cases that packet is not transmitted at one go.
- To avoid deadlock problem(client receives server, and vice versa), firstly get “Content-Length” and when given body size is greater than or equal to Content-Length, I intentionally break recv while loop.