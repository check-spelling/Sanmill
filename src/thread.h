/*
  Fishmill, a UCI Mill Game playing engine derived from Stockfish
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad (Stockfish author)
  Copyright (C) 2015-2020 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad (Stockfish author)
  Copyright (C) 2020 Calcitem <calcitem@outlook.com>

  Fishmill is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Fishmill is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "movepick.h"
#include "position.h"
#include "search.h"
#include "thread_win32_osx.h"

#include "config.h"
#ifdef QT_UI
#include <QObject>
#endif QT_UI

/// Thread class keeps together all the thread-related stuff. We use
/// per-thread pawn and material hash tables so that once we get a
/// pointer to an entry its life time is unlimited and we don't have
/// to care about someone changing the entry under our feet.
class Thread 
#ifdef QT_UI
    : public QObject
#endif
{
public:
    std::mutex mutex;
    std::condition_variable cv;
    size_t idx;
    bool exit = false, searching = true; // Set before starting std::thread
    NativeThread stdThread;

    string strCommand;

    explicit Thread(size_t n
#ifdef QT_UI
                    , QObject *parent = nullptr
#endif
    );
    virtual ~Thread();
    int search();
    void clear();
    void idle_loop();
    void start_searching();
    void wait_for_search_finished();
    int best_move_count(Move move) const;

    void pause();

    size_t pvIdx, pvLast;
    uint64_t ttHitAverage;
    int selDepth, nmpMinPly;
    Color nmpColor;
    std::atomic<uint64_t> nodes, tbHits, bestMoveChanges;

    Position *rootPos { nullptr };
    Search::RootMoves rootMoves;
    Depth rootDepth, completedDepth;
    CounterMoveHistory counterMoves;
    ButterflyHistory mainHistory;
    LowPlyHistory lowPlyHistory;
    CapturePieceToHistory captureHistory;
    ContinuationHistory continuationHistory[2][2];

    void setAi(Position *p);
    void setAi(Position *p, int time);

    void clearTT();
    string nextMove();
    Depth adjustDepth();

    int getTimeLimit()
    {
        return timeLimit;
    }

    void analyze(Color c);

    void clearHistoryScore()
    {
#ifdef HOSTORY_HEURISTIC
        movePicker->clearHistoryScore();
#endif
    }

#ifdef TIME_STAT
    TimePoint sortTime{ 0 };
#endif
#ifdef CYCLE_STAT
    stopwatch::rdtscp_clock::time_point sortCycle;
    stopwatch::timer<std::chrono::system_clock>::duration sortCycle { 0 };
    stopwatch::timer<std::chrono::system_clock>::period sortCycle;
#endif

#ifdef ENDGAME_LEARNING
    bool findEndgameHash(key_t key, Endgame &endgame);
    static int recordEndgameHash(key_t key, const Endgame &endgame);
    void clearEndgameHashMap();
    static void recordEndgameHashMapToFile();
    static void loadEndgameFileToHashMap();
#endif // ENDGAME_LEARNING

#ifdef TRANSPOSITION_TABLE_ENABLE
#ifdef TRANSPOSITION_TABLE_DEBUG
    size_t tteCount{ 0 };
    size_t ttHitCount{ 0 };
    size_t ttMissCount{ 0 };
    size_t ttInsertNewCount{ 0 };
    size_t ttAddrHitCount{ 0 };
    size_t ttReplaceCozDepthCount{ 0 };
    size_t ttReplaceCozHashCount{ 0 };
#endif
#endif

public:
    Depth originDepth { 0 };
    Depth adjustedDepth { 0 };

    Move bestMove { MOVE_NONE };
    Value bestvalue { VALUE_ZERO };
    Value lastvalue { VALUE_ZERO };

    int us;

private:
    int timeLimit;

#ifdef QT_UI
    Q_OBJECT

public:
    void emitCommand();

signals:
    void command(const string &cmdline, bool update = true);
#endif // QT_UI
};


/// MainThread is a derived class specific for main thread

struct MainThread : public Thread
{
    using Thread::Thread;

    int search() /* override */;
    void check_time();

    double previousTimeReduction;
    Value bestPreviousScore;
    Value iterValue[4];
    int callsCnt;
    bool stopOnPonderhit;
    std::atomic_bool ponder;
};


/// ThreadPool struct handles all the threads-related stuff like init, starting,
/// parking and, most importantly, launching a thread. All the access to threads
/// is done through this class.

struct ThreadPool : public std::vector<Thread *>
{
    void start_thinking(Position *, StateListPtr &, const Search::LimitsType &, bool = false);
    void clear();
    void set(size_t);

    MainThread *main()        const
    {
        return static_cast<MainThread *>(front());
    }
    uint64_t nodes_searched() const
    {
        return accumulate(&Thread::nodes);
    }
    uint64_t tb_hits()        const
    {
        return accumulate(&Thread::tbHits);
    }

    std::atomic_bool stop, increaseDepth;

private:
    StateListPtr setupStates;

    uint64_t accumulate(std::atomic<uint64_t> Thread:: *member) const
    {
        uint64_t sum = 0;
        for (Thread *th : *this)
            sum += (th->*member).load(std::memory_order_relaxed);
        return sum;
    }
};

extern ThreadPool Threads;

#endif // #ifndef THREAD_H_INCLUDED
