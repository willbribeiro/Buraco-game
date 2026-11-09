// Buraco entry point.
//
//   buraco.exe                          normal SDL2 window
//   buraco.exe --script k.txt --out s.txt
//                                       headless: replay key tokens, dump the
//                                       80x25 text screen after every step
//   buraco.exe --auto N --out s.txt     self-play N steps and dump the screen
//   buraco.exe --check N --out s.txt    self-play N steps, checking invariants
//   buraco.exe --savetest N --out s.txt save/load round-trip and divergence test
//
// Presentation lives in ui.cpp/video.cpp/renderer.cpp, save-file discovery in
// save.cpp and the machine-readable dumps in report.cpp.
#include <SDL.h>

#include "game.h"
#include "renderer.h"
#include "report.h"
#include "save.h"
#include "ui.h"
#include "video.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <random>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Scripted key names (used by the headless regression mode).
// ---------------------------------------------------------------------------
static SDL_Keycode keyFromToken(const std::string& t) {
    if (t == "ENTER" || t == "RETURN") return SDLK_RETURN;
    if (t == "SPACE") return SDLK_SPACE;
    if (t == "LEFT") return SDLK_LEFT;
    if (t == "RIGHT") return SDLK_RIGHT;
    if (t == "UP") return SDLK_UP;
    if (t == "DOWN") return SDLK_DOWN;
    if (t == "ESC" || t == "ESCAPE") return SDLK_ESCAPE;
    if (t == "F1") return SDLK_F1;
    if (t == "F2") return SDLK_F2;
    if (t == "F3") return SDLK_F3;
    if (t == "F4") return SDLK_F4;
    if (t == "F5") return SDLK_F5;
    if (t == "PGUP" || t == "PAGEUP") return SDLK_PAGEUP;
    if (t == "Y") return SDLK_y;
    if (t == "S" || t == "SIM") return SDLK_s;
    if (t == "N" || t == "NAO") return SDLK_n;
    if (t.size() == 1) return SDL_Keycode(std::tolower(uint8_t(t[0])));
    return 0;
}

static void dumpScreen(const dos::Video& v, std::FILE* f, const char* label) {
    std::fprintf(f, "# %s\n", label);
    for (int r = 0; r < dos::TEXT_ROWS; ++r) {
        for (int c = 0; c < dos::TEXT_COLS; ++c)
            std::fputc(v.text[(r * dos::TEXT_COLS + c) * 2], f);
        std::fputc('\n', f);
    }
}

static void dumpState(const buraco::Game& g, std::FILE* f) {
    const std::string s = report::stateLine(g);
    std::fwrite(s.data(), 1, s.size(), f);
}

// Fresh 32-bit seed for a new match.  SDL_GetTicks() is nearly constant at
// startup (the tick counter only starts when SDL initialises), which used to
// make every deal identical; mix the wall clock and random_device instead.
static uint32_t makeSeed() {
    const uint64_t t =
        uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::random_device rd;
    uint64_t v = t ^ (uint64_t(rd()) << 32) ^ uint64_t(rd());
    v += 0x9E3779B97F4A7C15ull;   // splitmix64 finaliser
    v = (v ^ (v >> 30)) * 0xBF58476D1CE4E5B9ull;
    v = (v ^ (v >> 27)) * 0x94D049BB133111EBull;
    v ^= v >> 31;
    const uint32_t s = uint32_t(v ^ (v >> 32));
    return s ? s : 0x9E3779B9u;
}

int main(int argc, char** argv) {
    std::string scriptPath;
    std::string outPath = "screen.txt";
    uint32_t seed = 0;
    int autoSteps = 0;
    int checkSteps = 0;
    int saveTestSteps = 0;
    bool guiAfterAuto = false;
    bool savePathSet = false;
    bool flowTest = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--script" && i + 1 < argc) scriptPath = argv[++i];
        else if (a == "--out" && i + 1 < argc) outPath = argv[++i];
        else if (a == "--seed" && i + 1 < argc) seed = uint32_t(std::strtoul(argv[++i], nullptr, 10));
        else if (a == "--auto" && i + 1 < argc) autoSteps = std::atoi(argv[++i]);
        else if (a == "--check" && i + 1 < argc) checkSteps = std::atoi(argv[++i]);
        else if (a == "--savetest" && i + 1 < argc) saveTestSteps = std::atoi(argv[++i]);
        else if (a == "--save" && i + 1 < argc) {
            save::path() = argv[++i];
            savePathSet = true;
            const std::filesystem::path sp(save::path());
            save::dirOverride() =
                sp.has_parent_path() ? sp.parent_path().string() : std::string(".");
        } else if (a == "--savedir" && i + 1 < argc) {
            save::dirOverride() = argv[++i];
        } else if (a == "--flow") {
            flowTest = true;
        } else if (a == "--gui") {
            guiAfterAuto = true;
        }
    }
    bool headless = (!scriptPath.empty() || autoSteps > 0) && !guiAfterAuto;
    // Every batch mode (script, auto, check, savetest) keeps the historical
    // default seed so failures are reproducible; only the windowed game picks
    // a fresh seed per match.
    const bool batch = headless || checkSteps > 0 || saveTestSteps > 0;

    dos::Video video;

    // `--auto ... --gui` is a screenshot/demo mode: it skips the startup flow
    // just like the headless modes.  A plain interactive run starts on the
    // intro screen and only deals after the player picks "continue" or types
    // a name.
    const bool demo = guiAfterAuto && autoSteps > 0;
    // --flow lets a headless --script replay exercise the intro/name/continue
    // screens without opening a window.
    const bool flowMode = flowTest && !scriptPath.empty() && autoSteps == 0;
    const bool interactiveStart = flowMode || (!batch && !demo);

    buraco::Game game;
    ui::UiState ui;
    if (interactiveStart) {
        ui.screen = ui::UiState::Screen::Intro;
    } else {
        game.newMatch("JOGADOR", seed ? seed : (batch ? 1234u : makeSeed()));
    }
    bool started = false;
    bool running = true;

    auto resetSelection = [&]() {
        ui.selected.assign(game.player(0).hand.size(), false);
        ui.cursor = std::clamp(ui.cursor, 0, std::max(0, int(game.player(0).hand.size()) - 1));
        ui.layChoice = false;
        ui.layTarget = -1;
        ui.layCards.clear();
    };

    auto key = [&](SDL_Keycode sym) {
        using buraco::Phase;
        using Screen = ui::UiState::Screen;

        // ---- startup flow -------------------------------------------------
        if (ui.screen == Screen::Intro) {
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                const std::optional<std::filesystem::path> found = save::findLatestSave();
                if (found) {
                    ui.promptSave = found->string();
                    ui.promptName = save::savedPlayerName(*found);
                    ui.screen = Screen::ContinuePrompt;
                } else {
                    ui.inputName.clear();
                    ui.inputError.clear();
                    ui.screen = Screen::NameEntry;
                }
            } else if (sym == SDLK_ESCAPE) {
                running = false;
            }
            return;
        }

        if (ui.screen == Screen::ContinuePrompt) {
            if (sym == SDLK_s || sym == SDLK_y || sym == SDLK_RETURN ||
                sym == SDLK_KP_ENTER) {
                if (game.loadFrom(ui.promptSave)) {
                    save::path() = ui.promptSave;
                    started = true;
                    ui.help = false;
                    ui.status = false;
                    ui.screen = Screen::Game;
                    resetSelection();
                    game.setMessage("Jogo restaurado.");
                } else {
                    ui.inputName.clear();
                    ui.inputError = "Nao consegui ler o jogo gravado. Digite um nome.";
                    ui.screen = Screen::NameEntry;
                }
            } else if (sym == SDLK_n) {
                ui.inputName.clear();
                ui.inputError.clear();
                ui.screen = Screen::NameEntry;
            } else if (sym == SDLK_ESCAPE) {
                running = false;
            }
            return;
        }

        if (ui.screen == Screen::NameEntry) {
            if (sym == SDLK_BACKSPACE) {
                if (!ui.inputName.empty()) ui.inputName.pop_back();
            } else if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                if (ui.inputName.empty()) {
                    ui.inputError = "Digite um nome antes de continuar.";
                    return;
                }
                const std::string name = ui.inputName;
                if (!savePathSet) save::path() = save::uniquePathForName(name);
                game.newMatch(name, seed ? seed : makeSeed());
                started = true;
                ui.help = false;
                ui.status = false;
                ui.screen = Screen::Game;
                ui.inputError.clear();
                resetSelection();
            } else if (sym == SDLK_ESCAPE) {
                ui.inputName.clear();
                ui.inputError.clear();
                ui.screen = Screen::Intro;
            } else if (batch && sym >= 32 && sym < 127) {
                if (ui.inputName.size() < 40) ui.inputName += char(sym);
            }
            return;
        }

        // ---- normal game --------------------------------------------------
        // Persistence works in every screen, including before the deal, so a
        // match can be restored without starting a fresh one.
        if (sym == SDLK_F2) {
            if (game.saveTo(save::path()))
                game.setMessage("Jogo gravado em " + save::path() + ".");
            else
                game.setMessage("Nao consegui gravar o jogo.");
            return;
        }
        if (sym == SDLK_F5) {
            if (game.loadFrom(save::path())) {
                started = true;
                ui.help = false;
                ui.status = false;
                resetSelection();
                game.setMessage("Jogo restaurado.");
            } else {
                game.setMessage("Nao consegui restaurar o jogo.");
            }
            return;
        }
        if (!started) {
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                started = true;
                game.newHand();
                resetSelection();
            }
            return;
        }
        if (ui.help) {
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_F1 ||
                sym == SDLK_ESCAPE)
                ui.help = false;
            return;
        }
        if (ui.status) {
            // F3 is a full screen ending in "...agora aperte ENTER".
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_F3 ||
                sym == SDLK_F4 || sym == SDLK_ESCAPE)
                ui.status = false;
            return;
        }

        const int handSize = int(game.player(0).hand.size());

        switch (sym) {
            case SDLK_ESCAPE:
                if (std::any_of(ui.selected.begin(), ui.selected.end(),
                                [](bool b) { return b; })) {
                    std::fill(ui.selected.begin(), ui.selected.end(), false);
                } else {
                    running = false;
                }
                return;
            case SDLK_F1:
                ui.help = true;
                return;
            case SDLK_F3:
                ui.status = !ui.status;
                return;
            case SDLK_F4:
                ui.status = false;
                return;
            default:
                break;
        }

        if (game.phase() == Phase::HandOver) {
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                game.continueAfterHand();
                resetSelection();
            }
            return;
        }
        if (game.phase() == Phase::MatchOver) {
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                const std::string name =
                    game.player(0).name.empty() ? "JOGADOR" : game.player(0).name;
                game.newMatch(name, seed ? seed : makeSeed());
                resetSelection();
            }
            return;
        }
        if (game.current() != 0) return;  // the AI is playing

        // "Grow the existing meld or lay a new one?" question.
        if (ui.layChoice) {
            if (sym == SDLK_a || sym == SDLK_s || sym == SDLK_y) {
                const std::string err = game.layMeld(ui.layCards, ui.layTarget);
                if (!err.empty()) game.setMessage(err);
                resetSelection();
            } else if (sym == SDLK_n) {
                const std::string err = game.layMeld(ui.layCards, buraco::Game::kNewMeld);
                if (!err.empty()) game.setMessage(err);
                resetSelection();
            } else if (sym == SDLK_ESCAPE) {
                // Keep the cards selected so the player can adjust them.
                ui.layChoice = false;
                ui.layTarget = -1;
                ui.layCards.clear();
                game.announceTurn();
            }
            return;
        }

        switch (sym) {
            case SDLK_LEFT:
                ui.cursor = std::max(0, ui.cursor - 1);
                return;
            case SDLK_RIGHT:
                ui.cursor = std::min(std::max(0, handSize - 1), ui.cursor + 1);
                return;
            case SDLK_UP:
            case SDLK_DOWN: {
                const int rows = ui::ROW_RULE3 - ui::ROW_MELD;
                int count = 0;
                for (int seat = 0; seat < 2; ++seat) {
                    int n = 0;
                    for (const auto& m : game.player(seat).melds)
                        if (!m.empty()) ++n;
                    count = std::max(count, n);
                }
                const int maxScroll = std::max(0, count - rows);
                ui.meldScroll = std::clamp(ui.meldScroll + (sym == SDLK_DOWN ? 1 : -1), 0,
                                           maxScroll);
                return;
            }
            case SDLK_SPACE:
                if (ui.cursor >= 0 && ui.cursor < int(ui.selected.size()))
                    ui.selected[ui.cursor] = !ui.selected[ui.cursor];
                return;
            case SDLK_s:
            case SDLK_y:
                if (game.phase() == Phase::Draw) {
                    game.draw(true);
                    resetSelection();
                }
                return;
            case SDLK_n:
                if (game.phase() == Phase::Draw) {
                    game.draw(false);
                    resetSelection();
                }
                return;
            case SDLK_RETURN:
            case SDLK_KP_ENTER: {
                if (game.phase() == Phase::Draw) {
                    game.draw(false);
                    resetSelection();
                    return;
                }
                if (game.phase() == Phase::Play) {
                    std::vector<buraco::Card> chosen;
                    for (size_t i = 0; i < ui.selected.size() && i < game.player(0).hand.size();
                         ++i)
                        if (ui.selected[i]) chosen.push_back(game.player(0).hand[i]);
                    if (chosen.empty()) {
                        game.requestDiscard();
                        return;
                    }
                    const buraco::Game::LayOptions opts = game.layOptions(chosen);
                    if (opts.canCreate && !opts.targets.empty()) {
                        // Both readings are legal: let the player decide.
                        ui.layChoice = true;
                        ui.layTarget = opts.targets.front();
                        ui.layCards = chosen;
                        game.setMessage("Aumentar o jogo (A) ou baixar um novo (N)?");
                        return;
                    }
                    const int target = !opts.targets.empty()
                                           ? opts.targets.front()
                                           : (opts.canCreate ? buraco::Game::kNewMeld : -1);
                    const std::string err = game.layMeld(chosen, target);
                    if (!err.empty()) game.setMessage(err);
                    resetSelection();
                    return;
                }
                if (game.phase() == Phase::Discard) {
                    const auto& hand = game.player(0).hand;
                    if (hand.empty()) return;
                    const int idx = std::clamp(ui.cursor, 0, int(hand.size()) - 1);
                    const buraco::Card card = hand[idx];
                    game.discardCard(card);
                    resetSelection();
                    return;
                }
                return;
            }
            default:
                return;
        }
    };

    // ---- self-play for a demo / screenshot, then hand over to the window ----
    if (guiAfterAuto && autoSteps > 0) {
        resetSelection();
        for (int i = 0; i < autoSteps; ++i) {
            if (game.matchOver()) break;
            if (game.phase() == buraco::Phase::HandOver) {
                game.continueAfterHand();
                continue;
            }
            game.autoStep();
        }
        started = true;
        game.announceTurn();
    }

    // ---- save/restore round-trip ------------------------------------------
    if (saveTestSteps > 0) {
        std::FILE* out = std::fopen(outPath.c_str(), "wb");
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
            return 1;
        }
        auto advance = [](buraco::Game& g, int steps) {
            for (int i = 0; i < steps; ++i) {
                if (g.matchOver()) break;
                if (g.phase() == buraco::Phase::HandOver) {
                    g.continueAfterHand();
                    continue;
                }
                g.autoStep();
            }
        };

        buraco::Game a;
        a.newMatch("JOGADOR", seed ? seed : 1234u);
        advance(a, saveTestSteps);
        const std::string before = report::strictLine(a);
        bool ok = a.saveTo(save::path());
        buraco::Game b;
        ok = ok && b.loadFrom(save::path());
        ok = ok && report::strictLine(b) == before;
        std::string detail = "roundtrip";

        // A faithful load must serialise back to exactly the same file: this
        // catches fields that saveTo writes but loadFrom forgets.
        if (ok) {
            b.saveTo(outPath + ".reload.sav");
            std::ifstream f1(save::path(), std::ios::binary);
            std::ifstream f2(outPath + ".reload.sav", std::ios::binary);
            const std::string s1((std::istreambuf_iterator<char>(f1)),
                                 std::istreambuf_iterator<char>());
            const std::string s2((std::istreambuf_iterator<char>(f2)),
                                 std::istreambuf_iterator<char>());
            if (s1 != s2) {
                ok = false;
                detail = "save/load is lossy";
            }
        }

        if (ok) {
            advance(a, 400);
            advance(b, 400);
            const std::string la = report::strictLine(a);
            const std::string lb = report::strictLine(b);
            if (la != lb) {
                ok = false;
                detail = "diverged after 400 more steps";
                // Point at the first visible difference; the two states are
                // saved next to the report so the split can be reproduced.
                size_t d = 0;
                while (d < la.size() && d < lb.size() && la[d] == lb[d]) ++d;
                const size_t from = d > 60 ? d - 60 : 0;
                std::fprintf(out, "first diff at column %zu\n", d);
                std::fprintf(out, "A ...%s\n", la.c_str() + from);
                std::fprintf(out, "B ...%s\n", lb.c_str() + from);
                a.saveTo(outPath + ".a.sav");
                b.saveTo(outPath + ".b.sav");
            }
        } else if (detail == "roundtrip") {
            ok = false;
            detail = "save/load state mismatch";
        }
        std::fprintf(out, "%s savetest steps=%d file=%s %s\n", ok ? "OK" : "FAIL",
                     saveTestSteps, save::path().c_str(), detail.c_str());
        std::fclose(out);
        return ok ? 0 : 2;
    }

    // ---- invariant self-check ---------------------------------------------
    if (checkSteps > 0) {
        std::FILE* out = std::fopen(outPath.c_str(), "wb");
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
            return 1;
        }
        std::string err;
        int steps = 0, hands = 0;
        // A deal that keeps buying a one-card table and discarding it back
        // never ends.  Remember recent states so the self-check reports the
        // loop instead of silently burning the whole step budget.
        std::deque<size_t> recent;
        for (; steps < checkSteps && !game.matchOver(); ++steps) {
            if (game.phase() == buraco::Phase::HandOver) {
                ++hands;
                std::fprintf(out, "# hand %d scored at step %d: p0 %d p1 %d\n", hands, steps,
                             game.player(0).lastHandPoints, game.player(1).lastHandPoints);
                game.continueAfterHand();
                recent.clear();
                continue;
            }
            game.autoStep();
            // Scoring hands the morto cards out of the accounting (endHand
            // clears them), so only check while a hand is actually in play.
            if (game.phase() == buraco::Phase::HandOver ||
                game.phase() == buraco::Phase::MatchOver)
                continue;
            if (!report::checkInvariants(game, err)) {
                std::fprintf(out, "FAIL at step %d (hand %d): %s\n%s", steps, hands + 1,
                             err.c_str(), report::stateLine(game).c_str());
                game.saveTo(outPath + ".sav");
                std::fclose(out);
                return 2;
            }
            const std::string st = report::stateLine(game);
            const size_t h = std::hash<std::string>{}(st);
            if (std::find(recent.begin(), recent.end(), h) != recent.end()) {
                std::fprintf(out, "FAIL at step %d (hand %d): stalled deal (state repeats)\n%s",
                             steps, hands + 1, st.c_str());
                game.saveTo(outPath + ".sav");
                std::fclose(out);
                return 2;
            }
            recent.push_back(h);
            if (recent.size() > 256) recent.pop_front();
        }
        std::fprintf(out, "OK steps=%d hands=%d p0=%d p1=%d\n", steps, hands,
                     game.player(0).total, game.player(1).total);
        std::fclose(out);
        return 0;
    }

    // ---- headless replay --------------------------------------------------
    if (headless) {
        std::FILE* out = std::fopen(outPath.c_str(), "wb");
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
            return 1;
        }

        // Self-play: both seats driven by the same rule engine, used to check
        // deals, the morto, going out and the scoring without a window.
        if (autoSteps > 0) {
            resetSelection();
            int hands = 0;
            for (int i = 0; i < autoSteps;) {
                if (game.matchOver()) break;
                if (game.phase() == buraco::Phase::HandOver) {
                    std::fprintf(out, "# hand %d over: p0 %d p1 %d\n", hands + 1,
                                 game.player(0).lastHandPoints, game.player(1).lastHandPoints);
                    dumpState(game, out);
                    game.continueAfterHand();
                    ++hands;
                    continue;
                }
                game.autoStep();
                ++i;
            }
            ui::draw(video, game, ui);
            dumpScreen(video, out, "auto");
            dumpState(game, out);
            std::fclose(out);
            return 0;
        }

        std::FILE* in = std::fopen(scriptPath.c_str(), "rb");
        if (!in) {
            std::fclose(out);
            std::fprintf(stderr, "cannot open script %s\n", scriptPath.c_str());
            return 1;
        }

        resetSelection();
        ui::draw(video, game, ui);
        dumpScreen(video, out, "start");
        char line[128];
        int step = 0;
        while (std::fgets(line, sizeof line, in)) {
            std::string tok(line);
            while (!tok.empty() && (tok.back() == '\n' || tok.back() == '\r' || tok.back() == ' '))
                tok.pop_back();
            while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
            if (tok.empty() || tok[0] == '#') continue;

            const SDL_Keycode k = keyFromToken(tok);
            if (!k) {
                std::fprintf(out, "# unknown token '%s'\n", tok.c_str());
                continue;
            }
            key(k);

            // Let the AI play until the turn comes back to the human.
            for (int guard = 0; guard < 40; ++guard) {
                using buraco::Phase;
                if (game.phase() == Phase::HandOver || game.phase() == Phase::MatchOver) break;
                if (game.current() == 0) break;
                game.update(0.75);
            }
            if (started && game.current() == 0 &&
                int(ui.selected.size()) != int(game.player(0).hand.size()))
                resetSelection();

            ui::draw(video, game, ui);
            char label[160];
            std::snprintf(label, sizeof label, "step %d: %s", ++step, tok.c_str());
            dumpScreen(video, out, label);
            dumpState(game, out);
        }
        std::fclose(in);
        std::fclose(out);
        return 0;
    }

    // ---- interactive window ----------------------------------------------
    dos::Renderer renderer;
    if (!renderer.init("Buraco", 2)) {
        std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!guiAfterAuto) game.setMessage("Embaralhando. Por favor, corte... <ENTER>");
    resetSelection();

    uint64_t last = SDL_GetTicks64();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                key(e.key.keysym.sym);
            } else if (e.type == SDL_TEXTINPUT &&
                       ui.screen == ui::UiState::Screen::NameEntry) {
                for (const char* p = e.text.text; *p; ++p) {
                    const unsigned char c = static_cast<unsigned char>(*p);
                    if (c >= 32 && c != 127 && ui.inputName.size() < 40)
                        ui.inputName += char(c);
                }
            }
        }

        const uint64_t now = SDL_GetTicks64();
        const double dt = double(now - last) / 1000.0;
        last = now;

        game.update(dt);
        if (started && game.current() == 0) {
            if (int(ui.selected.size()) != int(game.player(0).hand.size()))
                resetSelection();
        } else {
            ui.cursor = 0;
            if (!ui.selected.empty()) std::fill(ui.selected.begin(), ui.selected.end(), false);
        }

        ui::draw(video, game, ui);
        renderer.present(video, double(now) / 1000.0);
        SDL_Delay(8);
    }
    return 0;
}
