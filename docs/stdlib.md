# Standard Library

All built-in functions are globally available — no `import` required.

## Core Functions

```cnext
print("Hello")              // Print without newline
printin("Hello")            // Print with newline
str name = input("Name: ")  // Read user input
int n = len(arr)            // Length of string or array
str t = typeof(42)          // Type name as string
assert(x > 0, "x must be positive")  // Assert condition
panic("something went wrong")         // Exit with error
exit(0)                     // Exit with code
var copy = clone(x)         // Deep copy
swap(a, b)                  // Swap two variables
```

## Type Conversion

```cnext
int n = to_int(3.14)            // 3
float f = to_float(42)          // 42.0
str s = to_str(42)              // "42"
bool b = to_bool(1)             // true
char c = to_char(65)            // 'A'
int n = parse_int("123")        // 123
float f = parse_float("3.14")   // 3.14
int code = char('A')            // 65
int code = ord("A")             // 65
CnextString bytes = bytes("hello")  // Same as string
```

## String Functions

```cnext
str upper = str_upper("hello")         // "HELLO"
str lower = str_lower("HELLO")         // "hello"
str trimmed = str_trim("  hi  ")       // "hi"
bool has = str_contains("hello", "ell") // true
bool starts = str_starts_with("hello", "he") // true
bool ends = str_ends_with("hello", "lo")     // true
int idx = str_index_of("hello", "ll")  // 2
int idx = str_last_index_of("aab", "a") // 1
str r = str_replace("hello", "l", "r") // "herro"
str sub = str_substring("hello", 1, 3) // "el"
str r = str_repeat("ab", 3)            // "ababab"
str r = str_reverse("hello")           // "olleh", reverses code points (UTF-8 safe)
int n = str_count("hello", "l")        // 2
bool empty = str_is_empty("")          // true
int c = str_char_at("hello", 1)       // 'e'
int n = str_char_count("héllo")       // 5 (code points, UTF-8 aware)
int cp = str_codepoint_at("héllo", 1) // 233, -1 when out of bounds
str s = str_sub_chars("héllo", 1, 4)  // "éllo" (by character, not byte, offsets)
int idx = str_find("hello", "ll")     // 2
str s = str_capitalize("hello")        // "Hello"
str s = str_title("hello world")       // "Hello World"
str s = str_pad_left("42", 5, '0')    // "00042"
str s = str_pad_right("hi", 5, '.')   // "hi..."
str s = str_remove("hello", "l")       // "heo"
str s = str_insert("hello", 2, "XX")  // "heXXllo"
CnextStringArray parts = str_split("a,b,c", ",")
str joined = str_join(parts, "-")      // "a-b-c"
int n = str_to_int("42")              // 42
float f = str_to_float("3.14")        // 3.14
```

## Math Functions

```cnext
int a = math_abs(-5)            // 5
int m = math_min(5, 3)          // 3
int M = math_max(5, 3)          // 5
int c = math_clamp(10, 0, 5)    // 5
int r = math_round(3.7)         // 4
int f = math_floor(3.7)         // 3
int C = math_ceil(3.2)          // 4
float s = math_sqrt(16)         // 4.0
float p = math_pow(2, 10)       // 1024.0
float l = math_log(2.718)       // ~1.0
float e = math_exp(1.0)         // ~2.718
float sin_val = math_sin(3.14)  // ~0
float cos_val = math_cos(0)     // 1.0
float tan_val = math_tan(0)     // 0.0
float r = math_random()         // 0.0 to 1.0
int n = math_random_range(1, 100) // 1 to 100
int g = math_gcd(12, 8)         // 4
int l = math_lcm(4, 6)          // 12
long f = math_factorial(5)      // 120
int fib = math_fibonacci(10)    // 55
```

## Time Functions

```cnext
long now = time_now()           // Current timestamp
time_sleep(1000)                // Sleep 1 second
long ts = time_timestamp()      // Unix timestamp
str date = time_date()          // "2024-01-15"
str time = time_time()          // "14:30:00"
time_stopwatch_start()          // Start timer
long us = time_stopwatch_stop() // Microseconds elapsed
str fmt = time_format_time("%Y-%m-%d %H:%M:%S")  // Formatted time
```

## File Functions

```cnext
str content = read_file("data.txt")
write_file("output.txt", "Hello, World!")
append_file("log.txt", "New entry\n")
delete_file("temp.txt")
copy_file("source.txt", "dest.txt")
move_file("old.txt", "new.txt")
bool exists = file_exists("data.txt")
long size = file_size("data.txt")
```

## System Functions

```cnext
str dir = cwd()                 // Current working directory
chdir("/path/to/dir")           // Change directory
str os = platform()             // "windows", "linux", "macos"
CnextString args = sys_args()   // Command line args
str val = getenv("HOME")        // Get environment variable
setenv("MY_VAR", "hello")       // Set environment variable
str host = hostname()           // Machine hostname
str user = username()           // Current user
int cpus = cpu_count()          // Number of CPUs
str tmp = temp_dir()            // Temp directory
str home = home_dir()           // Home directory
sys_exec("ls -la")              // Execute command
str output = sys_shell("ls")    // Execute and capture output
```

## JSON Functions

```cnext
CnextString data = json_parse('{"name": "Alice"}')
str json = json_stringify(data)
```

## Encoding Functions

```cnext
str encoded = base64_encode("hello")
str decoded = base64_decode(encoded)
```

## Crypto Functions

```cnext
str hash = md5("hello")
str hash = sha1("hello")
str hash = sha256("hello")
str id = uuid()                 // Generate UUID
```

## Collections (Array Builtins)

```cnext
int[] arr = {1, 2, 3}
push(arr, 4)                    // Add to end
int last = pop(arr)             // Remove from end
int first = shift(arr)          // Remove from start
unshift(arr, 0)                 // Add to start
insert_at(arr, 1, 99)           // Insert at index
remove_at(arr, 2)               // Remove at index
clear(arr)                      // Remove all elements
sort(arr)                       // Sort ascending
reverse_array(arr)              // Reverse in place
bool has = contains_item(arr, 3) // Check if contains
int idx = array_index(arr, 3)   // Find index
int last = array_last(arr)      // Last element
int first = array_first(arr)    // First element
int[] slice = array_slice(arr, 1, 3) // Slice
int[] unique = array_unique(arr)     // Remove duplicates
shuffle(arr)                    // Shuffle randomly
```

## Higher-Order Array Functions

```cnext
int[] nums = {1, 2, 3, 4, 5}

// Map: transform each element
int[] doubled = array_map(nums, (int x) => x * 2)

// Filter: keep elements matching condition
int[] evens = array_filter(nums, (int x) => x % 2 == 0)

// Reduce: combine elements
int sum = array_reduce(nums, 0, (int acc, int x) => acc + x)

// Find: first element matching condition
int found = array_find(nums, (int x) => x > 3)
```

## Utility Functions

```cnext
debug(42)                       // Print debug info to stderr
gc()                            // Trigger garbage collection
long us = benchmark(1000000)    // Benchmark (microseconds)
```

## HTTP Functions (Module Required)

```cnext
import http

str response = http_get("https://api.example.com/data")
str result = http_post("https://api.example.com/submit", "data")
str result = http_put("https://api.example.com/update", "data")
str result = http_delete("https://api.example.com/delete")
```

## Thread Functions (Module Required)

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
