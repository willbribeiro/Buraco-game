// Interface translations: the Portuguese default, language selection and the
// English rendering of the screens.
#include "i18n.h"

#include "config.h"
#include "game.h"
#include "options.h"
#include "ui.h"
#include "video.h"

#include "test_framework.hpp"
#include "test_helpers.h"

#include <string>

using namespace buraco;

namespace {

// Tests that switch the global language restore it on the way out, so the
// golden UI hash and every later case keep running in Portuguese.
class LanguageGuard {
public:
    LanguageGuard() : saved_(i18n::language()) {}
    ~LanguageGuard() { i18n::setLanguage(saved_); }

    LanguageGuard(const LanguageGuard&) = delete;
    LanguageGuard& operator=(const LanguageGuard&) = delete;

private:
    i18n::Language saved_;
};

std::string rowText(const dos::Video& v, int row) {
    std::string s;
    for (int c = 0; c < ui::COLS; ++c) s += char(v.text[(row * ui::COLS + c) * 2]);
    return s;
}

bool screenContains(const dos::Video& v, const std::string& needle) {
    for (int r = 0; r < ui::ROWS; ++r)
        if (rowText(v, r).find(needle) != std::string::npos) return true;
    return false;
}

}  // namespace

TEST_CASE("i18n defaults to Portuguese and keeps the original wording") {
    CHECK(i18n::language() == i18n::Language::Pt);
    CHECK_EQ(std::string(i18n::code(i18n::Language::Pt)), std::string("pt"));
    CHECK_EQ(std::string(i18n::displayName(i18n::Language::Pt)),
             std::string("Portugues"));
    CHECK_EQ(std::string(i18n::text(i18n::Text::DefaultPlayerName)),
             std::string("Voce"));
    CHECK_EQ(std::string(i18n::text(i18n::Text::HelpTitle)),
             std::string("BURACO - ajuda"));
    CHECK_EQ(std::string(i18n::text(i18n::Text::HelpOptions)),
             std::string("Para ver as opcoes do jogo, aperte F9."));
    CHECK_EQ(std::string(i18n::text(i18n::Text::MsgBuyTableQuestion)),
             std::string("Comprar a mesa? (S/N)"));

    // The F3 line keeps the CP437 0xA0 ("já") and the historical
    // "e eu ganhou" wording of the original game.
    const std::string matches = i18n::format(i18n::Text::StatusMatches, 3, 2, 1);
    CHECK_EQ(matches,
             std::string("nos j\xA0 3 partidas das quais voce ganhou 2 e eu ganhou 1."));
}

TEST_CASE("i18n parses both language spellings and rejects the rest") {
    i18n::Language lang = i18n::Language::En;

    CHECK(i18n::parseLanguage("pt", lang));
    CHECK(lang == i18n::Language::Pt);
    CHECK(i18n::parseLanguage("PT", lang));
    CHECK(lang == i18n::Language::Pt);
    CHECK(i18n::parseLanguage(" Portugues ", lang));
    CHECK(lang == i18n::Language::Pt);

    CHECK(i18n::parseLanguage("en", lang));
    CHECK(lang == i18n::Language::En);
    CHECK(i18n::parseLanguage("ENGLISH", lang));
    CHECK(lang == i18n::Language::En);
    CHECK(i18n::parseLanguage(" ingles ", lang));
    CHECK(lang == i18n::Language::En);

    CHECK(!i18n::parseLanguage("", lang));
    CHECK(!i18n::parseLanguage("fr", lang));
    CHECK(!i18n::parseLanguage("englishx", lang));
}

TEST_CASE("i18n toggles, validates and switches the global language") {
    LanguageGuard guard;
    i18n::setLanguage(i18n::Language::Pt);
    CHECK(i18n::valid(i18n::Language::Pt));
    CHECK(i18n::valid(i18n::Language::En));
    CHECK(!i18n::valid(static_cast<i18n::Language>(7)));
    CHECK(i18n::toggled(i18n::Language::Pt) == i18n::Language::En);
    CHECK(i18n::toggled(i18n::Language::En) == i18n::Language::Pt);

    i18n::setLanguage(i18n::Language::En);
    CHECK(i18n::language() == i18n::Language::En);
    CHECK_EQ(std::string(i18n::code(i18n::Language::En)), std::string("en"));
    CHECK_EQ(std::string(i18n::text(i18n::Text::DefaultPlayerName)),
             std::string("You"));
    CHECK_EQ(std::string(i18n::text(i18n::Text::HelpTitle)),
             std::string("BURACO - help"));
    CHECK_EQ(i18n::format(i18n::Text::StatusMatches, 3, 2, 1),
             std::string("in 3 matches, of which you won 2 and I won 1."));

    // An invalid value falls back to Portuguese instead of leaving the game
    // in an unknown language.
    i18n::setLanguage(static_cast<i18n::Language>(7));
    CHECK(i18n::language() == i18n::Language::Pt);
}

TEST_CASE("English translates the game table, the intro and F3") {
    LanguageGuard guard;
    i18n::setLanguage(i18n::Language::En);

    Game g;
    g.newMatch("Ana", 7u);
    dos::Video v;
    ui::UiState state;
    ui::draw(v, g, state);
    CHECK(rowText(v, ui::ROW_MSG).find("Draw a card from the stock") !=
          std::string::npos);
    CHECK(rowText(v, ui::ROW_LABEL).find("Your hand.") != std::string::npos);
    CHECK(rowText(v, ui::ROW_RULE2 + 1).find("YOUR MELDS") != std::string::npos);
    CHECK(rowText(v, ui::ROW_RULE2 + 1).find("MY MELDS") != std::string::npos);

    dos::Video help;
    ui::UiState intro;
    intro.screen = ui::UiState::Screen::Intro;
    ui::draw(help, g, intro);
    CHECK(rowText(help, 0).rfind("BURACO - help", 0) == 0);
    CHECK(screenContains(help, "Shall we play buraco?"));
    CHECK(screenContains(help, "To see the game options, press F9."));
    CHECK(screenContains(help, "Good luck!"));

    dos::Video status;
    ui::UiState statusState;
    statusState.status = true;
    ui::draw(status, g, statusState);
    CHECK(rowText(status, 0).rfind("BURACO - match status", 0) == 0);
    CHECK(screenContains(status, "in 0 matches, of which you won 0 and I won 0."));
    CHECK(screenContains(status, "There are"));
}

TEST_CASE("English translates the scoring screen") {
    LanguageGuard guard;
    i18n::setLanguage(i18n::Language::En);

    Game g;
    const std::string text = testutil::saveHeader(int(Phase::HandOver)) +
                             testutil::playerLine(0, {}, true, 130, 130) +
                             testutil::playerLine(1, {53}, false, -20, 0);
    REQUIRE(testutil::loadText(g, text, "ui_score_en.sav"));

    dos::Video v;
    ui::UiState state;
    ui::draw(v, g, state);
    CHECK(rowText(v, 0).find("SCORING   hand 1") != std::string::npos);
    CHECK(rowText(v, 18).find("Melds") != std::string::npos);
    CHECK(rowText(v, 20).find("Cards in hand") != std::string::npos);
    CHECK(rowText(v, 22).find("Hand total") != std::string::npos);
    CHECK(rowText(v, 24).find("ENTER to continue") != std::string::npos);
}

TEST_CASE("English translates the options screen and its language row") {
    LanguageGuard guard;
    i18n::setLanguage(i18n::Language::En);

    config::Settings settings;
    settings.language = i18n::Language::En;
    dos::Video v;
    ui::drawOptions(v, settings, int(options::Row::Language));
    CHECK(rowText(v, 0).rfind("BURACO - options", 0) == 0);
    CHECK(rowText(v, 8).find("Language") != std::string::npos);
    CHECK(rowText(v, 8).find("English") != std::string::npos);
    CHECK(rowText(v, 8)[2] == '>');
    CHECK(rowText(v, 9).find("Sound") != std::string::npos);
    CHECK(rowText(v, 12).find("Center window") != std::string::npos);
    CHECK(rowText(v, 16).find("Up/down select a row") != std::string::npos);
}
