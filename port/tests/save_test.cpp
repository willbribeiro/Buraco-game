// Save-file naming and player-name validation.
#include "save.h"

#include "test_framework.hpp"
#include "test_helpers.h"

#include <filesystem>
#include <fstream>
#include <string>

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
