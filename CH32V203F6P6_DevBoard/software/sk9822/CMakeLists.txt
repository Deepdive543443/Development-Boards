cmake_minimum_required(VERSION 3.16)

# Toolchain (must be set before project())
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)
set(PREFIX                 riscv64-unknown-elf)
set(CMAKE_C_COMPILER       ${PREFIX}-gcc)
set(CMAKE_ASM_COMPILER     ${PREFIX}-gcc)
set(CMAKE_OBJCOPY          ${PREFIX}-objcopy)
set(CMAKE_OBJDUMP          ${PREFIX}-objdump)
set(CMAKE_SIZE             ${PREFIX}-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)  # no startup code for test builds

project(sk9822 C ASM)

# Files and Folders
set(TARGET   ${PROJECT_NAME})
set(SOURCE   ${CMAKE_SOURCE_DIR}/src)
set(NEWLIB   /usr/include/newlib)

# Microcontroller Settings
set(LDSCRIPT ${CMAKE_SOURCE_DIR}/ld/ch32v203x6.ld)
set(CPUARCH  -march=rv32imac -mabi=ilp32)

# Sources
file(GLOB CFILES CONFIGURE_DEPENDS ${SOURCE}/*.c ${SOURCE}/*.S)

add_executable(${TARGET} ${CFILES})
set_target_properties(${TARGET} PROPERTIES SUFFIX ".elf")

# Compiler Flags
target_compile_definitions(${TARGET} PRIVATE F_CPU=144000000)
target_include_directories(${TARGET} PRIVATE ${NEWLIB} ${SOURCE} ${CMAKE_SOURCE_DIR})
target_compile_options(${TARGET} PRIVATE
  -g -Os -flto -ffunction-sections -fdata-sections -fno-builtin -nostdlib
  ${CPUARCH} -Wall
)

# Linker Flags
target_link_options(${TARGET} PRIVATE
  -flto -nostdlib ${CPUARCH}
  -T${LDSCRIPT} -Wl,--gc-sections,--build-id=none
)
target_link_libraries(${TARGET} PRIVATE gcc)
set_target_properties(${TARGET} PROPERTIES LINK_DEPENDS ${LDSCRIPT})

# Build .bin/.hex/.asm/.lst/.map and print size after linking
add_custom_command(TARGET ${TARGET} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
  COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET}> ${TARGET}.hex
  COMMAND ${CMAKE_OBJDUMP} -d $<TARGET_FILE:${TARGET}> > ${TARGET}.asm
  COMMAND ${CMAKE_OBJDUMP} -S $<TARGET_FILE:${TARGET}> > ${TARGET}.lst
  COMMAND ${CMAKE_OBJDUMP} -t $<TARGET_FILE:${TARGET}> > ${TARGET}.map
  COMMAND ${CMAKE_SIZE} -d $<TARGET_FILE:${TARGET}>
  BYPRODUCTS ${TARGET}.bin ${TARGET}.hex ${TARGET}.asm ${TARGET}.lst ${TARGET}.map
  VERBATIM
)