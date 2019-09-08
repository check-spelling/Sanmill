﻿/*****************************************************************************
 * Copyright (C) 2018-2019 MillGame authors
 *
 * Authors: liuweilhy <liuweilhy@163.com>
 *          Calcitem <calcitem@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <cmath>
#include <array>
#include <chrono>
#include <algorithm>

#include "search.h"
#include "evaluate.h"
#include "movegen.h"
#include "hashmap.h"
#include "types.h"

using namespace CTSL;

#ifdef TRANSPOSITION_TABLE_ENABLE
static constexpr int TRANSPOSITION_TABLE_SIZE = 0x2000000; // 8-128M:102s, 4-64M:93s 2-32M:91s 1-16M: 冲突
HashMap<hash_t, MillGameAi_ab::HashValue> transpositionTable(TRANSPOSITION_TABLE_SIZE);
#endif // TRANSPOSITION_TABLE_ENABLE

#ifdef BOOK_LEARNING
static constexpr int bookHashsize = 0x1000000; // 16M
HashMap<hash_t, MillGameAi_ab::HashValue> bookHashMap(bookHashsize);
vector<hash_t> openingBook;
#endif // BOOK_LEARNING

// 用于检测重复局面
vector<hash_t> positions;

MillGameAi_ab::MillGameAi_ab()
{
    buildRoot();
}

MillGameAi_ab::~MillGameAi_ab()
{
    deleteTree(rootNode);
    rootNode = nullptr;
}

depth_t MillGameAi_ab::changeDepth(depth_t originalDepth)
{
    depth_t newDepth = originalDepth;

    if ((gameTemp.context.stage) & (GAME_PLACING)) {
#ifdef GAME_PLACING_DYNAMIC_DEPTH
#ifdef DEAL_WITH_HORIZON_EFFECT
#ifdef TRANSPOSITION_TABLE_ENABLE
        depth_t depthTable[] = { 4, 11, 12, 13, 14, 14,  14, 12, 11, 10, 6, 6, 1 };
#else // TRANSPOSITION_TABLE_ENABLE
        depth_t depthTable[] = { 2, 11, 11, 11, 11, 10,   9,  8,  8, 8, 7, 7, 1 };
#endif // TRANSPOSITION_TABLE_ENABLE
#else // DEAL_WITH_HORIZON_EFFECT
#ifdef TRANSPOSITION_TABLE_ENABLE
#ifdef RAPID_GAME
        depth_t depthTable[] = { 6, 14, 15, 16, 15, 15, 15, 13, 10,  9, 8, 7, 1 };
#else
        depth_t depthTable[] = { 6, 15, 16, 17, 16, 16, 16, 14, 13, 12, 9, 7, 1 };
      //depth_t depthTable[] = { 6, 15, 16, 17, 16, 16, 16, 12, 12, 12, 9, 7, 1 };
#endif  // RAPID_GAME
#else // TRANSPOSITION_TABLE_ENABLE
        depth_t depthTable[] = { 2, 13, 13, 13, 12, 11, 10,  9,  9,  8, 8, 7, 1 };
#endif
#endif // DEAL_WITH_HORIZON_EFFECT
        newDepth = depthTable[gameTemp.getPiecesInHandCount_1()];
#elif defined GAME_PLACING_FIXED_DEPTH
        newDepth = GAME_PLACING_FIXED_DEPTH;
#endif // GAME_PLACING_DYNAMIC_DEPTH
    }

#ifdef GAME_MOVING_FIXED_DEPTH
    // 走棋阶段将深度调整
    if ((gameTemp.context.stage) & (GAME_MOVING)) {
        newDepth = GAME_MOVING_FIXED_DEPTH;
    }
#endif /* GAME_MOVING_FIXED_DEPTH */

    loggerDebug("Depth: %d\n", newDepth);

    return newDepth;
}

void MillGameAi_ab::buildRoot()
{
    rootNode = addNode(nullptr, 0, 0, 0, PLAYER_NOBODY);
}

struct MillGameAi_ab::Node *MillGameAi_ab::addNode(
    Node *parent,
    value_t value,
    move_t move,
    move_t bestMove,
    enum Player player
)
{
#ifdef MEMORY_POOL
    Node *newNode = pool.newElement();
#else
    Node *newNode = new Node;
#endif

    newNode->parent = parent;
    newNode->value = value;
    newNode->move = move;

    nodeCount++;
#ifdef DEBUG_AB_TREE
    newNode->id = nodeCount;
#endif

#ifdef SORT_CONSIDER_PRUNED
    newNode->pruned = false;
#endif

#ifdef DEBUG_AB_TREE
    newNode->hash = 0;
#endif

#ifdef DEBUG_AB_TREE
#ifdef TRANSPOSITION_TABLE_ENABLE
    newNode->isHash = false;
#endif
#endif

    newNode->player = player;

#ifdef DEBUG_AB_TREE
    newNode->root = rootNode;
    newNode->stage = gameTemp.context.stage;
    newNode->action = gameTemp.context.action;
    newNode->evaluated = false;
    newNode->nPiecesInHandDiff = INT_MAX;
    newNode->nPiecesOnBoardDiff = INT_MAX;
    newNode->nPiecesNeedRemove = INT_MAX;
    newNode->alpha = -INF_VALUE;
    newNode->beta = INF_VALUE;
    newNode->result = 0;
    newNode->visited = false;

    int r, s;
    char cmd[32] = { 0 };

    if (move < 0) {
        gameTemp.context.board.pos2rs(-move, r, s);
        sprintf(cmd, "-(%1u,%1u)", r, s);
    } else if (move & 0x7f00) {
        int r1, s1;
        gameTemp.context.board.pos2rs(move >> 8, r1, s1);
        gameTemp.context.board.pos2rs(move & 0x00ff, r, s);
        sprintf(cmd, "(%1u,%1u)->(%1u,%1u)", r1, s1, r, s);
    } else {
        gameTemp.context.board.pos2rs(move & 0x007f, r, s);
        sprintf(cmd, "(%1u,%1u)", r, s);
    }

    newNode->cmd = cmd;
#endif // DEBUG_AB_TREE

    if (parent) {
        // 若没有启用置换表，或启用了但为叶子节点，则 bestMove 为0
        if (bestMove == 0 || move != bestMove) {
#ifdef MILL_FIRST
            // 优先成三
            if (gameTemp.getStage() == GAME_PLACING && move > 0 && gameTemp.context.board.isInMills(move, true)) {
                parent->children.insert(parent->children.begin(), newNode);
            } else {
                parent->children.push_back(newNode);
            }
#else
            parent->children.push_back(newNode);
#endif
        } else {
            // 如果启用了置换表并且不是叶子结点，把哈希得到的最优着法换到首位
            parent->children.insert(parent->children.begin(), newNode);
        }
    }

    return newNode;
}

bool MillGameAi_ab::nodeLess(const Node *first, const Node *second)
{
#ifdef SORT_CONSIDER_PRUNED
    if (first->value < second->value) {
        return true;
    }

    if ((first->value == second->value) &&
        (!first->pruned&& second->pruned)) {
        return true;
    }

    return false;
#else
    return first->value < second->value;
#endif
}

bool MillGameAi_ab::nodeGreater(const Node *first, const Node *second)
{
#ifdef SORT_CONSIDER_PRUNED
    if (first->value > second->value) {
        return true;
    }

    if ((first->value == second->value) &&
        (!first->pruned && second->pruned)) {
        return true;
    }

    return false;
#else
    return first->value > second->value;
#endif
}

void MillGameAi_ab::sortLegalMoves(Node *node)
{
    // 这个函数对效率的影响很大，排序好的话，剪枝较早，节省时间，但不能在此函数耗费太多时间

    if (gameTemp.whosTurn() == PLAYER1) {
        std::stable_sort(node->children.begin(), node->children.end(), nodeGreater);
    } else {
        std::stable_sort(node->children.begin(), node->children.end(), nodeLess);
    }
}

void MillGameAi_ab::deleteTree(Node *node)
{
    // 递归删除节点树
    if (node == nullptr) {
        return;
    }

    for (auto i : node->children) {
        deleteTree(i);
    }

    node->children.clear();

#ifdef MEMORY_POOL
    pool.deleteElement(node);
#else
    delete(node);
#endif  
}

void MillGameAi_ab::setGame(const MillGame &game)
{
    // 如果规则改变，重建hashmap
    if (strcmp(this->game_.currentRule.name, game.currentRule.name) != 0) {
#ifdef TRANSPOSITION_TABLE_ENABLE
        clearTranspositionTable();
#endif // TRANSPOSITION_TABLE_ENABLE

#ifdef BOOK_LEARNING
        // TODO: 规则改变时清空学习表
        //clearBookHashMap();
        //openingBook.clear();
#endif // BOOK_LEARNING

        positions.clear();
    }

    this->game_ = game;
    gameTemp = game;
    gameContext = &(gameTemp.context);
    requiredQuit = false;
    deleteTree(rootNode);
#ifdef MEMORY_POOL
    rootNode = pool.newElement();
#else
    rootNode = new Node;
#endif
    rootNode->value = 0;
    rootNode->move = 0;
    rootNode->parent = nullptr;
#ifdef SORT_CONSIDER_PRUNED
    rootNode->pruned = false;
#endif
#ifdef DEBUG_AB_TREE
    rootNode->action = ACTION_NONE;
    rootNode->stage = GAME_NONE;
    rootNode->root = rootNode;
#endif
}

int MillGameAi_ab::alphaBetaPruning(depth_t depth)
{
    value_t value = 0;

    depth_t d = changeDepth(depth);

    time_t time0 = time(nullptr);
    srand(static_cast<unsigned int>(time0));

    chrono::steady_clock::time_point timeStart = chrono::steady_clock::now();
    chrono::steady_clock::time_point timeEnd;

#ifdef BOOK_LEARNING
    if (game_.getStage() == GAME_PLACING)
    {
        if (game_.context.nPiecesInHand_1 <= 10) {
            // 开局库只记录摆棋阶段最后的局面
            openingBook.push_back(game_.getHash());
        } else {
            // 暂时在此处清空开局库
            openingBook.clear();
        }
    }
#endif

#ifdef THREEFOLD_REPETITION
    static int nRepetition = 0;

    if (game_.getStage() == GAME_MOVING) {
        hash_t hash = game_.getHash();
        
        if (std::find(positions.begin(), positions.end(), hash) != positions.end()) {
            nRepetition++;
            if (nRepetition == 3) {
                nRepetition = 0;
                return 3;
            }
        } else {
            positions.push_back(hash);
        }
    }

    if (game_.getStage() == GAME_PLACING) {
        positions.clear();
    }
#endif // THREEFOLD_REPETITION

    // 随机打乱着法顺序
    MoveList::shuffleMovePriorityTable(game_);   

#ifdef IDS_SUPPORT
    // 深化迭代
    for (depth_t i = 2; i < d; i += 1) {
#ifdef TRANSPOSITION_TABLE_ENABLE
#ifdef CLEAR_TRANSPOSITION_TABLE
        clearTranspositionTable();   // 每次走子前清空哈希表
#endif
#endif
        alphaBetaPruning(i, -INF_VALUE, INF_VALUE, rootNode);
    }

    timeEnd = chrono::steady_clock::now();
    loggerDebug("IDS Time: %llus\n", chrono::duration_cast<chrono::seconds>(timeEnd - timeStart).count());
#endif /* IDS_SUPPORT */

#ifdef TRANSPOSITION_TABLE_ENABLE
#ifdef CLEAR_TRANSPOSITION_TABLE
    clearTranspositionTable();  // 每次走子前清空哈希表
#endif
#endif

    value = alphaBetaPruning(d, -INF_VALUE /* alpha */, INF_VALUE /* beta */, rootNode);

    timeEnd = chrono::steady_clock::now();
    loggerDebug("Total Time: %llus\n", chrono::duration_cast<chrono::seconds>(timeEnd - timeStart).count());

    // 生成了 Alpha-Beta 树

    return 0;
}

value_t MillGameAi_ab::alphaBetaPruning(depth_t depth, value_t alpha, value_t beta, Node *node)
{
    // 评价值
    value_t value;

    // 当前节点的 MinMax 值，最终赋值给节点 value，与 alpha 和 Beta 不同
    value_t minMax;

    // 临时增加的深度，克服水平线效应用
    depth_t epsilon = 0;

    // 子节点的最优着法
    move_t bestMove = 0;

#if ((defined TRANSPOSITION_TABLE_ENABLE) || (defined BOOK_LEARNING))
    // 哈希值
    HashValue hashValue {};
    memset(&hashValue, 0, sizeof(hashValue));

    // 哈希类型
    enum HashType hashf = hashfALPHA;

    // 获取哈希值
    hash_t hash = gameTemp.getHash();
#ifdef DEBUG_AB_TREE
    node->hash = hash;
#endif
#endif

#ifdef TRANSPOSITION_TABLE_ENABLE
    HashType type = hashfEMPTY;

    value_t probeVal = probeHash(hash, depth, alpha, beta, bestMove, type);

    if (probeVal != INT16_MIN /* TODO: valUNKOWN */  && node != rootNode) {
#ifdef TRANSPOSITION_TABLE_DEBUG
        hashHitCount++;
#endif
#ifdef DEBUG_AB_TREE
        node->isHash = true;
#endif
        node->value = probeVal;

#ifdef SORT_CONSIDER_PRUNED
        if (type != hashfEXACT && type != hashfEMPTY) {
            node->pruned = true;    // TODO: 是否有用?
        }
#endif

#if 0
        // TODO: 有必要针对深度微调 value?
        if (gameContext->turn == PLAYER1)
            node->value += hashValue.depth - depth;
        else
            node->value -= hashValue.depth - depth;
#endif

        return node->value;
}

    //hashMapMutex.unlock();
#endif /* TRANSPOSITION_TABLE_ENABLE */

#ifdef DEBUG_AB_TREE
    node->depth = depth;
    node->root = rootNode;
    // node->player = gameContext->turn;
    // 初始化
    node->isLeaf = false;
    node->isTimeout = false;
    node->visited = true;
#ifdef TRANSPOSITION_TABLE_ENABLE
    node->isHash = false;
    node->hash = 0;
#endif // TRANSPOSITION_TABLE_ENABLE
#endif // DEBUG_AB_TREE

    // 搜索到叶子节点（决胜局面） // TODO: 对哈希进行特殊处理
    if (gameContext->stage == GAME_OVER) {
        // 局面评估
        node->value = Evaluation::getValue(gameTemp, gameContext, node);
        evaluatedNodeCount++;

        // 为争取速胜，value 值 +- 深度
        if (node->value > 0) {
            node->value += depth;
        } else {
            node->value -= depth;
        }

#ifdef DEBUG_AB_TREE
        node->isLeaf = true;
#endif

#ifdef TRANSPOSITION_TABLE_ENABLE
        // 记录确切的哈希值
        recordHash(node->value, depth, hashfEXACT, hash, 0);
#endif

        return node->value;
    }

    // 搜索到第0层或需要退出
    if (!depth || requiredQuit) {
        // 局面评估
        node->value = Evaluation::getValue(gameTemp, gameContext, node);
        evaluatedNodeCount++;

        // 为争取速胜，value 值 +- 深度 (有必要?)
        if (gameContext->turn == PLAYER1) {
            node->value += depth;
        } else {
            node->value -= depth;
        }

#ifdef DEBUG_AB_TREE
        if (requiredQuit) {
            node->isTimeout = true;
        }
#endif

#ifdef BOOK_LEARNING
        // 检索开局库
        if (gameContext->stage == GAME_PLACING && findBookHash(hash, hashValue)) {
            if (gameContext->turn == PLAYER2) {
                // 是否需对后手扣分 // TODO: 先后手都处理
                node->value += 1;
            }
        }
#endif

#ifdef TRANSPOSITION_TABLE_ENABLE
        // 记录确切的哈希值
        recordHash(node->value, depth, hashfEXACT, hash, 0);
#endif

        return node->value;
    }

    // 生成子节点树，即生成每个合理的着法
    MoveList::generateLegalMoves(*this, gameTemp, node, rootNode, bestMove);

    // 根据演算模型执行 MiniMax 检索，对先手，搜索 Max, 对后手，搜索 Min

    minMax = gameTemp.whosTurn() == PLAYER1 ? -INF_VALUE : INF_VALUE;

    for (auto child : node->children) {
        // 上下文入栈保存，以便后续撤销着法
        contextStack.push(gameTemp.context);

        // 执行着法
        gameTemp.command(child->move);

#ifdef DEAL_WITH_HORIZON_EFFECT
        // 克服“水平线效应”: 若遇到吃子，则搜索深度增加
        if (child->pruned == false && child->move < 0) {
            epsilon = 1;
        }
        else {
            epsilon = 0;
        }
#endif // DEAL_WITH_HORIZON_EFFECT

#ifdef DEEPER_IF_ONLY_ONE_LEGAL_MOVE
        if (node->children.size() == 1)
            epsilon++;
#endif /* DEEPER_IF_ONLY_ONE_LEGAL_MOVE */

        // 递归 Alpha-Beta 剪枝
        value = alphaBetaPruning(depth - 1 + epsilon, alpha, beta, child);

        // 上下文弹出栈，撤销着法
        gameTemp.context = contextStack.top();
        contextStack.pop();

        if (gameTemp.whosTurn() == PLAYER1) {
            // 为走棋一方的层, 局面对走棋的一方来说是以 α 为评价

            // 取最大值
            minMax = std::max(value, minMax);

            // α 为走棋一方搜索到的最好值，任何比它小的值对当前结点的走棋方都没有意义
            // 如果某个着法的结果小于或等于 α，那么它就是很差的着法，因此可以抛弃

            if (value > alpha) {
#ifdef TRANSPOSITION_TABLE_ENABLE
                hashf = hashfEXACT;
#endif
                alpha = value;
            }

        } else {

            // 为走棋方的对手一方的层, 局面对对手一方来说是以 β 为评价

            // 取最小值
            minMax = std::min(value, minMax);

            // β 表示对手目前的劣势，这是对手所能承受的最坏结果
            // β 值越大，表示对手劣势越明显
            // 在对手看来，他总是会找到一个对策不比 β 更坏的
            // 如果当前结点返回 β 或比 β 更好的值，作为父结点的对方就绝对不会选择这种策略，
            // 如果搜索过程中返回 β 或比 β 更好的值，那就够好的了，走棋的一方就没有机会使用这种策略了。
            // 如果某个着法的结果大于或等于 β，那么整个结点就作废了，因为对手不希望走到这个局面，而它有别的着法可以避免到达这个局面。
            // 因此如果我们找到的评价大于或等于β，就证明了这个结点是不会发生的，因此剩下的合理着法没有必要再搜索。

            // TODO: 本意是要删掉这句，忘了删，结果反而棋力没有明显问题，待查
            // 如果删掉这句，启用下面这段代码，则三有时不会堵并且计算效率较低
            // 有了这句之后，hashf 不可能等于 hashfBETA
            beta = std::min(value, beta);

#if 0
            if (value < beta) {
#ifdef TRANSPOSITION_TABLE_ENABLE
                hashf = hashfBETA;
#endif
                beta = value;
            }
#endif
        }
#ifndef MIN_MAX_ONLY
        // 如果某个着法的结果大于 α 但小于β，那么这个着法就是走棋一方可以考虑走的
        // 否则剪枝返回
        if (alpha >= beta) {
#ifdef SORT_CONSIDER_PRUNED
            node->pruned = true;
#endif
            break;
        }
#endif /* !MIN_MAX_ONLY */
    }

    node->value = minMax;

#ifdef DEBUG_AB_TREE
    node->alpha = alpha;
    node->beta = beta;
#endif

    // 删除“孙子”节点，防止层数较深的时候节点树太大
#ifndef DONOT_DELETE_TREE
    for (auto child : node->children) {
        for (auto grandChild : child->children) {
            deleteTree(grandChild);
        }
        child->children.clear();
    }
#endif // DONOT_DELETE_TREE

#ifdef IDS_SUPPORT
    // 排序子节点树
    sortLegalMoves(node);
#endif // IDS_SUPPORT

#ifdef TRANSPOSITION_TABLE_ENABLE
    // 记录不一定确切的哈希值
    recordHash(node->value, depth, hashf, hash, node->children[0]->move);
#endif /* TRANSPOSITION_TABLE_ENABLE */

    // 返回
    return node->value;
}

const char* MillGameAi_ab::bestMove()
{
    vector<Node*> bestMoves;
    size_t bestMovesSize = 0;
    bool isMostLose = true; // 是否必败

    if ((rootNode->children).empty()) {
        return "error!";
    }

    loggerDebug("\n");
    loggerDebug("31 ----- 24 ----- 25\n");
    loggerDebug("| \\       |      / |\n");
    loggerDebug("|  23 -- 16 -- 17  |\n");
    loggerDebug("|  | \\    |   / |  |\n");
    loggerDebug("|  |  15-08-09  |  |\n");
    loggerDebug("30-22-14    10-18-26\n");
    loggerDebug("|  |  13-12-11  |  |\n");
    loggerDebug("|  | /    |   \\ |  |\n");
    loggerDebug("|  21 -- 20 -- 19  |\n");
    loggerDebug("| /       |      \\ |\n");
    loggerDebug("29 ----- 28 ----- 27\n");
    loggerDebug("\n");

    int i = 0;
    string moves = "moves";

    for (auto child : rootNode->children) {
        if (child->value == rootNode->value
#ifdef SORT_CONSIDER_PRUNED
            && !child->pruned
#endif
            ) {
            loggerDebug("[%.2d] %d\t%s\t%d *\n", i, child->move, move2string(child->move), child->value);
        } else {
            loggerDebug("[%.2d] %d\t%s\t%d\n", i, child->move, move2string(child->move), child->value);
        }

        i++;
    }

    // 检查是否必败

    Player whosTurn = game_.whosTurn();

    for (auto child : rootNode->children) {
        // TODO: 使用常量代替
        if ((whosTurn == PLAYER1 && child->value > -10000) ||
            (whosTurn == PLAYER2 && child->value < 10000)) {
            isMostLose = false;
            break;
        }
    }

    // 自动认输
    if (isMostLose) {
        if (whosTurn == PLAYER1) {
            sprintf(cmdline, "Player1 give up!");
        } else if (whosTurn == PLAYER2) {
            sprintf(cmdline, "Player2 give up!");
        }

        return cmdline;
    }

    for (auto child : rootNode->children) {
        if (child->value == rootNode->value) {
            bestMoves.push_back(child);
        }
    }

    bestMovesSize = bestMoves.size();

    if (bestMovesSize == 0) {
        loggerDebug("Not any child value is equal to root value\n");
        for (auto child : rootNode->children) {
            bestMoves.push_back(child);
        }
    }

    loggerDebug("Evaluated: %llu / %llu = %llu%%\n", evaluatedNodeCount, nodeCount, evaluatedNodeCount * 100 / nodeCount);

    nodeCount = 0;
    evaluatedNodeCount = 0;

#ifdef TRANSPOSITION_TABLE_ENABLE
#ifdef TRANSPOSITION_TABLE_DEBUG
    loggerDebug(""Hash hit count: %llu\n", hashHitCount);
#endif
#endif

    if (bestMoves.empty()) {
        return nullptr;
    }

    return move2string(bestMoves[0]->move);
}

const char *MillGameAi_ab::move2string(move_t move)
{
    int r, s;

    if (move < 0) {
        gameTemp.context.board.pos2rs(-move, r, s);
        sprintf(cmdline, "-(%1u,%1u)", r, s);
    } else if (move & 0x7f00) {
        int r1, s1;
        gameTemp.context.board.pos2rs(move >> 8, r1, s1);
        gameTemp.context.board.pos2rs(move & 0x00ff, r, s);
        sprintf(cmdline, "(%1u,%1u)->(%1u,%1u)", r1, s1, r, s);
    } else {
        gameTemp.context.board.pos2rs(move & 0x007f, r, s);
        sprintf(cmdline, "(%1u,%1u)", r, s);
    }

    return cmdline;
}

#ifdef TRANSPOSITION_TABLE_ENABLE
value_t MillGameAi_ab::probeHash(hash_t hash,
                                 depth_t depth, value_t alpha, value_t beta,
                                 move_t &bestMove, HashType &type)
{
    const value_t valUNKNOWN = INT16_MIN;
    HashValue hashValue {};

    if (!transpositionTable.find(hash, hashValue)) {
        return valUNKNOWN;
    }

    if (depth > hashValue.depth) {
        goto out;
    }

    type = hashValue.type;

    if (hashValue.type == hashfEXACT) {
        return hashValue.value;
    }

    if ((hashValue.type == hashfALPHA) && // 最多是 hashValue.value
        (hashValue.value <= alpha)) {
        return alpha;
    }

    if ((hashValue.type == hashfBETA) && // 至少是 hashValue.value
        (hashValue.value >= beta)) {
        return beta;
    }

out:
    bestMove = hashValue.bestMove;
    return valUNKNOWN;
}

bool MillGameAi_ab::findHash(hash_t hash, HashValue &hashValue)
{
    return transpositionTable.find(hash, hashValue);

    // TODO: 变换局面
#if 0
    if (iter != hashmap.end())
        return iter;

    // 变换局面，查找 hash (废弃)
    gameTempShift = gameTemp;
    for (int i = 0; i < 2; i++) {
        if (i)
            gameTempShift.mirror(false);

        for (int j = 0; j < 2; j++) {
            if (j)
                gameTempShift.turn(false);
            for (int k = 0; k < 4; k++) {
                gameTempShift.rotate(k * 90, false);
                iter = hashmap.find(gameTempShift.getHash());
                if (iter != hashmap.end())
                    return iter;
            }
        }
    }
#endif
}

int MillGameAi_ab::recordHash(value_t value, depth_t depth, HashType type, hash_t hash, move_t bestMove)
{
    // 同样深度或更深时替换
    // 注意: 每走一步以前都必须把散列表中所有的标志项置为 hashfEMPTY

    //hashMapMutex.lock();
    HashValue hashValue  {};
    memset(&hashValue, 0, sizeof(HashValue));

    if (findHash(hash, hashValue) &&
        hashValue.type != hashfEMPTY &&
        hashValue.depth > depth) {
        return -1;
    }

    hashValue.value = value;
    hashValue.depth = depth;
    hashValue.type = type;
    hashValue.bestMove = bestMove;

    transpositionTable.insert(hash, hashValue);

    //hashMapMutex.unlock();

    return 0;
}

void MillGameAi_ab::clearTranspositionTable()
{
    //hashMapMutex.lock();
    transpositionTable.clear();
    //hashMapMutex.unlock();
}
#endif /* TRANSPOSITION_TABLE_ENABLE */

#ifdef BOOK_LEARNING

bool MillGameAi_ab::findBookHash(hash_t hash, HashValue &hashValue)
{
    return bookHashMap.find(hash, hashValue);
}

int MillGameAi_ab::recordBookHash(hash_t hash, const HashValue &hashValue)
{
    //hashMapMutex.lock();
    bookHashMap.insert(hash, hashValue);
    //hashMapMutex.unlock();

    return 0;
}

void MillGameAi_ab::clearBookHashMap()
{
    //hashMapMutex.lock();
    bookHashMap.clear();
    //hashMapMutex.unlock();
}

void MillGameAi_ab::recordOpeningBookToHashMap()
{
    HashValue hashValue;
    hash_t hash = 0;

    for (auto iter = openingBook.begin(); iter != openingBook.end(); ++iter)
    {
#if 0
        if (findBookHash(*iter, hashValue))
        {
        }
#endif
        memset(&hashValue, 0, sizeof(HashValue));
        hash = *iter;
        recordBookHash(hash, hashValue);  // 暂时使用直接覆盖策略
    }

    openingBook.clear();
}

void MillGameAi_ab::recordOpeningBookHashMapToFile()
{
    const QString bookFileName = "opening-book.txt";
    bookHashMap.dump(bookFileName);
}

void MillGameAi_ab::loadOpeningBookFileToHashMap()
{
    const QString bookFileName = "opening-book.txt";
    bookHashMap.load(bookFileName);
}
#endif // BOOK_LEARNING