# Unit Tests for Buraco Port

This directory contains unit tests for the Buraco card game port.

## Test Coverage

### cards.cpp
- `makeCard()`: Creates cards from IDs (1-108), validates deck 0 and 1 ranges
- `cardPoints()`: Returns point values (A=15, 2/10/K=10, others=5, JOKER=20)
- `isWild()`: Identifies wild cards (2s and jokers)
- `isRedSuit()`: Checks if a card is from red suits (♥♦)
- `rankGlyph()`: Gets CP437 rank glyph for display
- `suitGlyphFor()`: Gets suit character for meld labels
- `cardWidth()`: Returns display width (2 normal, 3 for ten)
- `describe()`: Returns human-readable card description

### cards.cpp (Meld tests)
- `buildMeld()`: Creates a valid meld from a selection of cards
- `buildMeldAuto()`: Auto-detects the best suit for a meld
- `extendMeld()`: Adds cards to an existing meld run
- Meld size, points, value calculations
- Canastra (7-card) bonus detection
- Low/high slot tracking

### game.cpp
- `Game::newMatch()`: Initializes a new game with seed
- `Game::newHand()`: Deals 108 cards, sets up boards
- Draw phase: `draw()` from monte or table
- Play phase: `layMeld()` for laying/growing melds
- Discard phase: `discardCard()` 
- AI turn handling via `update()` and `autoStep()`
- Persistence: `saveTo()`, `loadFrom()`

## Building Tests

### Quick Build (MinGW-w64 + CMake)
```powershell
cd port
. build.ps1
cd ..
cmake -S port -B port/build_tests -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DSDL2_DIR=work/toolchain/sdl2/SDL2-2.32.10/x86_64-w64-mingw32/lib/cmake/SDL2 ^
    -DCATCH2_ROOT="C:/Packages/Catch2/share/cmake"

cmake --build port/build_tests --config Release
```

### Alternative with vcpkg
```powershell
vcpkg install catch2 sdl2:x64-windows
cd port/tests
cmake -B build -G "Visual Studio 17 2022" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCATCH2_ROOT_PATH="C:/vcpkg/installed/x64-windows" ^
    -DSDL2_DIR="C:/vcpkg/installed/x64-windows/lib/cmake/SDL2"
cmake --build build --config Release
```

## Running Tests

### From Build Directory (MinGW)
```powershell
cd port/build_tests
Release\buraco_test.exe
```

### From Build Directory (Visual Studio)
```powershell
cd port/build_tests
.\bin\x64\Release\buraco_test.exe
```

## Test Output

Tests use Catch2 framework and report:
- ✅ Passed tests
- ❌ Failed tests with error messages
- ℹ️ Benchmark times for fast operations

Example output:
```
===============================================================================
All tests passed (45 assertions in 37 test cases)
```

Or when failures occur:
```
test/game.cpp:128: FAILED:
  bool(ok) == true
with expansion:
  false == true

test/cards.cpp:156: DYNAMICALLY GENERATED ERROR:
  Assertion failed
```

## Adding New Tests

1. Add a new `.cpp` file to `tests/` directory
2. Include it in `tests/CMakeLists.txt`:
   ```cmake
   add_executable(buraco_test 
       tests/buraco.cpp
       tests/cards.cpp
       tests/game.cpp
       tests/new_file.cpp  # Add your new test here
   )
   ```
3. Use Catch2 macros in your test file:
   ```cpp
   TEST_CASE("Your test name", "[category]") {
       // Test code here
   }
   ```

## Continuous Integration

The `port/build.ps1` script can be modified to include tests:
```powershell
cmake --build port/build --config Release --target buraco_test
.\port\build\Release\buraco_test.exe
```

## Notes

- Tests use deterministic RNG when compiled with `DETERMINISTIC_TEST` defined
- All tests are designed to pass with the current game state machine implementation
- Save/load tests use temporary files in the tests directory
- Cards use IDs 1-108 (matching original BURACO.EXE format)
