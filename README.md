# Overview

Graph based subsequence search.

# Compile
```
g++ -o lz -std=c++20 -ffast-math -O3 -march=native *.cpp -ltbb
```

# Usage
```
./lz [opts] <qry> <files>
```

or

```
somecmd | ./lz [opts] <qry>
```

For options, see lazy.cpp.  Help hasn't been written yet.

# Examples
```
find | ./lz myqry
find | ./lz -p myqry # parallel

find ~/ > data.txt
./lz 'myqry' data
cat data | ./lz 'myqry'
```
