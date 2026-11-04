#include "game.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace buraco {
namespace {

bool hasCanastra(const PlayerState& p) {
    for (const Meld& m : p.melds)
        if (!m.empty() && m.canastra()) return true;
    return false;
}

void removeCard(std::vector<Card>& hand, const Card& c) {
    for (auto it = hand.begin(); it != hand.end(); ++it) {
        if (it->id == c.id) {
            hand.erase(it);
            return;
        }
    }
}

}  // namespace

Game::Game() {
    players_[0].name = "Voce";
    players_[1].name = "Eu";
}

uint32_t Game::rnd() {
    // xorshift32 – deterministic per match, plenty for shuffling 108 cards.
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

void Game::shuffle(std::vector<Card>& deck) {
    for (size_t i = deck.size(); i > 1; --i) {
        size_t j = rnd() % i;
        std::swap(deck[i - 1], deck[j]);
    }
}

void Game::newMatch(const std::string& playerName, uint32_t seed) {
    if (!playerName.empty()) players_[0].name = playerName;
    rng_ = seed ? seed : 0x9E3779B9u;
    for (PlayerState& p : players_) {
        p.total = 0;
        p.lastHandPoints = 0;
    }
    handNumber_ = 0;
    newHand();
}

void Game::newHand() {
    for (PlayerState& p : players_) {
        p.hand.clear();
        p.melds.clear();
        p.tookMorto = false;
        p.wentOut = false;
        p.lastHandPoints = 0;
    }
    monte_.clear();
    discard_.clear();
    morto_.clear();
    lastDrawn_ = Card{};

    std::vector<Card> deck;
    deck.reserve(DECK_SIZE);
    for (uint8_t id = 1; id <= DECK_SIZE; ++id) deck.push_back(makeCard(id));
    shuffle(deck);

    for (int i = 0; i < HAND_SIZE; ++i) {
        players_[0].hand.push_back(deck.back());
        deck.pop_back();
        players_[1].hand.push_back(deck.back());
        deck.pop_back();
    }
    for (int i = 0; i < MORTO_SIZE; ++i) {
        morto_.push_back(deck.back());
        deck.pop_back();
    }
    monte_ = deck;

    ++handNumber_;
    lastWentOutSeat_ = -1;
    lastWentOutByMeld_ = false;
    current_ = (handNumber_ - 1) % 2;
    prompt_ = Prompt::None;
    setPhase(Phase::Draw);
    message_ = "Embaralhando. Por favor, corte...";
    timer_ = 0.0;
    beginTurn();
}

void Game::setPhase(Phase p) {
    phase_ = p;
    timer_ = 0.0;
}

void Game::info(const std::string& m) { message_ = m; }

bool Game::canTakeDiscard() const { return !discard_.empty(); }

void Game::requestDiscard() {
    if (phase_ != Phase::Play) return;
    setPhase(Phase::Discard);
    info("Escolha a carta a descartar.");
}

bool Game::draw(bool fromTable) {
    if (phase_ != Phase::Draw) return false;
    PlayerState& p = players_[current_];

    if (fromTable) {
        if (discard_.empty()) return false;
        // Buraco: buying the table takes the whole discard pile, not just the
        // top card.  Keeping the vector order leaves the top card at the end
        // of the hand, which is also what lastDrawn_ points at below.
        p.hand.insert(p.hand.end(), discard_.begin(), discard_.end());
        discard_.clear();
        info(current_ == 0 ? "Voce comprou a mesa." : "Estou comprando a mesa.");
    } else {
        if (monte_.empty()) {
            // The original's deck is a 108-slot array and FUN_1000_2a03 returns
            // 0 when every slot is empty: the draw simply gives no card, the
            // hand shrinks by one on the discard and the deal finally ends.
            info("O monte acabou.");
            prompt_ = Prompt::None;
            setPhase(Phase::Play);
            return false;
        }
        p.hand.push_back(monte_.back());
        monte_.pop_back();
        info(current_ == 0 ? "Voce comprou do monte." : "Estou comprando do monte.");
    }
    prompt_ = Prompt::None;
    setPhase(Phase::Play);
    lastDrawn_ = p.hand.back();
    return true;
}

Game::LayOptions Game::layOptions(const std::vector<Card>& cards) const {
    LayOptions o;
    if (phase_ != Phase::Play || cards.empty()) return o;

    const PlayerState& p = players_[current_];
    Meld built;
    o.isMeld = buildMeldAuto(cards, built);

    const bool wouldEmpty = cards.size() == p.hand.size();
    const bool blocked = wouldEmpty && p.tookMorto && !hasCanastra(p);
    o.canCreate = o.isMeld && int(p.melds.size()) < kMaxMelds && !blocked;

    // Extending is legal with any number of cards; only a brand-new meld needs
    // three.  This is what lets the player drop a single card on an existing
    // run, as in the original.
    for (size_t i = 0; i < p.melds.size(); ++i) {
        if (p.melds[i].empty()) continue;
        Meld probe = p.melds[i];
        if (extendMeld(probe, cards)) o.targets.push_back(int(i));
    }
    return o;
}

std::string Game::layMeld(const std::vector<Card>& cards, int target) {
    if (phase_ != Phase::Play) return "Agora nao e hora de baixar jogos.";
    if (cards.empty()) return "Escolha pelo menos uma carta.";

    PlayerState& p = players_[current_];
    const bool wouldEmpty = cards.size() == p.hand.size();
    if (wouldEmpty && p.tookMorto && !hasCanastra(p))
        return "Voce nao pode bater porque nao tem canastra.";

    auto takeFromHand = [&p, &cards]() {
        for (const Card& c : cards) removeCard(p.hand, c);
    };
    auto finish = [&]() {
        if (p.hand.empty()) afterEmptyHand(true);
        return std::string();
    };

    // The player explicitly asked for a second meld of a suit that is already
    // on the table.
    if (target == kNewMeld) {
        if (cards.size() < size_t(MELD_MIN)) return "Um jogo novo precisa de pelo menos 3 cartas.";
        Meld built;
        if (!buildMeldAuto(cards, built)) return "Estas cartas nao formam um jogo valido.";
        if (int(p.melds.size()) >= kMaxMelds) return "Nao cabem mais jogos na mesa.";
        takeFromHand();
        p.melds.push_back(built);
        info("Jogo baixado.");
        return finish();
    }

    // The player explicitly picked a meld to grow.
    if (target >= 0) {
        if (target >= int(p.melds.size()) || p.melds[target].empty())
            return "Jogo invalido.";
        if (!extendMeld(p.melds[target], cards)) return "Estas cartas nao entram neste jogo.";
        takeFromHand();
        info("Jogo aumentado.");
        return finish();
    }

    // Automatic mode (AI and scripted play): extend when possible, otherwise
    // lay the cards as a new meld.
    Meld built;
    if (buildMeldAuto(cards, built)) {
        for (Meld& m : p.melds) {
            if (m.empty() || m.suit != built.suit) continue;
            if (!extendMeld(m, cards)) continue;
            takeFromHand();
            info("Jogo aumentado.");
            return finish();
        }
        if (int(p.melds.size()) >= kMaxMelds) return "Nao cabem mais jogos na mesa.";
        takeFromHand();
        p.melds.push_back(built);
        info("Jogo baixado.");
        return finish();
    }

    for (Meld& m : p.melds) {
        if (m.empty()) continue;
        if (!extendMeld(m, cards)) continue;
        takeFromHand();
        info("Jogo aumentado.");
        return finish();
    }
    return "Estas cartas nao formam um jogo valido.";
}

void Game::afterEmptyHand(bool byMeld) {
    PlayerState& p = players_[current_];
    if (!p.hand.empty()) return;
    // Emptying the hand without having taken the morto is only possible while
    // there are still cards in it; otherwise the player is out.
    if (!p.tookMorto && !morto_.empty()) {
        for (int i = 0; i < MORTO_HALF && !morto_.empty(); ++i) {
            p.hand.push_back(morto_.back());
            morto_.pop_back();
        }
        p.tookMorto = true;
        info("Voce pegou o morto.");
        setPhase(Phase::Discard);
    } else {
        p.tookMorto = true;
        p.wentOut = true;
        lastWentOutSeat_ = current_;
        lastWentOutByMeld_ = byMeld;
        endHand(true, byMeld);
    }
}

bool Game::discardCard(const Card& card) {
    if (phase_ != Phase::Discard) return false;
    PlayerState& p = players_[current_];
    auto it = std::find_if(p.hand.begin(), p.hand.end(),
                           [&](const Card& c) { return c.id == card.id; });
    if (it == p.hand.end()) return false;

    p.hand.erase(it);
    discard_.push_back(card);

    if (p.hand.empty()) {
        if (!p.tookMorto && !morto_.empty()) {
            for (int i = 0; i < MORTO_HALF && !morto_.empty(); ++i) {
                p.hand.push_back(morto_.back());
                morto_.pop_back();
            }
            p.tookMorto = true;
            info("Voce pegou o morto.");
            nextTurn();
        } else {
            p.tookMorto = true;
            p.wentOut = true;
            lastWentOutSeat_ = current_;
            lastWentOutByMeld_ = false;
            endHand(true, false);
        }
        return true;
    }

    if (current_ == 0) info("Sua vez acabou.");
    nextTurn();
    return true;
}

void Game::nextTurn() {
    current_ ^= 1;
    beginTurn();
}

void Game::beginTurn() {
    setPhase(Phase::Draw);
    prompt_ = Prompt::None;
    lastDrawn_ = Card{};
    if (current_ == 0) {
        if (canTakeDiscard())
            info("Comprar a mesa? (S/N)");
        else
            info("Compre uma carta do monte. <ENTER>");
    } else {
        info("Agora e minha vez.");
    }
}

void Game::endHand(bool /*playerWentOut*/, bool /*playerWonByMeld*/) {
    // Cards left in the morto are not added to any hand: the original only
    // charges the flat 100 point penalty for each player who never took it.
    morto_.clear();

    const int bonusSeat = lastWentOutSeat_;
    const int bonus = lastWentOutByMeld_ ? 200 : 100;

    for (int seat = 0; seat < 2; ++seat) {
        PlayerState& p = players_[seat];
        int pts = 0;
        for (const Meld& m : p.melds) pts += m.value();
        int handPts = 0;
        for (const Card& c : p.hand) handPts += cardPoints(c);
        if (!p.tookMorto) handPts += 100;  // "perdeu 100 pontos pelo morto"
        pts -= handPts;
        if (seat == bonusSeat) pts += bonus;
        p.lastHandPoints = pts;
        p.total += pts;
    }

    prompt_ = Prompt::NewHand;
    setPhase(Phase::HandOver);
    message_ = "Vamos contar os pontos.";
}

void Game::continueAfterHand() {
    if (players_[0].total >= kTargetScore || players_[1].total >= kTargetScore) {
        setPhase(Phase::MatchOver);
        message_ = "A partida acabou !!!";
        return;
    }
    newHand();
}

void Game::answerPrompt(bool yes) {
    if (prompt_ == Prompt::TakeDiscard) {
        prompt_ = Prompt::None;
        if (yes) draw(true);
        else draw(false);
        return;
    }
    if (prompt_ == Prompt::NewHand) {
        prompt_ = Prompt::None;
        continueAfterHand();
    }
}

void Game::update(double dt) {
    if (phase_ == Phase::HandOver || phase_ == Phase::MatchOver) return;
    if (current_ != 1) return;

    timer_ += dt;
    if (timer_ < 0.7) return;
    autoStep();
}

void Game::announceTurn() {
    if (phase_ == Phase::HandOver || phase_ == Phase::MatchOver) return;
    if (current_ != 0) {
        message_ = "Agora e minha vez.";
        return;
    }
    switch (phase_) {
        case Phase::Draw:
            message_ = canTakeDiscard() ? "Comprar a mesa (S/N) ?"
                                        : "Compre uma carta do monte. <ENTER>";
            break;
        case Phase::Play:
            message_ = "Baixe os jogos.";
            break;
        case Phase::Discard:
            message_ = "Escolha a carta a descartar.";
            break;
        default:
            break;
    }
}

// One phase of an automated turn (draw, lay melds, or discard).
void Game::autoStep() {
    if (phase_ == Phase::HandOver || phase_ == Phase::MatchOver) return;
    switch (phase_) {
        case Phase::Draw:
            aiTurn();
            break;
        case Phase::Play:
            aiLayMelds();
            // layMeld/afterEmptyHand may already have ended the hand (batida)
            // or moved straight to the discard after taking the morto.
            if (phase_ == Phase::Play) setPhase(Phase::Discard);
            break;
        case Phase::Discard:
            aiDiscard();
            break;
        default:
            break;
    }
}

void Game::aiTurn() {
    // Take the table card only when it can be used right away: it extends an
    // existing meld or completes a three-card meld with two cards in hand.
    // Otherwise draw from the monte, which is what actually advances the deal.
    const bool takeTable = !discard_.empty() && canUseTableCard(discard_.back());
    draw(takeTable);
}

bool Game::canUseTableCard(const Card& top) const {
    const PlayerState& me = players_[current_];
    for (const Meld& m : me.melds) {
        Meld probe = m;
        if (extendMeld(probe, {top})) return true;
    }

    const bool room = int(me.melds.size()) < kMaxMelds;
    for (size_t i = 0; i < me.hand.size(); ++i) {
        for (size_t j = i + 1; j < me.hand.size(); ++j) {
            std::vector<Card> sel{top, me.hand[i], me.hand[j]};
            Meld probe;
            if (!buildMeldAuto(sel, probe)) continue;
            if (room) return true;
            // When every meld slot is taken layMeld can only grow an existing
            // one; without this check the AI kept buying a table it could not
            // actually use, so the hand never ended.
            for (const Meld& m : me.melds) {
                if (m.empty() || m.suit != probe.suit) continue;
                Meld grow = m;
                if (extendMeld(grow, sel)) return true;
            }
        }
    }
    return false;
}

std::vector<Card> Game::aiFindMeld() const {
    const PlayerState& me = players_[current_];
    std::vector<Card> best;
    int bestScore = -1;

    for (uint8_t suit = 1; suit <= 4; ++suit) {
        bool already = false;
        for (const Meld& m : me.melds)
            if (!m.empty() && m.suit == suit) already = true;
        if (already) continue;

        std::vector<Card> natural;   // natural cards of this suit
        std::vector<Card> wildPool;  // 2s and jokers
        for (const Card& c : me.hand) {
            if (isWild(c)) wildPool.push_back(c);
            else if (c.suit == suit) natural.push_back(c);
        }
        std::sort(natural.begin(), natural.end(),
                  [](const Card& a, const Card& b) { return a.rank < b.rank; });

        for (int variant = 0; variant < 2; ++variant) {
            // variant 0: ace low, variant 1: ace high (after the king)
            std::vector<Card> pool;
            for (const Card& c : natural) pool.push_back(c);
            if (variant == 1) {
                bool kingish = false;
                for (const Card& c : pool)
                    if (c.rank == QUEEN || c.rank == KING) kingish = true;
                if (!kingish) continue;
                for (Card& c : pool)
                    if (c.rank == ACE) c.rank = ACE_HIGH;
            }
            std::sort(pool.begin(), pool.end(),
                      [](const Card& a, const Card& b) { return a.rank < b.rank; });

            for (size_t i = 0; i < pool.size(); ++i) {
                std::vector<Card> run{pool[i]};
                int wildUsed = 0;
                for (size_t j = i + 1; j < pool.size(); ++j) {
                    const int gap = int(pool[j].rank) - int(run.back().rank) - 1;
                    if (gap < 0) continue;
                    if (gap == 0) {
                        run.push_back(pool[j]);
                    } else if (wildUsed + gap <= int(wildPool.size())) {
                        wildUsed += gap;
                        run.push_back(pool[j]);
                    } else {
                        break;
                    }
                }
                const int total = int(run.size()) + wildUsed;
                if (total < MELD_MIN) continue;

                std::vector<Card> chosen;
                for (const Card& c : run) {
                    // The ace-high variant borrows rank 14 while sorting and
                    // measuring gaps.  Never let that synthetic rank reach the
                    // meld: the save file stores only card ids, so a reloaded
                    // match would rebuild the ace as rank 1 and the AI would
                    // take different decisions (see --savetest).
                    chosen.push_back(c.rank == ACE_HIGH ? makeCard(c.id) : c);
                }
                for (int w = 0; w < wildUsed; ++w) chosen.push_back(wildPool[w]);

                int score = 0;
                for (const Card& c : chosen) score += cardPoints(c) + 1;
                if (score > bestScore) {
                    bestScore = score;
                    best = chosen;
                }
            }
        }
    }
    return best;
}

bool Game::aiLayMelds() {
    bool laid = false;
    PlayerState& me = players_[current_];

    // Play the card just drawn first.  When it came from the table this is what
    // keeps the deal moving: a purchase always turns into a meld instead of
    // bouncing between the two hands forever.
    if (lastDrawn_.valid()) {
        const Card lead = lastDrawn_;
        lastDrawn_ = Card{};
        for (Meld& m : me.melds) {
            if (m.empty()) continue;
            if (extendMeld(m, {lead})) {
                removeCard(me.hand, lead);
                laid = true;
                if (me.hand.empty()) {
                    afterEmptyHand(true);
                    return laid;
                }
                break;
            }
        }
        if (!laid) {
            for (size_t i = 0; i < me.hand.size() && !laid; ++i) {
                for (size_t j = i + 1; j < me.hand.size(); ++j) {
                    std::vector<Card> sel{lead, me.hand[i], me.hand[j]};
                    Meld probe;
                    if (!buildMeldAuto(sel, probe)) continue;
                    if (layMeld(sel).empty()) {
                        laid = true;
                        break;
                    }
                    // No free meld slot accepts these cards: try another pair instead.
                }
            }
        }
    }

    for (int guard = 0; guard < 8; ++guard) {
        std::vector<Card> meld = aiFindMeld();
        if (meld.empty()) break;
        if (!layMeld(meld).empty()) break;
        laid = true;
    }
    // Try to grow existing melds with single cards.
    for (int guard = 0; guard < 12; ++guard) {
        bool grew = false;
        for (Meld& m : me.melds) {
            if (m.empty()) continue;
            for (const Card& c : me.hand) {
                if (extendMeld(m, {c})) {
                    removeCard(me.hand, c);
                    grew = true;
                    laid = true;
                    if (me.hand.empty()) {
                        afterEmptyHand(true);
                        return laid;
                    }
                    break;
                }
            }
            if (grew) break;
        }
        if (!grew) break;
    }
    return laid;
}

void Game::aiDiscard() {
    PlayerState& me = players_[current_];
    if (me.hand.empty()) return;

    // Prefer to throw away the card least connected to the rest of the hand,
    // and among those the most expensive one.
    int bestIdx = 0;
    int bestScore = 1 << 30;
    for (size_t i = 0; i < me.hand.size(); ++i) {
        const Card& c = me.hand[i];
        int connections = 0;
        for (const Card& o : me.hand) {
            if (o.id == c.id) continue;
            if (o.suit == c.suit && std::abs(int(o.rank) - int(c.rank)) == 1) ++connections;
            if (isWild(o)) ++connections;
        }
        if (isWild(c)) connections += 4;  // keep wildcards
        const int score = connections * 100 - cardPoints(c);
        if (score < bestScore) {
            bestScore = score;
            bestIdx = int(i);
        }
    }
    const Card card = me.hand[bestIdx];
    info(current_ == 0 ? "Voce descartou." : "Eu vou descartar.");
    discardCard(card);
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------
// Line-oriented text so a save can be diffed and eyeballed.  Every card is
// stored by its original id (1..108), which is enough to rebuild rank/suit/
// deck through makeCard().  Melds store the 15 slots plus their wild flags.
//
//   BURACO-SAVE 1
//   hand <n>          current <seat>     phase <0..4>    prompt <0..2>
//   rng <u32>         wentout <seat> <byMeld 0|1>        timer <seconds>
//   name <seat> <text to end of line>
//   player <seat> <tookMorto> <wentOut> <lastHandPoints> <total> <n> <ids...>
//   melds <seat> <count>
//   meld <seat> <suit> <dirty> <15 ids> <15 wild flags>
//   monte <n> <ids...>   mesa <n> <ids...>   morto <n> <ids...>
//   msg <text to end of line>

namespace {

constexpr const char* kSaveMagic = "BURACO-SAVE 1";

bool validId(int id) { return id == 0 || (id >= 1 && id <= DECK_SIZE); }

}  // namespace

bool Game::saveTo(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f << kSaveMagic << '\n';
    f << "hand " << handNumber_ << '\n';
    f << "current " << current_ << '\n';
    f << "phase " << int(phase_) << '\n';
    f << "prompt " << int(prompt_) << '\n';
    f << "rng " << rng_ << '\n';
    f << "wentout " << lastWentOutSeat_ << ' ' << (lastWentOutByMeld_ ? 1 : 0) << '\n';
    f << "timer " << timer_ << '\n';
    f << "drawn " << int(lastDrawn_.id) << '\n';

    for (int seat = 0; seat < 2; ++seat) {
        const PlayerState& p = players_[seat];
        f << "name " << seat << ' ' << p.name << '\n';
        f << "player " << seat << ' ' << (p.tookMorto ? 1 : 0) << ' '
          << (p.wentOut ? 1 : 0) << ' ' << p.lastHandPoints << ' ' << p.total << ' '
          << p.hand.size();
        for (const Card& c : p.hand) f << ' ' << int(c.id);
        f << '\n';

        f << "melds " << seat << ' ' << p.melds.size() << '\n';
        for (const Meld& m : p.melds) {
            f << "meld " << seat << ' ' << int(m.suit) << ' ' << (m.dirty ? 1 : 0);
            for (int i = 1; i <= 15; ++i) f << ' ' << int(m.slot[i].id);
            for (int i = 1; i <= 15; ++i) f << ' ' << (m.wild[i] ? 1 : 0);
            f << '\n';
        }
    }

    auto dumpPile = [&f](const char* tag, const std::vector<Card>& pile) {
        f << tag << ' ' << pile.size();
        for (const Card& c : pile) f << ' ' << int(c.id);
        f << '\n';
    };
    dumpPile("monte", monte_);
    dumpPile("mesa", discard_);
    dumpPile("morto", morto_);

    f << "msg " << message_ << '\n';
    return bool(f);
}

bool Game::loadFrom(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    // Parse into a scratch copy so a corrupt file never halves a live match.
    Game g;
    std::string line;
    if (!std::getline(f, line)) return false;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line != kSaveMagic) return false;

    int meldCount[2] = {0, 0};
    int meldSeen[2] = {0, 0};

    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        std::istringstream in(line);
        std::string tag;
        in >> tag;

        if (tag == "hand") {
            in >> g.handNumber_;
        } else if (tag == "current") {
            in >> g.current_;
        } else if (tag == "phase") {
            int v = 0;
            in >> v;
            if (v < 0 || v > int(Phase::MatchOver)) return false;
            g.phase_ = Phase(v);
        } else if (tag == "prompt") {
            int v = 0;
            in >> v;
            if (v < 0 || v > int(Prompt::NewHand)) return false;
            g.prompt_ = Prompt(v);
        } else if (tag == "rng") {
            in >> g.rng_;
        } else if (tag == "wentout") {
            int byMeld = 0;
            in >> g.lastWentOutSeat_ >> byMeld;
            g.lastWentOutByMeld_ = byMeld != 0;
        } else if (tag == "timer") {
            in >> g.timer_;
        } else if (tag == "drawn") {
            int id = 0;
            in >> id;
            if (!validId(id)) return false;
            g.lastDrawn_ = id ? makeCard(uint8_t(id)) : Card{};
        } else if (tag == "name") {
            int seat = -1;
            in >> seat;
            if (seat < 0 || seat > 1) return false;
            std::string rest;
            std::getline(in, rest);
            if (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
            g.players_[seat].name = rest;
        } else if (tag == "player") {
            int seat = -1;
            in >> seat;
            if (seat < 0 || seat > 1) return false;
            int took = 0, out = 0, n = 0;
            in >> took >> out >> g.players_[seat].lastHandPoints >>
                g.players_[seat].total >> n;
            if (n < 0 || n > DECK_SIZE) return false;
            g.players_[seat].tookMorto = took != 0;
            g.players_[seat].wentOut = out != 0;
            g.players_[seat].hand.clear();
            for (int i = 0; i < n; ++i) {
                int id = 0;
                if (!(in >> id) || !validId(id) || id == 0) return false;
                g.players_[seat].hand.push_back(makeCard(uint8_t(id)));
            }
        } else if (tag == "melds") {
            int seat = -1;
            in >> seat;
            if (seat < 0 || seat > 1) return false;
            in >> meldCount[seat];
            if (meldCount[seat] < 0 || meldCount[seat] > kMaxMelds) return false;
            g.players_[seat].melds.clear();
        } else if (tag == "meld") {
            int seat = -1, suit = 0, dirty = 0;
            in >> seat >> suit >> dirty;
            if (seat < 0 || seat > 1 || suit < 0 || suit > JOKER || dirty < 0 || dirty > 1)
                return false;
            Meld m;
            m.suit = uint8_t(suit);
            m.dirty = dirty != 0;
            for (int i = 1; i <= 15; ++i) {
                int id = 0;
                if (!(in >> id) || !validId(id)) return false;
                m.slot[i] = id ? makeCard(uint8_t(id)) : Card{};
            }
            for (int i = 1; i <= 15; ++i) {
                int w = 0;
                if (!(in >> w)) return false;
                m.wild[i] = w != 0;
            }
            if (meldSeen[seat] >= meldCount[seat]) return false;
            ++meldSeen[seat];
            g.players_[seat].melds.push_back(m);
        } else if (tag == "monte" || tag == "mesa" || tag == "morto") {
            int n = 0;
            in >> n;
            if (n < 0 || n > DECK_SIZE) return false;
            std::vector<Card>& pile =
                (tag == "monte") ? g.monte_ : (tag == "mesa") ? g.discard_ : g.morto_;
            pile.clear();
            for (int i = 0; i < n; ++i) {
                int id = 0;
                if (!(in >> id) || id < 1 || id > DECK_SIZE) return false;
                pile.push_back(makeCard(uint8_t(id)));
            }
        } else if (tag == "msg") {
            std::string rest;
            std::getline(in, rest);
            if (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
            g.message_ = rest;
        } else {
            return false;  // unknown tag: refuse the whole file
        }
    }

    if (g.current_ != 0 && g.current_ != 1) return false;
    if (meldSeen[0] != meldCount[0] || meldSeen[1] != meldCount[1]) return false;
    if (g.handNumber_ < 1) return false;

    *this = g;
    return true;
}

}  // namespace buraco
