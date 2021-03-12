cmake_minimum_required(VERSION 3.15)
project(fastcgi C)

add_executable(fastcgi main.c fcgi_proto.c hashtable.c writer.c request_pool.c vector.c response.c)
