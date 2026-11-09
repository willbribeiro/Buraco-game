#include "ui.h"

#include "cards.h"
#include "options.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ui {
namespace {

using buraco::Card;
using buraco::Game;
using buraco::Meld;

inline void put(dos::Video& v, int col, int row, uint8_t ch, uint8_t attr) {
    if (col < 0 || col >= COLS || row < 0 || row >= ROWS) return;
    const int o = (row * COLS + col) * 2;
    v.text[o] = ch;
    v.text[o + 1] = attr;
}

inline void puts(dos::Video& v, int col, int row, const std::string& s, uint8_t attr) {
    for (char c : s) put(v, col++, row, uint8_t(c), attr);
}

inline void hrule(dos::Video& v, int row, uint8_t ch, uint8_t attr) {
    for (int c = 0; c < COLS; ++c) put(v, c, row, ch, attr);
}

inline void clear(dos::Video& v) {
    for (int i = 0; i < COLS * ROWS * 2; i += 2) {
        v.text[i] = ' ';
        v.text[i + 1] = FIELD;
    }
}

// Double-line frame shared by every full-screen panel.
inline void drawFrame(dos::Video& v, int c0, int c1, int r0, int r1) {
    put(v, c0, r0, 0xC9, FIELD);
    for (int c = c0 + 1; c < c1; ++c) put(v, c, r0, 0xCD, FIELD);
    put(v, c1, r0, 0xBB, FIELD);
    for (int r = r0 + 1; r < r1; ++r) {
        put(v, c0, r, 0xBA, FIELD);
        put(v, c1, r, 0xBA, FIELD);
    }
    put(v, c0, r1, 0xC8, FIELD);
    for (int c = c0 + 1; c < c1; ++c) put(v, c, r1, 0xCD, FIELD);
    put(v, c1, r1, 0xBC, FIELD);
}

// One 6x5 text-cell card, exactly like FUN_1000_1dfa.
inline void drawCard(dos::Video& v, const Card& c, int col, int row, bool faceUp,
                     bool selected = false) {
    const uint8_t base = selected ? CARD_SEL : CARD;
    put(v, col, row, 0xDA, base);  // ┌
    for (int i = 1; i <= 4; ++i) put(v, col + i, row, 0xC4, base);
    put(v, col + 5, row, 0xBF, base);  // ┐
    for (int r = 1; r <= 3; ++r) {
        put(v, col, row + r, 0xB3, base);
        put(v, col + 5, row + r, 0xB3, base);
        for (int i = 1; i <= 4; ++i)
            put(v, col + i, row + r, faceUp ? ' ' : 0xB1,
                faceUp ? base : (selected ? CARD_SEL : BACK));
    }
    put(v, col, row + 4, 0xC0, base);  // └
    for (int i = 1; i <= 4; ++i) put(v, col + i, row + 4, 0xC4, base);
    put(v, col + 5, row + 4, 0xD9, base);  // ┘
    if (!faceUp) return;

    const bool red = buraco::isRedSuit(c);
    const uint8_t fg = selected ? CARD_SEL : (red ? CARD_RED : CARD);
    put(v, col + 1, row + 1, uint8_t(buraco::rankGlyph(c)), fg);
    if (c.rank == 10) put(v, col + 2, row + 1, '0', fg);
    put(v, col + 1, row + 2, uint8_t(buraco::suitGlyph(c)), fg);
}

struct FanResult {
    int endCol = 0;
    std::vector<int> columns;  // starting column of every drawn card
};

// Draws a fanned row of cards.  `maxCol` is the last usable column.
inline FanResult drawFan(dos::Video& v, const std::vector<Card>& cards, int col, int row,
                         bool faceUp, int maxCol, const std::vector<bool>* selected) {
    FanResult out;
    int x = col;
    for (size_t i = 0; i < cards.size(); ++i) {
        const int step = buraco::cardWidth(cards[i]);
        if (x + step > maxCol + 1) break;
        const bool sel = selected && i < selected->size() && (*selected)[i];
        out.columns.push_back(x);
        drawCard(v, cards[i], x, row - (sel ? 1 : 0), faceUp, sel);
        x += step;
    }
    out.endCol = x;
    return out;
}

// Compact meld line: the suit once, then the ranks in order.  Wildcards keep
// their own printed rank+suit and are drawn in yellow.
inline void drawMeldLine(dos::Video& v, const Meld& m, int col, int row, int maxCol) {
    if (m.empty()) return;
    int x = col;
    put(v, x++, row, uint8_t(buraco::suitGlyphFor(m.suit)), FIELD);
    put(v, x++, row, ' ', FIELD);
    const bool real = m.realCanastra();
    for (int slot = 1; slot <= 15; ++slot) {
        const Card& c = m.slot[slot];
        if (!c.valid()) continue;
        if (x + 4 > maxCol) {
            puts(v, x, row, "...", DIM);
            return;
        }
        // In a real canastra the 2 is its natural rank, not a wildcard.
        const bool wild = m.wild[slot] && !(real && slot == 2);
        const uint8_t fg = wild ? WILD : FIELD;
        put(v, x++, row, uint8_t(buraco::rankGlyph(c)), fg);
        if (c.rank == 10) put(v, x++, row, '0', fg);
        if (wild) put(v, x++, row, uint8_t(buraco::suitGlyph(c)), fg);
        put(v, x++, row, ' ', FIELD);
    }
    if (m.realCanastra()) {
        puts(v, x + 1, row, "c+500", FIELD);
    } else if (m.canastra()) {
        const std::string tag = m.dirty ? "c+100" : "c+200";
        puts(v, x + 1, row, tag, m.dirty ? WILD : DIM);
    }
}

inline void drawPiles(dos::Video& v, const Game& g) {
    // Morto: the two halves are stacked with a two-column offset, so the second
    // back's frame sits on top of the first (see the morto box in FUN_1000_1920).
    const int halves = (g.mortoRemaining() + buraco::MORTO_HALF - 1) / buraco::MORTO_HALF;
    for (int i = 0; i < halves; ++i) drawCard(v, Card{}, COL_MORTO + i * 2, ROW_PILE, false);

    // Discard pile ("a mesa"): the original fans every card left to right.
    const auto& pile = g.discardPile();
    if (!pile.empty()) {
        const int maxCards = 26;
        std::vector<Card> shown(pile.end() - std::min<size_t>(pile.size(), maxCards), pile.end());
        drawFan(v, shown, COL_MESA, ROW_PILE, true, 70, nullptr);
    }
}

inline void drawMelds(dos::Video& v, const Game& g, const UiState& ui) {
    static const char* kHeader[2] = {"SEUS JOGOS", "MEUS JOGOS"};
    const int col[2] = {COL_MELD_L, COL_MELD_R};
    const int maxCol[2] = {37, 78};
    const int rows = ROW_RULE3 - ROW_MELD;
    for (int seat = 0; seat < 2; ++seat) {
        const auto& melds = g.player(seat).melds;
        int total = 0;
        for (const Meld& m : melds)
            if (!m.empty()) ++total;
        const int start = std::clamp(ui.meldScroll, 0, std::max(0, total - rows));

        if (total > rows) {
            char header[40];
            std::snprintf(header, sizeof header, "%s %d-%d/%d", kHeader[seat], start + 1,
                          std::min(total, start + rows), total);
            puts(v, col[seat], ROW_RULE2 + 1, header, DIM);
        } else {
            puts(v, col[seat], ROW_RULE2 + 1, kHeader[seat], DIM);
        }

        int row = ROW_MELD;
        int shown = 0;
        for (size_t i = size_t(start); i < melds.size() && shown < rows; ++i) {
            if (melds[i].empty()) continue;
            drawMeldLine(v, melds[i], col[seat], row, maxCol[seat]);
            ++row;
            ++shown;
        }
        if (total == 0) puts(v, col[seat], ROW_MELD, "nenhum jogo", DIM);
    }
    // Scores between the two headers so they never sit on top of a meld line.
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d  x  %d", g.player(0).total, g.player(1).total);
    puts(v, 30, ROW_RULE2 + 1, buf, DIM);
}

inline void drawHand(dos::Video& v, const Game& g, const UiState& ui) {
    // During the scoring screen the human hand stays on the table; while the
    // AI is playing its hand is shown face-down under "Minha mao.".
    const bool scoring = g.phase() == buraco::Phase::HandOver ||
                         g.phase() == buraco::Phase::MatchOver;
    const int seat = scoring ? 0 : g.current();
    const auto& p = g.player(seat);
    puts(v, 1, ROW_LABEL, seat == 0 ? "Sua mao." : "Minha mao.", FIELD);

    char buf[64];
    int pts = 0;
    for (const Card& c : p.hand) pts += buraco::cardPoints(c);
    std::snprintf(buf, sizeof buf, "cartas: %2d   pontos: %3d", int(p.hand.size()), pts);
    puts(v, 50, ROW_LABEL, buf, DIM);

    // Never reveal the opponent's cards: the AI hand is only ever drawn from
    // the back.  (PageUp used to toggle a peek, which made it too easy to see
    // the CPU's cards by accident.)
    const bool faceUp = seat == 0;

    // A hand bought from the table can grow past what fits in 78 columns.
    // Show a window that always contains the cursor; moving left/right scrolls
    // it, so every card stays reachable.
    int first = 0;
    int last = int(p.hand.size()) - 1;
    if (seat == 0 && !p.hand.empty()) {
        const int cur = std::clamp(ui.cursor, 0, last);
        int used = buraco::cardWidth(p.hand[cur]);
        first = cur;
        while (first > 0) {
            const int w = buraco::cardWidth(p.hand[first - 1]);
            if (used + w > 78) break;
            used += w;
            --first;
        }
        last = cur;
        while (last < int(p.hand.size()) - 1) {
            const int w = buraco::cardWidth(p.hand[last + 1]);
            if (used + w > 78) break;
            used += w;
            ++last;
        }
    }

    std::vector<Card> visible;
    if (!p.hand.empty()) visible.assign(p.hand.begin() + first, p.hand.begin() + last + 1);
    std::vector<bool> visibleSel;
    const std::vector<bool>* selPtr = nullptr;
    if (seat == 0) {
        visibleSel.assign(visible.size(), false);
        for (size_t i = 0; i < visible.size(); ++i) {
            const size_t src = size_t(first) + i;
            if (src < ui.selected.size()) visibleSel[i] = ui.selected[src];
        }
        selPtr = &visibleSel;
    }

    FanResult fan = drawFan(v, visible, 1, ROW_HAND, faceUp, 78, selPtr);

    if (seat == 0 && !p.hand.empty()) {
        const int idx = std::clamp(ui.cursor, 0, int(p.hand.size()) - 1) - first;
        if (idx >= 0 && idx < int(fan.columns.size()))
            put(v, fan.columns[idx] + 1, ROW_CURSOR, 0x1F, FIELD);  // ▼ above the cursor card
    }
    if (first > 0) put(v, 0, ROW_HAND, '<', DIM);
    if (last < int(p.hand.size()) - 1) put(v, 79, ROW_HAND, '>', DIM);
}

// Splits a paragraph into lines of at most `width` columns, breaking on
// spaces.  The screens are written in CP437 bytes, so a byte count is enough.
inline std::vector<std::string> wrapText(const std::string& text, int width) {
    std::vector<std::string> lines;
    if (text.empty()) {
        lines.emplace_back();
        return lines;
    }
    size_t start = 0;
    while (start < text.size()) {
        if (text.size() - start <= size_t(width)) {
            lines.push_back(text.substr(start));
            break;
        }
        size_t cut = start + size_t(width);
        size_t space = text.rfind(' ', cut);
        if (space == std::string::npos || space < start) space = cut;
        lines.push_back(text.substr(start, space - start));
        start = space;
        while (start < text.size() && text[start] == ' ') ++start;
    }
    return lines;
}

// Full-screen framed text panel shared by the intro, the F1 help and F3.
inline void drawTextBox(dos::Video& v, const char* title,
                        const std::vector<std::string>& paragraphs) {
    clear(v);
    const int c0 = 1, c1 = 78, r0 = 1, r1 = 23;
    drawFrame(v, c0, c1, r0, r1);

    int row = r0 + 1;
    for (const std::string& p : paragraphs) {
        for (const std::string& line : wrapText(p, c1 - c0 - 3)) {
            if (row >= r1) break;
            puts(v, c0 + 2, row, line, FIELD);
            ++row;
        }
        if (row >= r1) break;
    }
    puts(v, 0, 0, title, DIM);
}

// F3: the general situation, exactly in the wording of the original game.
inline void drawStatus(dos::Video& v, const Game& g) {
    char buf[256];
    std::vector<std::string> lines;
    lines.push_back("A situacao do jogo ate aqui:");
    lines.push_back(g.player(0).name + ",");
    // CP437 0xA0 is the accented 'a' used by the original text ("já").
    std::snprintf(buf, sizeof buf,
                  "nos j%c %d partidas das quais voce ganhou %d e eu ganhou %d.",
                  char(0xA0), g.handsPlayed(), g.wins(0), g.wins(1));
    lines.push_back(buf);
    std::snprintf(buf, sizeof buf,
                  "Na ultima partida voce fez %d, e eu fiz %d.",
                  g.player(0).lastHandPoints, g.player(1).lastHandPoints);
    lines.push_back(buf);
    std::snprintf(buf, sizeof buf,
                  "Ate agora voce ja fez %d pontos e eu fiz %d.",
                  g.player(0).total, g.player(1).total);
    lines.push_back(buf);
    lines.push_back("Estamos no meio de uma partida.");
    std::snprintf(buf, sizeof buf, "Voce tem %d cartas na mao, e eu tenho %d.",
                  int(g.player(0).hand.size()), int(g.player(1).hand.size()));
    lines.push_back(buf);
    std::snprintf(buf, sizeof buf, "Voce ja baixou %d jogos e eu %d.",
                  g.meldCount(0), g.meldCount(1));
    lines.push_back(buf);
    std::snprintf(buf, sizeof buf, "Na mesa ha %d cartas.",
                  int(g.discardPile().size()));
    lines.push_back(buf);
    std::snprintf(buf, sizeof buf, "No monte ha %d cartas.", int(g.monte().size()));
    lines.push_back(buf);
    lines.push_back("...agora aperte ENTER");
    drawTextBox(v, "BURACO - situacao geral", lines);
}

// Compact name for a meld slot (1..13, plus the ace-high slot 14).
inline std::string slotName(int slot) {
    switch (slot) {
        case 1:
        case 14:
            return "A";
        case 11:
            return "J";
        case 12:
            return "Q";
        case 13:
            return "K";
        default:
            return std::to_string(slot);
    }
}

inline std::string meldRange(const Meld& m) {
    const int lo = m.lowSlot(), hi = m.highSlot();
    std::string s;
    s += buraco::suitGlyphFor(m.suit);
    s += ' ';
    s += slotName(lo);
    if (hi != lo) {
        s += '-';
        s += slotName(hi);
    }
    return s;
}

inline const char* meldBonusTag(const Meld& m) {
    if (m.realCanastra()) return "real+500";
    if (m.canastra()) return m.dirty ? "c+100" : "c+200";
    return "";
}

// Right-aligned number inside one 37-column score column.
inline void scoreNumber(dos::Video& v, int col, int row, int value, bool plus,
                        uint8_t attr) {
    char buf[24];
    if (plus && value > 0)
        std::snprintf(buf, sizeof buf, "+%d", value);
    else
        std::snprintf(buf, sizeof buf, "%d", value);
    puts(v, col + 36 - int(std::strlen(buf)), row, buf, attr);
}

inline void drawScoreColumn(dos::Video& v, const Game& g, int seat, int col) {
    const auto& p = g.player(seat);
    char buf[80];

    puts(v, col, 2, p.name, FIELD);
    std::snprintf(buf, sizeof buf, "acum %d", p.total);
    puts(v, col + 36 - int(std::strlen(buf)), 2, buf, DIM);

    int row = 3;
    int meldTotal = 0;
    int shown = 0;
    for (const Meld& m : p.melds) {
        if (m.empty()) continue;
        ++shown;
        const int pts = m.value();
        meldTotal += pts;

        std::snprintf(buf, sizeof buf, "Jogo %2d %s", shown, meldRange(m).c_str());
        puts(v, col, row, buf, FIELD);
        const char* tag = meldBonusTag(m);
        if (*tag) {
            const bool dirty = m.dirty && !m.realCanastra();
            puts(v, col + 18, row, tag, dirty ? WILD : DIM);
        }
        scoreNumber(v, col, row, pts, false, FIELD);
        ++row;
    }
    if (shown == 0) puts(v, col, row, "(nenhum jogo)", DIM);

    int handPts = 0;
    for (const Card& c : p.hand) handPts += buraco::cardPoints(c);
    const int batida = seat == g.lastWentOutSeat() ? 100 : 0;
    const int morto = p.tookMorto ? 0 : -100;
    const int total = meldTotal + batida - handPts + morto;

    puts(v, col, 18, "Jogos", FIELD);
    scoreNumber(v, col, 18, meldTotal, false, FIELD);
    puts(v, col, 19, "Batida", batida ? FIELD : DIM);
    scoreNumber(v, col, 19, batida, true, batida ? FIELD : DIM);
    puts(v, col, 20, "Cartas na mao", FIELD);
    scoreNumber(v, col, 20, -handPts, false, FIELD);
    puts(v, col, 21, "Morto", morto ? FIELD : DIM);
    scoreNumber(v, col, 21, morto, false, morto ? FIELD : DIM);
    puts(v, col, 22, "Total da mao", FIELD);
    scoreNumber(v, col, 22, total, false, FIELD);
}

inline void drawHandOver(dos::Video& v, const Game& g) {
    // The score screen takes over the whole 80x25 display so every meld of
    // both players fits (up to 15 melds each).
    clear(v);
    const int r0 = 1, r1 = 23;
    drawFrame(v, 0, COLS - 1, r0, r1);
    for (int r = r0 + 1; r < r1; ++r) put(v, 39, r, 0xBA, FIELD);

    char buf[80];
    std::snprintf(buf, sizeof buf, "CONTAGEM DE PONTOS   mao %d", g.handNumber());
    puts(v, 2, 0, buf, FIELD);

    drawScoreColumn(v, g, 0, 2);
    drawScoreColumn(v, g, 1, 41);

    puts(v, 2, ROWS - 1, "ENTER para continuar", WILD);
}

// The text of the initial screen, also shown by F1.  Each entry is one
// paragraph; drawTextBox wraps them to the width of the panel.
inline void drawHelp(dos::Video& v) {
    static const char* kLines[] = {
        "Vamos jogar buraco ?",
        "",
        "Durante o jogo, voce pode responder minhas perguntas com \"S\" ou \"N\".",
        "As flechas para esquerda e direita servem para escolher as suas cartas.",
        "Quando voce tiver escolhido, aperte a barra de espacos para que eu saiba "
        "qual carta voce escolheu. Se voce nao quizer mais esta carta, aperte a "
        "barra novamente. Para baixar um jogo, escolha algumas cartas e no fim "
        "aperte ENTER. Para descartar, aperte ENTER sem ter escolhido nenhuma "
        "carta.",
        "As flechas para cima e para baixo mostram os jogos que estao na mesa.",
        "Para ver esta tela, aperte F1.",
        "Para gravar o jogo e continuar depois, aperte F2.",
        "Para ver a situacao geral do jogo, aperte F3.",
        "Boa sorte!",
        "",
        "...agora aperte ENTER",
    };
    std::vector<std::string> paragraphs;
    for (const char* line : kLines) paragraphs.emplace_back(line);
    drawTextBox(v, "BURACO - ajuda", paragraphs);
}

// Asked right after the intro when a save file was found in the game folder.
inline void drawContinuePrompt(dos::Video& v, const std::string& saveFile,
                               const std::string& playerName) {
    std::vector<std::string> lines;
    lines.push_back("Encontrei um jogo gravado:");
    lines.push_back(saveFile);
    if (!playerName.empty()) lines.push_back("Jogador: " + playerName);
    lines.push_back("");
    lines.push_back("Continuar o jogo (S) ou criar um novo (N)?");
    drawTextBox(v, "BURACO - jogo gravado", lines);
}

// Asked for a new game: the typed name becomes the save file name.
inline void drawNameEntry(dos::Video& v, const std::string& input,
                          const std::string& error) {
    std::vector<std::string> lines;
    lines.push_back("Qual o seu nome?");
    lines.push_back("");
    lines.push_back("> " + input + "_");
    lines.push_back("");
    lines.push_back("O nome digitado sera o nome do arquivo de progresso.");
    lines.push_back("ENTER confirma.  ESC volta a tela inicial.");
    if (!error.empty()) {
        lines.push_back("");
        lines.push_back(error);
    }
    drawTextBox(v, "BURACO - novo jogo", lines);
}

}  // namespace

void draw(dos::Video& v, const Game& g, const UiState& ui) {
    switch (ui.screen) {
        case UiState::Screen::Intro:
            drawHelp(v);
            return;
        case UiState::Screen::ContinuePrompt:
            drawContinuePrompt(v, ui.promptSave, ui.promptName);
            return;
        case UiState::Screen::NameEntry:
            drawNameEntry(v, ui.inputName, ui.inputError);
            return;
        case UiState::Screen::Game:
            break;
    }

    clear(v);
    hrule(v, ROW_RULE1, 0xC4, FIELD);
    hrule(v, ROW_RULE2, 0xC4, FIELD);
    hrule(v, ROW_RULE3, 0xC4, FIELD);
    hrule(v, ROW_RULE4, 0xC4, FIELD);
    put(v, COL_SPLIT, ROW_RULE2, 0xD2, FIELD);
    for (int r = ROW_RULE2 + 1; r < ROW_RULE3; ++r) put(v, COL_SPLIT, r, 0xBA, FIELD);
    put(v, COL_SPLIT, ROW_RULE3, 0xD0, FIELD);

    puts(v, 0, ROW_MSG, g.message(), FIELD);

    drawPiles(v, g);
    drawMelds(v, g, ui);
    drawHand(v, g, ui);

    if (g.phase() == buraco::Phase::HandOver) drawHandOver(v, g);
    if (ui.status) drawStatus(v, g);
    if (ui.help) drawHelp(v);
}

void drawOptions(dos::Video& v, const config::DisplaySettings& settings,
                 int selectedRow) {
    clear(v);
    drawFrame(v, 1, 78, 1, 23);
    puts(v, 0, 0, "BURACO - opcoes", DIM);

    static const int kRows[options::kRowCount] = {3, 4, 5, 6, 7, 9, 10, 11};
    for (int i = 0; i < options::kRowCount; ++i) {
        const options::Row row = static_cast<options::Row>(i);
        const bool selected = i == selectedRow;
        const uint8_t attr = selected ? CARD_SEL : FIELD;
        if (selected) put(v, 2, kRows[i], '>', attr);
        puts(v, 4, kRows[i], options::label(row), attr);
        const std::string text = options::value(row, settings);
        if (!text.empty()) puts(v, 27, kRows[i], text, attr);
    }

    puts(v, 3, 14, "Setas cima/baixo escolhem; esquerda/direita alteram.", DIM);
    puts(v, 3, 15, "ENTER aplica.  F9 ou ESC fecha esta tela.", DIM);
    puts(v, 3, 16, "As preferencias sao gravadas no buraco.cfg ao sair.", DIM);
    puts(v, 3, 18, "1x..8x = 640x400 vezes a escala.  Auto usa o tamanho salvo", DIM);
    puts(v, 3, 19, "e reduz para 90% da area util da tela.", DIM);
    if (settings.integerScale)
        puts(v, 3, 21, "Modo inteiro: a imagem pode cair para 1x em telas pequenas.", WILD);
}

}  // namespace ui
