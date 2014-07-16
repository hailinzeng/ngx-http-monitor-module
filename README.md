Nginx Module to monitor url of non-200 http response, will send url info to remote monitor server.

### Build ###

1../configure Nginx adding this module with:
    
    ./configure (...) --add-module=/path/to/ngx_http_monitor_module

2.Build Nginx as usual with `make`.


### Configure ###
1.Configure the module.

    directive `monitor_backlog` in the **main** context. default 10.
    directive `monitor_server` in the **server** context.
 
 Example:

    monitor_backlog 55;
  
    server {
      listen       80;
      server_name  localhost;

      monitor_server 127.0.0.1 10000;
    
      ...
    }

 Now doing something like:
 
    curl -i http://localhost/nopath
 
 will send a error message to 127.0.0.1:10000.