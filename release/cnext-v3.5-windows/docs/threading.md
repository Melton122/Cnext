# Threading

## Importing the Thread Module

```cnext
import thread
```

## Mutex

Mutex provides mutual exclusion for thread-safe access:

```cnext
var m = mutex_new()

// Lock before accessing shared data
mutex_lock(m)
// critical section
var x = shared_data
mutex_unlock(m)

// Clean up
mutex_free(m)
```

## Channel

Channel provides communication between threads:

```cnext
var ch = channel_new(8)  // Buffer size 8

// Send messages
channel_send(ch, "hello")
channel_send(ch, "world")

// Receive messages
var msg1 = channel_recv(ch)
var msg2 = channel_recv(ch)

// Clean up
channel_free(ch)
```

## Basic Threading Example

```cnext
import thread

main {
    var m = mutex_new()
    var counter = 0

    // Thread-safe increment
    mutex_lock(m)
    counter = counter + 1
    mutex_unlock(m)

    mutex_free(m)
}
```

## Producer-Consumer Pattern

```cnext
import thread

main {
    var ch = channel_new(8)

    // Producer
    channel_send(ch, "message1")
    channel_send(ch, "message2")
    channel_send(ch, "message3")

    // Consumer
    var msg1 = channel_recv(ch)
    var msg2 = channel_recv(ch)
    var msg3 = channel_recv(ch)

    printin(msg1 + " " + msg2 + " " + msg3)
}
```

## Thread-Safe Counter

```cnext
import thread

main {
    var m = mutex_new()
    var count = 0

    func increment() {
        mutex_lock(m)
        count = count + 1
        mutex_unlock(m)
    }

    // Multiple threads can safely call increment()
    increment()
    increment()
    increment()

    printin(count)  // 3
    mutex_free(m)
}
```

## Channel Communication

```cnext
import thread

main {
    var ch = channel_new(16)

    // Multiple messages
    for int i = 0; i < 5; i = i + 1 {
        channel_send(ch, "message " + i)
    }

    // Receive all messages
    for int i = 0; i < 5; i = i + 1 {
        var msg = channel_recv(ch)
        printin(msg)
    }
}
```

## Mutex for Shared State

```cnext
import thread

main {
    var m = mutex_new()
    var shared_list = {}

    func add_item(str item) {
        mutex_lock(m)
        shared_list.append(item)
        mutex_unlock(m)
    }

    func get_items() -> str[] {
        mutex_lock(m)
        var items = shared_list
        mutex_unlock(m)
        return items
    }
}
```

## Channel Patterns

### Fan-Out

```cnext
import thread

main {
    var ch = channel_new(8)

    // Multiple producers
    channel_send(ch, "from producer 1")
    channel_send(ch, "from producer 2")
    channel_send(ch, "from producer 3")

    // Single consumer
    for int i = 0; i < 3; i = i + 1 {
        var msg = channel_recv(ch)
        printin(msg)
    }
}
```

### Fan-In

```cnext
import thread

main {
    var ch1 = channel_new(8)
    var ch2 = channel_new(8)

    // Single producer
    channel_send(ch1, "message 1")
    channel_send(ch2, "message 2")

    // Multiple consumers
    var msg1 = channel_recv(ch1)
    var msg2 = channel_recv(ch2)

    printin(msg1 + " " + msg2)
}
```

## Thread Safety Tips

1. Always use mutex when accessing shared data
2. Keep critical sections as small as possible
3. Always unlock mutex, even on error paths
4. Use channels for communication, mutex for shared state
5. Avoid deadlocks by locking in consistent order

## Platform Support

- **Windows**: Win32 threads (CreateThread)
- **Linux/macOS**: pthreads
- **Other**: Not yet supported

## Example: Parallel Processing

```cnext
import thread

main {
    var m = mutex_new()
    var results = {}
    var ch = channel_new(8)

    // Process items
    for int i = 0; i < 10; i = i + 1 {
        var result = i * i
        channel_send(ch, result)
    }

    // Collect results
    for int i = 0; i < 10; i = i + 1 {
        var result = channel_recv(ch)
        mutex_lock(m)
        results.append(result)
        mutex_unlock(m)
    }

    printin(results)
    mutex_free(m)
    channel_free(ch)
}
```
