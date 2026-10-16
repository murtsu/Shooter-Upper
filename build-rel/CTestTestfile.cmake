# CMake generated Testfile for 
# Source directory: /home/claude/shooter-upper
# Build directory: /home/claude/shooter-upper/build-rel
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(game "/home/claude/shooter-upper/build-rel/test_game")
set_tests_properties(game PROPERTIES  _BACKTRACE_TRIPLES "/home/claude/shooter-upper/CMakeLists.txt;35;add_test;/home/claude/shooter-upper/CMakeLists.txt;0;")
add_test(audio "/home/claude/shooter-upper/build-rel/test_audio")
set_tests_properties(audio PROPERTIES  _BACKTRACE_TRIPLES "/home/claude/shooter-upper/CMakeLists.txt;39;add_test;/home/claude/shooter-upper/CMakeLists.txt;0;")
add_test(music "/home/claude/shooter-upper/build-rel/test_music")
set_tests_properties(music PROPERTIES  _BACKTRACE_TRIPLES "/home/claude/shooter-upper/CMakeLists.txt;43;add_test;/home/claude/shooter-upper/CMakeLists.txt;0;")
add_test(sense "/home/claude/shooter-upper/build-rel/test_sense")
set_tests_properties(sense PROPERTIES  _BACKTRACE_TRIPLES "/home/claude/shooter-upper/CMakeLists.txt;47;add_test;/home/claude/shooter-upper/CMakeLists.txt;0;")
add_test(errors "/home/claude/shooter-upper/build-rel/test_errors")
set_tests_properties(errors PROPERTIES  _BACKTRACE_TRIPLES "/home/claude/shooter-upper/CMakeLists.txt;50;add_test;/home/claude/shooter-upper/CMakeLists.txt;0;")
add_test(cast "/home/claude/shooter-upper/build-rel/test_cast")
set_tests_properties(cast PROPERTIES  _BACKTRACE_TRIPLES "/home/claude/shooter-upper/CMakeLists.txt;54;add_test;/home/claude/shooter-upper/CMakeLists.txt;0;")
add_test(selftest "/home/claude/shooter-upper/build-rel/shooter-upper" "--selftest" "--config-dir" "/home/claude/shooter-upper/build-rel/selftest-cfg")
set_tests_properties(selftest PROPERTIES  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy" _BACKTRACE_TRIPLES "/home/claude/shooter-upper/CMakeLists.txt;56;add_test;/home/claude/shooter-upper/CMakeLists.txt;0;")
add_test(smoke "/home/claude/shooter-upper/build-rel/shooter-upper" "--smoke" "7200" "--config-dir" "/home/claude/shooter-upper/build-rel/smoke-cfg")
set_tests_properties(smoke PROPERTIES  _BACKTRACE_TRIPLES "/home/claude/shooter-upper/CMakeLists.txt;58;add_test;/home/claude/shooter-upper/CMakeLists.txt;0;")
