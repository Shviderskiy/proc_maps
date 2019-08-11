# proc_maps

A library for retrieving records from `/proc/${PID}/maps` file. Not uses dynamic memory. Records are passed to callback. To stop iteration callback should return non-zero value.

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
