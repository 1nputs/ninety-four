#!/usr/bin/env bash
# Record the standard capture sessions for `make test`. Needs build/nhl_oracle.so, the saved
# states out/states/demo_faceoff.state (CPU vs CPU, penalties / offsides / line changes on,
# 5 minute periods) and out/states/demo1p_faceoff.state (one human player, same options), and
# the random input script out/captures/checks_script.txt.
set -e
# at most CAPTURE_JOBS sessions at once (memory: a session can hold a few hundred MB of call log)
JOBS="${CAPTURE_JOBS:-4}"
throttle() { while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do wait -n; done; }
cd "$(dirname "$0")/../.."
ROM="${NHL94_ROM:-NHL 94 (UE) [!].gen}"
fns() { python3 -I - "$@" <<'PY'
import json, sys
cg = json.load(open('out/callgraph.json'))
print(','.join(n for n, f in cg.items() if isinstance(f, dict) and f.get('file') in sys.argv[1:] and f.get('insns', 0) >= 2))
PY
}
SCRIPT="$(cat out/captures/checks_script.txt)"
S1=out/states/demo1p_faceoff.state
S2=out/states/demo_faceoff.state
run() { out=$1; shift; python3 tools/hybrid/capture.py "$ROM" --out "out/captures/$out.cap" --frames 40000 --spread "$@" > "out/captures/$out.log" 2>&1; echo "$out: $(grep -c . out/captures/$out.log) lines, $(tail -n 2 out/captures/$out.log | head -n 1)"; }
throttle; run collide      --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns collide94),randomd0,vtoa,GetHot,SetSPA" &
throttle; run checks       --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns checks94)" &
throttle; run checks2      --state $S2 --max 16 --fn "$(fns checks94)" &
throttle; run assign_input --state $S2 --max 32 --fn "$(fns assign94 input94)" &
throttle; run input1p      --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns input94)" &
throttle; run goc          --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns goalie94 onetimer94 crowd94)" &
throttle; run penalty      --state $S2 --max 32 --fn "$(fns penalty94 | sed -E "s/(^|,)(Intermission|DoHiLights|StartHL|StartHL2)(,|$)/\\1/g; s/,,+/,/g; s/^,|,$//g")" &
# period94 + hockey94 without the frame drivers (they run whole frames: ported in the game-loop phase)
throttle; run ph           --state $S2 --max 24 --fn "$(fns period94 hockey94 | sed -E "s/(^|,)(Begin|DoGameFrame|GameStatisticsScreen|Gameloop|Pausemode|PeriodStatsScreen|StartGame|StartPer|VBjsr)(,|$)/\\1/g; s/,,+/,/g; s/^,|,$//g")" &
# display94 + replay94 without ReplayMode (interactive; the replay navigation helpers get no calls in these sessions)
throttle; run dr           --state $S1 --script "$SCRIPT" --max 24 --fn "$(fns display94 replay94 | sed -E "s/(^|,)ReplayMode(,|$)/\\1/g; s/,,+/,/g; s/^,|,$//g")" &
# video94: the hardware boundary (VDP writes are checked in order against the logged writes)
throttle; run video        --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns video94)" &
# the game-play helpers of title94 / data94 / setup94 / stats94 / cards94 / shootout94 / menu94 / fourway94 / sram94
# (the front-end screens of those modules never run in a game); 80000 frames to reach the end of the game
HELPERS="$(tr -s ' \n' ',' < tools/hybrid/helpers_fns.txt)"
throttle; run helpers1p    --state $S1 --script "$SCRIPT" --max 24 --frames 80000 --fn "$HELPERS" &
throttle; run helpers      --state $S2 --max 24 --frames 80000 --fn "$HELPERS" &
# sound94: the sequencer runs every vblank (p_music_vblank); songs at faceoffs, goals and period ends
throttle; run sound        --state $S2 --max 32 --frames 40000 --fn "$(fns sound94)" &
throttle; run sound1p      --state $S1 --script "$SCRIPT" --max 32 --frames 40000 --fn "$(fns sound94)" &
# whole frames and the routines that wait for the vblank: the shim logs each interrupt, and the
# tests replay it where the C waits (records whose interrupts all landed in wait loops)
FRAME="$(tr -s ' \n' ',' < tools/hybrid/frame_fns.txt)"
throttle; run frame1p      --state $S1 --script "$SCRIPT" --max 24 --frames 80000 --fn "$FRAME" &
throttle; run frame        --state $S2 --max 24 --frames 80000 --fn "$FRAME" &
# power on to the title, Start pressed there (the front end; --state "" starts at power on)
BOOT=SegaInit,ValidationRoutine,EASportsScreen,Detect4WayPlay,DefaultMenus,InitSaveRAM,SRAMWaitVblank,VBcount,ValidateSRAM,ClearSRAM,ReadSRAM,WriteSRAM,MakeSRAMChecksum,ReadLineData,HiScoreScreen,newTitleScreen,CreditsPrintRow,CreditsWait,VBlank_TitleScreen,CreditsLineScroll,CreditsScrollStep
throttle; run boot         --state "" --script "1200:START,1206:" --max 24 --frames 1300 --fn "$BOOT" &
# the game setup screen: the menu, the player cards, the play modes, then Start; and a shootout's start
SETUP=GoNewPlayoffs,GoContinuePlayoffs,MoveMenuFrame,SetMenuArea,SetFrameRect,FixModeOptions,ChangeOption,WrapOption,PrintOptions,PrintOptionValue,PrintScrollMarks,PrintOptTeamName,PrintOptionNames,setoptions,ReadSetupPads,SetupDemo,SetupStart,LoadSetupTiles,UpdateRosterPlayers,EraseCard,DrawHomeBlock,DrawVisBlock,DrawTeamLogo,LogoBoxRight,LogoBoxHome,LogoBoxLeft,LogoBoxVis,DrawLogoBox,SuperDiv,ReadPassBits,GetRandomUnusedTeam,InitializeGameStructures,FigureJoy,maketree,NewPO,ContinuePlayoffs,InitShooters,ClearShootout,UpdatePlayoffLevel,DrawMatchupBitmaps,DrawTeamBitmap,VBlank_SetOptions,UnpackPicture,ResetCardTimer,PlayerCardTimer,PlayerCardScreen,PrintRecordValue,CopyString,AppendNumber,GetRecordValue,PrintRecordHolder,PrintRecordVs,AppendRecordHolder,AppendRecordVs,ReadNameLog,NameLogIO,AppendUserName,AppendTeamName,TrimSpaces
throttle; run setup        --state "" --script "$(grep -v '^#' tools/oracle/scripts/setup_menu.txt)" --max 24 --frames 3700 --fn "$SETUP" &
throttle; run setup_so     --state "" --script "$(grep -v '^#' tools/oracle/scripts/setup_shootout.txt)" --max 8 --frames 1500 --fn SetupStart,ClearShootout,InitShooters,ChangeOption &
# the name entry (user records on): a name typed in the letter grid and stored
NAMES=UserNameEntry,NameEntryScreen,NameLetterGrid,NameEntryHelp,PrintNamePrompt,SyncLetterCursor,FindLetter,PrintNameCursor,PrintCurLetter,LetterGridPos,PrintNameLog,MoveLogArrows,PrintLogArrows,GetNameLength,PrintNameField,NameEntryFramer,GetLogName,StoreUserName,WriteNameRecord,SkipOtherUserName,WriteNameLog,ReadNameLog,NameLogIO
throttle; run names        --state "" --script "$(grep -v '^#' tools/oracle/scripts/name_entry.txt)" --max 24 --frames 2200 --fn "$NAMES" &
# the playoff screen: New Playoffs (the first round), and New Playoffs/7 game poked to the semifinals (gamelevel 2:
# the tree pages, the series scores)
PO=PlayoffScreen,HandlePlayoffInput,UpdatePlayoffScroll,PlayoffScreen_waitvsync,PrintSeriesScore,DrawTreeArrow,DrawTeamBlocks,VBlank_Playoff,DrawPlayoffSprite,PlayoffScreenExit
throttle; run playoffs     --state "" --script "$(grep -v '^#' tools/oracle/scripts/playoffs.txt)" --max 16 --frames 1800 --fn "$PO" &
throttle; run playoffs2    --state "" --script "1200:START,1206:,1320:RIGHT,1326:,1346:RIGHT,1352:,1380:START,1386:,1600:RIGHT,1606:,1700:RIGHT,1760:,1800:LEFT,1806:,1900:LEFT,1950:,2100:START,2106:" --poke "1365:CEEC=2" --max 16 --frames 2150 --fn "$PO" &
# the scouting report (the matchups paged, fast text)
SCOUT=ScoutingReport,ScoutCrowdRecord,PageMatchup,PrintMatchupRating,PrintTwoSpaces,DrawMatchupPictures,DrawMatchupPicture,DrawMatchupLogo,DrawMatchupBitmap,GetTeamLogo,PrintMatchupRatings,GetMatchupPlayers,GetMatchupPlayer,RestartAdvantageMarks,PrintAdvantageMarks,GetTeamNickname,GetTeamArena,InitScoutingDisplay,UpdateScoutingDisplay,ScrollDisplayUp,PlayedByHome,PlayedByAway,PlayedByText,PlayedByNone,PlayedByExit,AttribAdjust,DrawPlayerPicture,CalcAttrib,NextHomeHotPlayer,NextAwayHotPlayer,NextHomeColdPlayer,NextAwayColdPlayer
throttle; run scout        --state "" --script "$(grep -v '^#' tools/oracle/scripts/boot_to_game_full.txt)" --max 24 --frames 2420 --fn "$SCOUT" &
# the menu screens' parts (the pause, intermission and game over menus): a pause menu tour from power on, the
# records of a game with user records on in the player cards and record holders, a shootout's shooters, and the
# scoring and penalty summaries of games with goals and penalties (the screens themselves are lockstep's)
MENUS=GetNameandAttrib,AttribStatus,AttribEnergy,AttribFighting,AttribRating,AttribPrintPct,AttribHanded,AttribWeight,DisplayPlayerList,UpdatePlayerListScroll,CheckPlayerListScroll,DisplayPlayerListUp,DisplayPlayerListDown,DrawLineIcon,ClearLineArea,DrawTeamScreen,ClearLineEditorTop,ClearMenuFlags,SetLineSlot,DecodeTeamLines,EncodeTeamLines,DrawGoalieSelectList,GetDefenseStart,GetForwardCount,FormatPlayerNameShort,FormatPlayerNameLast,UpdateScoringScroll,UpdatePenaltyScroll,CheckScoringScroll,CheckPenaltyScroll,DrawScrollArrowsPenalty,DisplayGameInfo,PrintGameInfo,DrawScrollArrows,UpdateScoresScroll,UpdateVertScrollReg,PrintCrowdMeter,PrintCrowdDB,GetLeagueCrowdRecord,GetTeamStruct
CARDS=BuildCardPlayerLists,BuildCardPlayerList,GetCardPlayer,GetCardTeam,GetJerseyString,PrintCentered,PrintCardTeam,PrintCardPosition,PrintOverallRating,PrintStartingLine,PrintTeamRating,PrintGoalsAssists,PrintSaves,ClearCardText,DrawPictureBox,DrawPlayerCard,PrintCardRecords,WrapCardIndex,PrintCardRecordLine,PrintCrowdRecordLine,PrintSaveRecordLine,PrintGoalRecordLine,PrintRecordTitles,PrintWinRecords,PrintPlayerRecords,PrintRecordPage,ClearRecordArea,PrintRecordName,CalcWinPercents,ReadTeamRecords
throttle; run menus        --state "" --script "$(grep -v '^#' tools/oracle/scripts/pause_menus.txt)" --max 16 --frames 8700 --fn "$MENUS,$CARDS" &
throttle; run records      --state "" --script "$(grep -v '^#' tools/oracle/scripts/records_game_30s.txt)" --max 16 --frames 15700 --fn "$CARDS,GetNameandAttrib,FormatPlayerNameShort,FormatPlayerNameLast" &
throttle; run shooters     --state "" --script "$(grep -v '^#' tools/oracle/scripts/shootout_menus.txt)" --max 16 --frames 4900 --fn PrintShooterList,ShootersBackground,ClearShooterScreen,ResetShooterScroll,PrintShooterSlots,PrintShooterBox,PrintShooterNames,GetNameandAttrib,AttribPrintPct,FormatPlayerNameShort &
SUMS=DrawScoringRow,DrawScoringRowUp,DrawScoringRowDown,PrintScorerName,DisplayPenaltyEntry,DisplayPenaltyLineUp,DisplayPenaltyLineDown,UpdateScoringScroll,UpdatePenaltyScroll,DrawScrollArrowsPenalty,AttribStatus,DisplayPlayerList
throttle; run summaries    --state out/states/faceoff.state --script "$(grep -v '^#' tools/oracle/scripts/pause_menus2.txt)" --max 16 --frames 22200 --from 16000 --fn "$SUMS" &
throttle; run summaries2   --state out/states/demo_faceoff.state --script "$(grep -v '^#' tools/oracle/scripts/pause_menus3.txt)" --max 16 --frames 10600 --from 7800 --fn "$SUMS" &
wait
