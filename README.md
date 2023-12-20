| Supported Targets | ESP32 |
| ----------------- | ----- |

# HEAP visual representation

Visual representation of the TLSF algorithm at work in the heap component.

## How to use example

- Tested on esp32 only.
- run `idf.py build flash monitor` to use

## Example folder contents

The project **heap_representation** contains one source file in C language [heap_representation.c](main/heap_representation.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt` files that provide set of directives and instructions describing the project's source files and targets (executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── heap_representation.c
│   └── helper.h
└── README.md                  This is the file you are currently reading
```

For more information on structure and contents of ESP-IDF projects, please refer to Section [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) of the ESP-IDF Programming Guide.

## Troubleshooting

* Program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.
