>>>
GET /?p=%20 HTTP/1.1
User-Agent: Mozilla

GET /?p=%21 HTTP/1.1
User-Agent: Mozilla

<<<
HTTP/1.0 200 OK
Date: Mon, 31 Aug 2009 20:25:50 GMT
Server: Apache
Connection: close
Content-Type: text/html
Content-Length: 14

Hello World!
H
<<<
TTP/1.0 200 OK
Date: Mon, 31 Aug 2009 20:25:50 GMT
Server: Apache
Connection: close
Content-Type: text/html
Content-Length: 13

Hello People!