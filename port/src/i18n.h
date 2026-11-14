// Interface translations.
//
// Portuguese is the original language and the default, so every existing save,
// script and headless dump keeps its bytes when the configuration is absent.
// English is selected with `language=en` in buraco.cfg or the Language row on
// the F9 options screen.  The table is intentionally static: it has no
// dependency on the game, SDL or the configuration parser, which keeps the
// translation layer testable on its own.
#pragma once

#include <cstdio>
#include <string>

namespace i18n {

enum class Language { Pt, En };

Language language();
void setLanguage(Language lang);

const char* code(Language lang);          // "pt" or "en"
const char* displayName(Language lang);   // "Portugues" or "English"
bool valid(Language lang);
bool parseLanguage(const std::string& text, Language& out);
Language toggled(Language lang);

// Every user-visible string.  Formatted entries use printf specifiers and must
// have the same argument list in both languages; call sites pass them through
// format() so a missing argument is caught by the compiler's printf checks
// where available.
enum class Text {
    Empty,
    DefaultPlayerName,   // "Voce" / "You"
    DefaultOpponentName, // "Eu" / "Me"
    DefaultPlayer,       // "JOGADOR" / "PLAYER"
    DefaultPlayer1,
    DefaultPlayer2,
    PressEnter,
    Yes,
    No,

    HelpTitle,
    HelpIntro,
    HelpRules,
    HelpArrows,
    HelpScroll,
    HelpMelds,
    HelpF1,
    HelpF2,
    HelpF3,
    HelpOptions,
    HelpLuck,

    StatusTitle,
    StatusIntro,
    StatusMatches,
    StatusLastHand,
    StatusTotals,
    StatusMidHand,
    StatusCards,
    StatusMelds,
    StatusTable,
    StatusStock,

    HandMine,
    HandOther,
    HandInfo,
    MeldsYours,
    MeldsMine,
    MeldsNone,
    HudTurn,

    ScoreAccum,
    ScoreMeldLine,
    ScoreNoMelds,
    ScoreMelds,
    ScoreBatida,
    ScoreHandCards,
    ScoreMorto,
    ScoreHandTotal,
    ScoreTitle,
    ScoreContinue,

    ContinueTitle,
    ContinueFound,
    ContinuePlayer,
    ContinueQuestion,

    ModeTitle,
    ModeQuestion,
    ModeOne,
    ModeTwo,
    ModeOnline,
    ModeHint,

    NameOnlinePrompt,
    NamePlayer1Prompt,
    NamePlayer2Prompt,
    NameSoloPrompt,
    NameOnlineBlank,
    NameOnlineShare,
    NameConfirm,
    NameHotseatBlank1,
    NameHotseatBlank2,
    NameHotseatSave,
    NameSoloSave,
    NameEmptyError,
    NameLoadError,

    OnlineTitle,
    OnlineIntro,
    OnlineHostChoice,
    OnlineJoinChoice,
    OnlineFirewall,
    OnlineMenuHint,
    OnlineHostTitle,
    OnlinePort,
    OnlineWaiting,
    OnlineHostStep1,
    OnlineHostStep2,
    OnlineCancel,
    OnlineJoinTitle,
    OnlineHostAddress,
    OnlineJoinHint,

    SeatSwitchTitle,
    SeatSwitchPass,
    SeatSwitchHide,

    OptionsTitle,
    OptionsScale,
    OptionsAdjustment,
    OptionsFullscreen,
    OptionsVsync,
    OptionsMaximized,
    OptionsLanguage,
    OptionsSound,
    OptionsVolume,
    OptionsCenter,
    OptionsReset,
    OptionsClose,
    OptionsInteger,
    OptionsStretched,
    OptionsCentered,
    OptionsFree,
    OptionsEnter,
    OptionsEnterEsc,
    OptionsHint1,
    OptionsHint2,
    OptionsHint3,
    OptionsHint4,
    OptionsHint5,
    OptionsIntegerNote,

    MsgShuffling,
    MsgShufflingEnter,
    MsgChooseDiscard,
    MsgBoughtTable,
    MsgAiBoughtTable,
    MsgStockEmpty,
    MsgBoughtStock,
    MsgAiBoughtStock,
    MsgNotLayTime,
    MsgChooseCard,
    MsgNeedCanastra,
    MsgNewMeldMin,
    MsgInvalidMeld,
    MsgNoMeldRoom,
    MsgMeldLaid,
    MsgInvalidMeldTarget,
    MsgCardsDontFit,
    MsgMeldGrown,
    MsgTookMorto,
    MsgTurnOver,
    MsgBuyTableQuestion,
    MsgDrawCard,
    MsgMyTurn,
    MsgCountPoints,
    MsgMatchOver,
    MsgBuyTableQuestionSpaced,
    MsgLayMelds,
    MsgYouDiscarded,
    MsgAiDiscarded,

    MsgOnlineStarted,
    MsgWaitYourTurn,
    MsgConnectionLost,
    MsgMoveSent,
    MsgDesyncHost,
    MsgDesyncDetected,
    MsgConnectionClosedHotseat,
    MsgRestored,
    MsgSaveDisabledOnline,
    MsgSaveFailed,
    MsgLoadDisabledOnline,
    MsgLoadFailed,
    MsgWaitHostContinue,
    MsgOnlineMatchOver,
    MsgLayChoice,
    MsgStartFailed,
    MsgSavedTo,
    MsgWaitingPort,
    MsgConnecting,
    MsgClientConnected,
    MsgConnectedWaiting,
    MsgClientDisconnected,
    MsgConnectionEnded,

    NetFailure,
    NetNoPort,
    NetMessageTooLarge,
    NetSendQueueFull,
    NetNoConnection,
    NetInvalidMessage,
    NetInvalidHello,
    NetUnexpectedSeat,
    NetInvalidSeat,
    NetUnexpectedRemoteSeat,
    NetUnexpectedStart,
    NetInvalidStart,
    NetStartBeforeSeat,
    NetActionBeforeStart,
    NetInvalidAction,
    NetInvalidFingerprint,
    NetInvalidErr,
    NetActionRefused,
    NetUnexpectedMessage,
    NetConnectTimeout,
    NetHandshakeTimeout,
    NetConnectionClosed,
    NetConnectionClosedHandshake,
    NetNoClient,
    NetNoMatch,
    NetNotConnected,

    ProtoLineBreak,
    ProtoShortMessage,
    ProtoUnknown,
    ProtoVersion,
    ProtoExtraArgs,
    ProtoExpectedHello,
    ProtoNameTooLong,
    ProtoExpectedSeat,
    ProtoInvalidSeat,
    ProtoExpectedStart,
    ProtoMissingSeed,
    ProtoInvalidSeed,
    ProtoExpectedFingerprint,
    ProtoInvalidFingerprint,
    ProtoExpectedErr,
    ProtoErrTooLong,
    ProtoExpectedAction,
    ProtoMissingAction,
    ProtoMissingSource,
    ProtoUnknownSource,
    ProtoIncompleteMeld,
    ProtoInvalidTarget,
    ProtoInvalidCount,
    ProtoInvalidCard,
    ProtoMissingAnswer,
    ProtoUnknownAnswer,
    ProtoUnknownAction,
    ProtoNotDrawPhase,
    ProtoEmptyTable,
    ProtoDrawRefused,
    ProtoDiscardRefused,
    ProtoNotPlayPhase,
    ProtoNoQuestion,
    ProtoNotHandOver,
};

const char* text(Text id);

template <typename... Args>
std::string format(Text id, Args... args) {
    char buffer[512];
    std::snprintf(buffer, sizeof buffer, text(id), args...);
    return std::string(buffer);
}

}  // namespace i18n
