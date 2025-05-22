# win32_www

A small example of using ClASP from a simple, bare win32 winsock application.

The application is mostly C code, except for the contents of `index.clasp`, which require C++ for the multiple overloads of `httpd_send_expr()` used to facilitate `<%= ... %>`

This is primarily provided as a demonstration that ClASP can be run on very different platforms, but may be usable as a starter for a browser based user interface internal to a windows application.
