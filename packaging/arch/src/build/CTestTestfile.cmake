# CMake generated Testfile for 
# Source directory: /home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0
# Build directory: /home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(game "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/test_game")
set_tests_properties(game PROPERTIES  _BACKTRACE_TRIPLES "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;35;add_test;/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;0;")
add_test(audio "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/test_audio")
set_tests_properties(audio PROPERTIES  _BACKTRACE_TRIPLES "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;39;add_test;/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;0;")
add_test(music "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/test_music")
set_tests_properties(music PROPERTIES  _BACKTRACE_TRIPLES "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;43;add_test;/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;0;")
add_test(sense "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/test_sense")
set_tests_properties(sense PROPERTIES  _BACKTRACE_TRIPLES "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;47;add_test;/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;0;")
add_test(errors "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/test_errors")
set_tests_properties(errors PROPERTIES  _BACKTRACE_TRIPLES "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;50;add_test;/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;0;")
add_test(cast "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/test_cast")
set_tests_properties(cast PROPERTIES  _BACKTRACE_TRIPLES "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;54;add_test;/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;0;")
add_test(selftest "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/shooter-upper" "--selftest" "--config-dir" "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/selftest-cfg")
set_tests_properties(selftest PROPERTIES  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy" _BACKTRACE_TRIPLES "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;56;add_test;/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;0;")
add_test(smoke "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/shooter-upper" "--smoke" "7200" "--config-dir" "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/build/smoke-cfg")
set_tests_properties(smoke PROPERTIES  _BACKTRACE_TRIPLES "/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;58;add_test;/home/murtsu/shooter-upper-github/shooter-upper/packaging/arch/src/shooter-upper-1.0.0/CMakeLists.txt;0;")
