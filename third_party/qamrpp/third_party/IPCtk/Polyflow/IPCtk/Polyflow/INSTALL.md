# Installation Guide

## Header-Only Installation

Polyflow is header-only. Copy the following files to your include path:

```bash
cp Polyflow.hpp /usr/local/include/
cp Polyexec.hpp /usr/local/include/
cp PolyflowData.hpp /usr/local/include/
cp PolyflowDSLUtils.hpp /usr/local/include/
```

Also copy dependency libraries:

```bash
cp -r Grafitt /usr/local/include/
cp -r ZethaDB /usr/local/include/
cp -r IPCtk /usr/local/include/
```

## CMake Installation

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

This installs:
- Headers to `/usr/local/include/polyflow/`
- `polyflow-monitor` binary to `/usr/local/bin/`

## Usage in Your Project

### Direct Include

```cpp
#include <polyflow/Polyflow.hpp>
#include <polyflow/Polyexec.hpp>
```

Compile with:

```bash
g++ -std=c++20 -I/usr/local/include your_app.cpp -pthread -o your_app
```

### CMake Integration

Add to your `CMakeLists.txt`:

```cmake
find_package(Polyflow REQUIRED)
target_link_libraries(your_target PRIVATE polyflow)
```

Or as a subdirectory:

```cmake
add_subdirectory(polyflow)
target_link_libraries(your_target PRIVATE polyflow)
```

## Requirements

- **Compiler**: GCC 10+, Clang 12+, or MSVC 19.29+
- **Standard**: C++20
- **Platform**: Linux/Unix (POSIX)
- **Dependencies**: pthread

## Verification

Test installation:

```bash
cat > test.cpp << 'EOF'
#include <polyflow/Polyflow.hpp>
#include <iostream>

int main() {
    polyflow::task_graph graph;
    auto t = graph.add_task("test");
    std::cout << "Polyflow installed correctly!\n";
    return 0;
}
EOF

g++ -std=c++20 test.cpp -pthread -o test
./test
```

## Monitor Binary

After installation, the monitor is available:

```bash
polyflow-monitor --discover
```

## Uninstall

```bash
cd build
sudo make uninstall
```

Or manually:

```bash
sudo rm -rf /usr/local/include/polyflow
sudo rm /usr/local/bin/polyflow-monitor
```
