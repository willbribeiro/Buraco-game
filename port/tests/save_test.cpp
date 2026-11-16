// Save-file naming and player-name validation.
#include "save.h"

#include "test_framework.hpp"
#include "test_helpers.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace save;

namespace {

// Restores the discovery override even when a CHECK fails early.
struct DirGuard {
    std::string previous;
    explicit DirGuard(const fs::path& dir) : previous(dirOverride()) {
        dirOverride() = dir.string();
    }
    ~DirGuard() { dirOverride() = previous; }
};

}  // namespace

TEST_CASE("sanitizeName keeps ordinary names") {
    CHECK_EQ(sanitizeName("Ana"), std::string("Ana"));
    CHECK_EQ(sanitizeName("Jose Maria"), std::string("Jose Maria"));
    CHECK_EQ(sanitizeName("voce_2"), std::string("voce_2"));
}

TEST_CASE("sanitizeName replaces characters Windows forbids") {
    CHECK_EQ(sanitizeName("a<b>c:d\"e/f\\g|h?i*j"), std::string("a_b_c_d_e_f_g_h_i_j"));
    CHECK_EQ(sanitizeName("x\ty"), std::string("xy"));  // control characters are dropped
}

TEST_CASE("sanitizeName trims dots and spaces") {
    CHECK_EQ(sanitizeName("  Ana  "), std::string("Ana"));
    CHECK_EQ(sanitizeName("...Ana..."), std::string("Ana"));
    CHECK_EQ(sanitizeName(" . "), std::string("JOGADOR"));
}

TEST_CASE("sanitizeName falls back to JOGADOR") {
    CHECK_EQ(sanitizeName(""), std::string("JOGADOR"));
    CHECK_EQ(sanitizeName("///"), std::string("___"));
    CHECK_EQ(sanitizeName("<>"), std::string("__"));
}

TEST_CASE("sanitizeName protects Windows device names") {
    CHECK_EQ(sanitizeName("CON"), std::string("CON_"));
    CHECK_EQ(sanitizeName("con"), std::string("con_"));
    CHECK_EQ(sanitizeName("NUL"), std::string("NUL_"));
    CHECK_EQ(sanitizeName("COM1"), std::string("COM1_"));
    CHECK_EQ(sanitizeName("lpt9"), std::string("lpt9_"));
    // Only the reserved forms are renamed.
    CHECK_EQ(sanitizeName("COM0"), std::string("COM0"));
    CHECK_EQ(sanitizeName("COM10"), std::string("COM10"));
    CHECK_EQ(sanitizeName("CONSOLE"), std::string("CONSOLE"));
}

TEST_CASE("sanitizeName protects device names that carry an extension") {
    CHECK_EQ(sanitizeName("CON.txt"), std::string("CON_.txt"));
    CHECK_EQ(sanitizeName("com1.log"), std::string("com1_.log"));
    CHECK_EQ(sanitizeName("NUL.tar.gz"), std::string("NUL_.tar.gz"));
    CHECK_EQ(sanitizeName("LPT9.sav"), std::string("LPT9_.sav"));
    // Windows ignores trailing spaces and dots before the extension.
    CHECK_EQ(sanitizeName("CON."), std::string("CON_"));
    CHECK_EQ(sanitizeName("CON .txt"), std::string("CON _.txt"));
    // Only the reserved forms are renamed.
    CHECK_EQ(sanitizeName("CONSOLE.txt"), std::string("CONSOLE.txt"));
    CHECK_EQ(sanitizeName("COM0.txt"), std::string("COM0.txt"));
}

TEST_CASE("sanitizeName caps the length at 60") {
    const std::string longName(80, 'a');
    const std::string out = sanitizeName(longName);
    CHECK_EQ(out.size(), size_t(60));
    CHECK_EQ(out, std::string(60, 'a'));
}

TEST_CASE("uniquePathForName returns the plain name when free") {
    const fs::path dir = fs::path(testutil::tempSavePath("unique_free.sav")).parent_path();
    DirGuard guard(dir);
    const fs::path expected = dir / "Ana.sav";
    std::error_code ec;
    fs::remove(expected, ec);
    CHECK_EQ(uniquePathForName("Ana"), expected.string());
}

TEST_CASE("uniquePathForName never overwrites an existing save") {
    const fs::path dir = fs::path(testutil::tempSavePath("unique_taken.sav")).parent_path();
    DirGuard guard(dir);
    std::error_code ec;
    fs::create_directories(dir, ec);

    const fs::path first = dir / "Bia.sav";
    const fs::path second = dir / "Bia2.sav";
    fs::remove(first, ec);
    fs::remove(second, ec);
    REQUIRE(testutil::writeText(first.string(), "BURACO-SAVE 1\n"));

    CHECK_EQ(uniquePathForName("Bia"), second.string());

    REQUIRE(testutil::writeText(second.string(), "BURACO-SAVE 1\n"));
    CHECK_EQ(uniquePathForName("Bia"), (dir / "Bia3.sav").string());
}

TEST_CASE("uniquePathForName sanitizes the requested name") {
    const fs::path dir = fs::path(testutil::tempSavePath("unique_bad.sav")).parent_path();
    DirGuard guard(dir);
    std::error_code ec;
    fs::remove(dir / "a_b.sav", ec);
    CHECK_EQ(uniquePathForName("a:b"), (dir / "a_b.sav").string());
}

TEST_CASE("savedPlayerName reads the seat-0 name") {
    const std::string path = testutil::tempSavePath("name_lookup.sav");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "BURACO-SAVE 1\nhand 1\nname 0 Ana\nname 1 Eu\n";
    out.close();
    CHECK_EQ(savedPlayerName(path), std::string("Ana"));
}

TEST_CASE("savedPlayerName returns empty when the name is missing") {
    const std::string path = testutil::tempSavePath("name_missing.sav");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "BURACO-SAVE 1\nhand 1\nname 1 Eu\n";
    out.close();
    CHECK_EQ(savedPlayerName(path), std::string());
}

TEST_CASE("looksLikeSave only accepts the magic line") {
    const std::string good = testutil::tempSavePath("looks_good.sav");
    const std::string bad = testutil::tempSavePath("looks_bad.sav");
    REQUIRE(testutil::writeText(good, "BURACO-SAVE 1\nhand 1\n"));
    REQUIRE(testutil::writeText(bad, "hello\nBURACO-SAVE 1\n"));
    CHECK(looksLikeSave(good));
    CHECK(!looksLikeSave(bad));
    CHECK(!looksLikeSave(testutil::tempSavePath("does_not_exist_at_all.sav")));
}

TEST_CASE("writableDirectory probes a folder and cleans up") {
    const fs::path dir = fs::path(testutil::tempSavePath("writable/dir")).parent_path();
    std::error_code ec;
    fs::create_directories(dir, ec);
    CHECK(writableDirectory(dir));

    // The probe file must not be left behind.
    for (const fs::directory_entry& entry : fs::directory_iterator(dir))
        CHECK(entry.path().extension() != ".tmp");

    // A folder that does not exist and a plain file are both unusable.
    CHECK(!writableDirectory(
        fs::path(testutil::tempSavePath("writable_missing/dir")).parent_path()));
    const std::string file = testutil::tempSavePath("writable_file.txt");
    REQUIRE(testutil::writeText(file, "x"));
    CHECK(!writableDirectory(file));
}

TEST_CASE("dir uses the override even when the folder is missing") {
    const fs::path folder =
        fs::path(testutil::tempSavePath("save_override_missing/x")).parent_path();
    std::error_code ec;
    fs::remove_all(folder, ec);
    DirGuard guard(folder);
    CHECK_EQ(dir(), folder);
}

TEST_CASE("searchDirs narrows to the --savedir override") {
    const fs::path dir = fs::path(testutil::tempSavePath("save_override_dirs/x")).parent_path();
    DirGuard guard(dir);
    const std::vector<fs::path> dirs = searchDirs();
    CHECK_EQ(dirs.size(), size_t(1));
    REQUIRE(!dirs.empty());
    CHECK_EQ(dirs.front(), dir);
}

TEST_CASE("findLatestSaveIn picks the newest save across folders") {
    const fs::path first =
        fs::path(testutil::tempSavePath("latest_first/x")).parent_path();
    const fs::path second =
        fs::path(testutil::tempSavePath("latest_second/x")).parent_path();
    std::error_code ec;
    fs::create_directories(first, ec);
    fs::create_directories(second, ec);

    const fs::path older = first / "Ana.sav";
    const fs::path newer = second / "Bia.sav";
    REQUIRE(testutil::writeText(older.string(), "BURACO-SAVE 1\nhand 1\n"));
    REQUIRE(testutil::writeText(newer.string(), "BURACO-SAVE 1\nhand 1\n"));

    const fs::file_time_type now = fs::file_time_type::clock::now();
    fs::last_write_time(older, now - std::chrono::hours(2), ec);
    fs::last_write_time(newer, now - std::chrono::hours(1), ec);

    const std::optional<fs::path> found = findLatestSaveIn({first, second});
    REQUIRE(found.has_value());
    CHECK_EQ(*found, newer);

    // The order of the folders must not matter, only the timestamp.
    fs::last_write_time(older, now, ec);
    fs::last_write_time(newer, now - std::chrono::hours(3), ec);
    const std::optional<fs::path> again = findLatestSaveIn({first, second});
    REQUIRE(again.has_value());
    CHECK_EQ(*again, older);
}

TEST_CASE("findLatestSaveIn ignores files that are not saves") {
    const fs::path dir = fs::path(testutil::tempSavePath("latest_skip/x")).parent_path();
    std::error_code ec;
    fs::create_directories(dir, ec);

    const fs::path real = dir / "real.sav";
    REQUIRE(testutil::writeText(real.string(), "BURACO-SAVE 1\nhand 1\n"));
    REQUIRE(testutil::writeText((dir / "notes.txt").string(), "BURACO-SAVE 1\n"));
    REQUIRE(testutil::writeText((dir / "fake.sav").string(), "not a save\n"));
    // A newer file that is not a save must not win.
    fs::last_write_time(real, fs::file_time_type::clock::now() - std::chrono::hours(5),
                        ec);

    const std::optional<fs::path> found = findLatestSaveIn({dir});
    REQUIRE(found.has_value());
    CHECK_EQ(*found, real);
}

TEST_CASE("findLatestSaveIn tolerates missing folders") {
    CHECK(!findLatestSaveIn({}).has_value());
    const fs::path missing =
        fs::path(testutil::tempSavePath("latest_missing/x")).parent_path();
    std::error_code ec;
    fs::remove_all(missing, ec);
    CHECK(!findLatestSaveIn({missing}).has_value());
}
