# Standard Library

## IO Module

```cnext
import io

// File operations
str content = read_file("data.txt")
write_file("output.txt", "Hello, World!")
append_file("log.txt", "New entry\n")
```

## Math Module

```cnext
import math

var x = math.abs(-5)      // 5
var y = math.sqrt(16)     // 4.0
var z = math.pow(2, 10)   // 1024.0
var w = math.floor(3.7)   // 3.0
var v = math.ceil(3.2)    // 4.0
var u = math.min(5, 3)    // 3
var t = math.max(5, 3)    // 5
```

## String Utils Module

```cnext
import string_utils

var s = "  Hello, World!  "
str trimmed = str_trim(s)              // "Hello, World!"
str upper = str_to_upper("hello")      // "HELLO"
str lower = str_to_lower("HELLO")      // "hello"
bool starts = str_starts_with("hello", "he")  // true
bool ends = str_ends_with("hello", "lo")      // true
bool contains = str_contains("hello", "ell")   // true
int idx = str_index_of("hello", "ll")          // 2
str replaced = str_replace("hello", "l", "r")  // "herro"
str sub = str_substring("hello", 1, 3)         // "el"
```

## JSON Module

```cnext
import json

var data = json_parse('{"name": "Alice", "age": 30}')
str name = json_get_string(data, "name")
int age = json_get_int(data, "age")
str json_str = json_stringify(data)
json_free(data)
```

## Net Module

```cnext
import net

// HTTP
str response = http_get("https://api.example.com/data")
str result = http_post("https://api.example.com/submit", "data")

// TCP
var conn = tcp_connect("example.com", 80)
tcp_send(conn, "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
str data = tcp_receive(conn)
tcp_close(conn)
```

## File Module

```cnext
import file

copy_file("source.txt", "dest.txt")
delete_file("temp.txt")
bool exists = file_exists("data.txt")
int size = file_size("data.txt")
```

## OS Module

```cnext
import os

str home = os_home_dir()
str temp = os_temp_dir()
str cwd = os_get_cwd()
os_set_cwd("/path/to/dir")
os_mkdir("new_dir")
os_rmdir("empty_dir")
str name = os_os_name()
str host = os_hostname()
int pid = os_pid()
```

## Time Module

```cnext
import time

long now = time_now()
long ms = time_now_ms()
time_sleep(1000)  // Sleep 1 second
str formatted = time_format(now, "%Y-%m-%d %H:%M:%S")
int year = time_year(now)
int month = time_month(now)
int day = time_day(now)
int hour = time_hour(now)
int minute = time_minute(now)
int second = time_second(now)
```

## Collections Module

```cnext
import collections

// List
var list = list_new()
list_add(list, "item1")
list_add(list, "item2")
str item = list_get(list, 0)
list_set(list, 0, "updated")
int size = list_size(list)
list_remove(list, 0)
list_clear(list)
list_free(list)

// Map
var map = map_new()
map_set(map, "key1", "value1")
str value = map_get(map, "key1")
bool has = map_has(map, "key1")
map_remove(map, "key1")
int map_size = map_size(map)
map_free(map)

// Set
var set = set_new()
set_add(set, "item1")
bool contains = set_has(set, "item1")
set_remove(set, "item1")
int set_size = set_size(set)
set_free(set)
```

## Regex Module

```cnext
import regex

bool match = regex_match("hello world", "hello")
str found = regex_search("hello world", "world")
str replaced = regex_replace("hello world", "world", "there")
```

## Crypto Module

```cnext
import crypto

str md5 = crypto_md5("hello")
str sha = crypto_sha256("hello")
str encoded = crypto_base64_encode("hello")
str decoded = crypto_base64_decode(encoded)
```

## Path Module

```cnext
import path

str joined = path_join("/home", "user", "file.txt")
str dir = path_dirname("/home/user/file.txt")    // "/home/user"
str base = path_basename("/home/user/file.txt")  // "file.txt"
str ext = path_extension("file.txt")             // ".txt"
str normalized = path_normalize("/home//user/../user/file.txt")
```

## Encoding Module

```cnext
import encoding

str b64 = base64_encode("hello")
str decoded = base64_decode(b64)
str hex = hex_encode("hello")
str hex_decoded = hex_decode(hex)
str url = url_encode("hello world")
str url_decoded = url_decode(url)
```

## Process Module

```cnext
import process

var proc = process_run("ls", {"-la"})
int status = process_run_status(proc)
bool exists = process_exists(proc)
process_kill(proc)
```

## Random Module

```cnext
import random

random_seed(42)
int rand_int = random_int(1, 100)
float rand_float = random_float(0.0, 1.0)
float uniform = random_uniform(0.0, 1.0)
float gaussian = random_gaussian(0.0, 1.0)
```

## Thread Module

```cnext
import thread

var m = mutex_new()
mutex_lock(m)
// critical section
mutex_unlock(m)
mutex_free(m)

var ch = channel_new(8)
channel_send(ch, "message")
var msg = channel_recv(ch)
channel_free(ch)
```
