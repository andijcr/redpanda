# boosttest_discover_tests cmake function

from 
https://gitlab.kitware.com/cmake/cmake/-/merge_requests/4145
and
https://github.com/Bagira80/cmake-modules/tree/master

boosttest_discover_tests cmake function, runtime discovery of test cases

usage of this repo, build and run all the tests:
`ctest --build-and-test . build --build-generator Ninja --test-command ctest`

selecting only rpunit or rpfixture works with a minor modification

`ctest -R ".*_rpunit.*" also works`

expected output:

```
Internal cmake changing into directory: /home/andrea/Dev/test_boost_test/build
======== CMake output     ======
The C compiler identification is GNU 12.2.0
The CXX compiler identification is GNU 12.2.0
Detecting C compiler ABI info
Detecting C compiler ABI info - done
Check for working C compiler: /usr/bin/cc - skipped
Detecting C compile features
Detecting C compile features - done
Detecting CXX compiler ABI info
Detecting CXX compiler ABI info - done
Check for working CXX compiler: /usr/bin/c++ - skipped
Detecting CXX compile features
Detecting CXX compile features - done
Configuring done
Generating done
Build files have been written to: /home/andrea/Dev/test_boost_test/build
======== End CMake output ======
Change Dir: /home/andrea/Dev/test_boost_test/build

Run Clean Command:/usr/bin/ninja clean
[1/1] Cleaning all built files...
Cleaning... 0 files.

Run Build Command(s):/usr/bin/ninja && [1/4] Building CXX object CMakeFiles/test_rpfixture.dir/test_rpfixture.cpp.o
[2/4] Building CXX object CMakeFiles/test_rpunit.dir/test_rpunit.cpp.o
[3/4] Linking CXX executable test_rpfixture
[4/4] Linking CXX executable test_rpunit

Running test command: "/usr/bin/ctest"
Test command failed: /usr/bin/ctest
Test project /home/andrea/Dev/test_boost_test/build
    Start 1: test_rpunit.first
1/4 Test #1: test_rpunit.first ................   Passed    0.00 sec
    Start 2: test_rpunit.second
2/4 Test #2: test_rpunit.second ...............***Failed    0.00 sec
    Start 3: test_rpfixture.first
3/4 Test #3: test_rpfixture.first .............   Passed    0.00 sec
    Start 4: test_rpfixture.second
4/4 Test #4: test_rpfixture.second ............***Failed    0.00 sec

50% tests passed, 2 tests failed out of 4

Total Test time (real) =   0.02 sec

The following tests FAILED:
	  2 - test_rpunit.second (Failed)
	  4 - test_rpfixture.second (Failed)
Errors while running CTest
Output from these tests are in: /home/andrea/Dev/test_boost_test/build/Testing/Temporary/LastTest.log
Use "--rerun-failed --output-on-failure" to re-run the failed cases verbosely.
```
