# Simple nginx setup for webserver

nginx is configured to serve static files first, then fallback to fenrirserver.

If fenrir detect that a proxy is used (X-Real-IP header) and the file can be streamed (single data file), the file will be server by nginx.

nginx will also cache all toc / menu request.

