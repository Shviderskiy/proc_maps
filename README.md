# proc_maps

A library for retrieving records from `/proc/${PID}/maps` file, which contains the memory map of a process. Not uses dynamic memory. Parsed records are passed to callback function. To stop iteration callback function should return non-zero value. On success `proc_maps_iterate` returns 0, or negative value if an error occurred.

## Getting sources
```bash
git clone https://github.com/Shviderskiy/proc_maps.git
```

## Build example
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_PROC_MAPS_EXAMPLE=ON # -DCMAKE_C_FLAGS=-m32
cmake --build .
```
