# Sedona VM and natives staged by './spro build esp32' (build/esp32/native):
# core + kits of the profile + platforms/esp32.
set(NATIVE "${CMAKE_CURRENT_LIST_DIR}/../../../../../build/esp32/native")
# re-glob on every build: the staged sources change with the profile/kits
# (CONFIGURE_DEPENDS is not allowed while IDF scans requirements in script mode)
if(CMAKE_SCRIPT_MODE_FILE)
  file(GLOB SEDONA_SRCS "${NATIVE}/*.c")
else()
  file(GLOB SEDONA_SRCS CONFIGURE_DEPENDS "${NATIVE}/*.c")
endif()
if(NOT SEDONA_SRCS)
  message(FATAL_ERROR "${NATIVE} is empty: run './spro build esp32' first")
endif()
# Linux-only parts of the svm (signals/backtrace, mlockall/SCHED_FIFO);
# svm_esp32.c provides their entry points
list(FILTER SEDONA_SRCS EXCLUDE REGEX ".*/(svm_crash|svm_rt)\\.c$")

idf_component_register(SRCS ${SEDONA_SRCS}
                       INCLUDE_DIRS "${NATIVE}"
                       REQUIRES esp_timer esp_system freertos lwip vfs log heap esp_hw_support
                                esp_driver_gpio esp_driver_ledc esp_adc
                       LDFRAGMENTS "linker.lf")

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  SEDONA_LOCAL_H SOCKET_FAMILY_INET=1 PLAT_BUILD_VERSION="1.2.29-spro-esp32s3")
# legacy Sedona C: keep the warnings the Linux build tolerates quiet
target_compile_options(${COMPONENT_LIB} PRIVATE
  -fno-strict-aliasing -fcommon
  -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function
  -Wno-format -Wno-pointer-sign -Wno-char-subscripts -Wno-implicit-fallthrough
  -Wno-maybe-uninitialized
  -Wno-error)

# svm main(): called from app_main as svm_main(argc, argv)
set_source_files_properties("${NATIVE}/main.c" PROPERTIES COMPILE_DEFINITIONS "main=svm_main")

# interpreter speed options (menuconfig -> Sedona VM)
if(CONFIG_SEDONA_COMPUTED_GOTO)
  set_source_files_properties("${NATIVE}/vm.c" PROPERTIES COMPILE_DEFINITIONS "COMPUTED_GOTO")
endif()
if(CONFIG_SEDONA_VM_O3)
  set_source_files_properties("${NATIVE}/vm.c" PROPERTIES COMPILE_OPTIONS "-O3")
endif()

# relative file names -> CONFIG_SEDONA_HOME (svm_esp32.c)
foreach(fn fopen rename remove unlink stat open mkdir chdir)
  target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${fn}")
endforeach()
