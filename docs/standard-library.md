# BuLang / Zen Standard Library Notes

This page is based on the tutorial examples. Adjust names if the VM implementation changes.

## Global functions

### Output

```zen
print(value);
```

### Conversion

```zen
int(value);
float(value);
```

### Length

```zen
len(value);
```

Some types also support `.len()`.

## Math functions

```zen
sin(x)
cos(x)
tan(x)
atan2(y, x)
rad(degrees)
deg(radians)
sqrt(x)
pow(a, b)
abs(x)
floor(x)
ceil(x)
log(x)
exp(x)
clock()
```

## String methods

```zen
s.len()
s.upper()
s.lower()
s.sub(start, end)
s.find(pattern)
s.replace(old, new)
s.starts_with(prefix)
s.ends_with(suffix)
s.trim()
s.char_at(index)
s.split(separator)
```

## Array methods

```zen
a.len()
a.push(value)
a.pop()
a.contains(value)
a.index_of(value)
a.reverse()
a.slice(start, end)
a.insert(index, value)
a.remove(index)
a.clear()
a.join(separator)
a.sort()
a.sort("desc")
```

## Map methods

```zen
m.set(key, value)
m.get(key)
m.has(key)
m.delete(key)
m.clear()
m.size()
m.keys()
m.values()
```

## Set methods

```zen
s.add(value)
s.has(value)
s.delete(value)
s.clear()
s.size()
s.values()
```

## Process/runtime functions and statements

Seen in tutorials:

```zen
process
frame
advance_process()
spawn
yield
resume(fiber)
father
son
```

These are useful for cooperative scheduling and game-like update flows.

## Typed buffers / arrays

The examples mention typed array names such as:

```zen
Int8Array
Uint8Array
Int16Array
Uint16Array
Int32Array
Uint32Array
Float32Array
Float64Array
```

Document their exact constructors and methods according to your C++ VM implementation.
