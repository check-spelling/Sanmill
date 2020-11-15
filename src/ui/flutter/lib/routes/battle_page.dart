/*
  FlutterMill, a mill game playing frontend derived from ChessRoad
  Copyright (C) 2019 He Zhaoyun (ChessRoad author)
  Copyright (C) 2019-2020 Calcitem <calcitem@outlook.com>

  FlutterMill is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FlutterMill is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import 'package:flutter/material.dart';

import '../board/board_widget.dart';
import '../common/properties.dart';
import '../common/toast.dart';
import '../common/types.dart';
import '../engine/analysis.dart';
import '../engine/engine.dart';
import '../engine/native_engine.dart';
import '../game/battle.dart';
import '../main.dart';
import '../mill/mill.dart';
import '../services/player.dart';
import 'settings_page.dart';

class BattlePage extends StatefulWidget {
  //
  static double boardMargin = 10.0, screenPaddingH = 10.0;

  final EngineType engineType;
  final AiEngine engine;

  BattlePage(this.engineType) : engine = NativeEngine();

  @override
  _BattlePageState createState() => _BattlePageState();
}

class _BattlePageState extends State<BattlePage> {
  //
  String _status = '';
  bool _analysising = false;

  //static int flag = 0;

  @override
  void initState() {
    //
    super.initState();
    Battle.shared.init();

    widget.engine.startup();
  }

  changeStatus(String status) => setState(() => _status = status);

  onBoardTap(BuildContext context, int index) {
    //

    //Phase phase = Phase.placing;

    final position = Battle.shared.position;

    //position
    //flag++;
    //position.putPiece(flag % 2 == 0 ? '@' : 'O', index);
    int sq = indexToSquare[index];

    if (sq == null) {
      print("putPiece skip index: $index");
      return;
    }

    if (position.putPiece(sq) == false) {
      return;
    }

    // 仅 Position 中的 side 指示一方能动棋
    if (position.side != Color.black) return;

    final tapedPiece = position.pieceOnGrid(index);
    print("Tap piece $tapedPiece at <$index>");

    switch (position.phase) {
      case Phase.placing:
        engineToGo();
        break;
      case Phase.moving:
        // 之前已经有棋子被选中了
        if (Battle.shared.focusIndex != Move.invalidValue &&
            Color.of(position.pieceOnGrid(Battle.shared.focusIndex)) ==
                Color.black) {
          //
          // 当前点击的棋子和之前已经选择的是同一个位置
          if (Battle.shared.focusIndex == index) return;

          // 之前已经选择的棋子和现在点击的棋子是同一边的，说明是选择另外一个棋子
          final focusPiece = position.pieceOnGrid(Battle.shared.focusIndex);

          if (Color.isSameColor(focusPiece, tapedPiece)) {
            //
            Battle.shared.select(index);
            //
          } else if (Battle.shared.move(Battle.shared.focusIndex, index)) {
            // 现在点击的棋子和上一次选择棋子不同边，要么是吃子，要么是移动棋子到空白处
            final result = Battle.shared.scanBattleResult();

            switch (result) {
              case GameResult.pending:
                engineToGo();
                break;
              case GameResult.win:
                gotWin();
                break;
              case GameResult.lose:
                gotLose();
                break;
              case GameResult.draw:
                gotDraw();
                break;
            }
          }
          //
        } else {
          // 之前未选择棋子，现在点击就是选择棋子
          if (tapedPiece != Piece.noPiece) Battle.shared.select(index);
        }

        break;
      default:
        break;
    }

    setState(() {});
  }

  engineToGo() async {
    //
    changeStatus('对方思考中...');

    final response = await widget.engine.search(Battle.shared.position);

    if (response.type == 'move') {
      //
      Move mv = response.value;
      final Move move = new Move(mv.move);

      Battle.shared.move(move);

      final result = Battle.shared.scanBattleResult();

      switch (result) {
        case GameResult.pending:
          changeStatus('请走棋...');
          break;
        case GameResult.win:
          gotWin();
          break;
        case GameResult.lose:
          gotLose();
          break;
        case GameResult.draw:
          gotDraw();
          break;
      }
      //
    } else {
      //
      changeStatus('Error: ${response.type}');
    }
  }

  newGame() {
    //
    confirm() {
      Navigator.of(context).pop();
      Battle.shared.newGame();
      setState(() {});
    }

    cancel() => Navigator.of(context).pop();

    showDialog(
      context: context,
      builder: (BuildContext context) {
        return AlertDialog(
          title:
              Text('放弃对局？', style: TextStyle(color: Properties.primaryColor)),
          content: SingleChildScrollView(child: Text('你确定要放弃当前的对局吗？')),
          actions: <Widget>[
            FlatButton(child: Text('确定'), onPressed: confirm),
            FlatButton(child: Text('取消'), onPressed: cancel),
          ],
        );
      },
    );
  }

  analysisPosition() async {
    //
    Toast.toast(context, msg: '正在分析局面...', position: ToastPostion.bottom);

    setState(() => _analysising = true);

    try {} catch (e) {
      Toast.toast(context, msg: '错误: $e', position: ToastPostion.bottom);
    } finally {
      setState(() => _analysising = false);
    }
  }

  showAnalysisItems(
    BuildContext context, {
    String title,
    List<AnalysisItem> items,
    Function(AnalysisItem item) callback,
  }) {
    //
    final List<Widget> children = [];

    for (var item in items) {
      children.add(
        ListTile(
          title: Text(item.stepName, style: TextStyle(fontSize: 18)),
          subtitle: Text('胜率：${item.winrate}%'),
          trailing: Text('分数：${item.score}'),
          onTap: () => callback(item),
        ),
      );
      children.add(Divider());
    }

    children.insert(0, SizedBox(height: 10));
    children.add(SizedBox(height: 56));

    showModalBottomSheet(
      context: context,
      builder: (BuildContext context) => SingleChildScrollView(
        child: Column(mainAxisSize: MainAxisSize.min, children: children),
      ),
    );
  }

  void gotWin() {
    //
    Battle.shared.position.result = GameResult.win;
    //Audios.playTone('win.mp3');

    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (BuildContext context) {
        return AlertDialog(
          title: Text('赢了', style: TextStyle(color: Properties.primaryColor)),
          content: Text('恭喜您取得了伟大的胜利！'),
          actions: <Widget>[
            FlatButton(child: Text('再来一盘'), onPressed: newGame),
            FlatButton(
                child: Text('关闭'),
                onPressed: () => Navigator.of(context).pop()),
          ],
        );
      },
    );

    if (widget.engineType == EngineType.Cloud)
      Player.shared.increaseWinCloudEngine();
    else
      Player.shared.increaseWinPhoneAi();
  }

  void gotLose() {
    //
    Battle.shared.position.result = GameResult.lose;
    //Audios.playTone('lose.mp3');

    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (BuildContext context) {
        return AlertDialog(
          title: Text('输了', style: TextStyle(color: Properties.primaryColor)),
          content: Text('勇士！坚定战斗，虽败犹荣！'),
          actions: <Widget>[
            FlatButton(child: Text('再来一盘'), onPressed: newGame),
            FlatButton(
                child: Text('关闭'),
                onPressed: () => Navigator.of(context).pop()),
          ],
        );
      },
    );
  }

  void gotDraw() {
    //
    Battle.shared.position.result = GameResult.draw;

    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (BuildContext context) {
        return AlertDialog(
          title: Text('和���', style: TextStyle(color: Properties.primaryColor)),
          content: Text('您用自己的力量捍卫了和平！'),
          actions: <Widget>[
            FlatButton(child: Text('再来一盘'), onPressed: newGame),
            FlatButton(
                child: Text('关闭'),
                onPressed: () => Navigator.of(context).pop()),
          ],
        );
      },
    );
  }

  void calcScreenPaddingH() {
    //
    // 当屏幕的纵横比小于16/9时，限制棋盘的宽度
    final windowSize = MediaQuery.of(context).size;
    double height = windowSize.height, width = windowSize.width;

    if (height / width < 16.0 / 9.0) {
      width = height * 9 / 16;
      BattlePage.screenPaddingH =
          (windowSize.width - width) / 2 - BattlePage.boardMargin;
    }
  }

  Widget createPageHeader() {
    //
    final titleStyle =
        TextStyle(fontSize: 28, color: Properties.darkTextPrimaryColor);
    final subTitleStyle =
        TextStyle(fontSize: 16, color: Properties.darkTextSecondaryColor);

    return Container(
      margin: EdgeInsets.only(top: SanmillApp.StatusBarHeight),
      child: Column(
        children: <Widget>[
          Row(
            children: <Widget>[
              IconButton(
                icon: Icon(Icons.arrow_back,
                    color: Properties.darkTextPrimaryColor),
                onPressed: () => Navigator.of(context).pop(),
              ),
              Expanded(child: SizedBox()),
              Hero(tag: 'logo', child: Image.asset('images/logo-mini.png')),
              SizedBox(width: 10),
              Text(widget.engineType == EngineType.Cloud ? '挑战云主机' : '人机对战',
                  style: titleStyle),
              Expanded(child: SizedBox()),
              IconButton(
                icon: Icon(Icons.settings,
                    color: Properties.darkTextPrimaryColor),
                onPressed: () => Navigator.of(context).push(
                  MaterialPageRoute(builder: (context) => SettingsPage()),
                ),
              ),
            ],
          ),
          Container(
            height: 4,
            width: 180,
            margin: EdgeInsets.only(bottom: 10),
            decoration: BoxDecoration(
              color: Properties.boardBackgroundColor,
              borderRadius: BorderRadius.circular(2),
            ),
          ),
          Container(
            padding: EdgeInsets.symmetric(horizontal: 16),
            child: Text(_status, maxLines: 1, style: subTitleStyle),
          ),
        ],
      ),
    );
  }

  Widget createBoard() {
    //
    return Container(
      margin: EdgeInsets.symmetric(
        horizontal: BattlePage.screenPaddingH,
        vertical: BattlePage.boardMargin,
      ),
      child: BoardWidget(
        width:
            MediaQuery.of(context).size.width - BattlePage.screenPaddingH * 2,
        onBoardTap: onBoardTap,
      ),
    );
  }

  Widget createOperatorBar() {
    //
    final buttonStyle = TextStyle(color: Properties.primaryColor, fontSize: 20);

    return Container(
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(5),
        color: Properties.boardBackgroundColor,
      ),
      margin: EdgeInsets.symmetric(horizontal: BattlePage.screenPaddingH),
      padding: EdgeInsets.symmetric(vertical: 2),
      child: Row(children: <Widget>[
        Expanded(child: SizedBox()),
        FlatButton(child: Text('新对局', style: buttonStyle), onPressed: newGame),
        Expanded(child: SizedBox()),
        FlatButton(
          child: Text('悔棋', style: buttonStyle),
          onPressed: () {
            Battle.shared.regret(steps: 2);
            setState(() {});
          },
        ),
        Expanded(child: SizedBox()),
        FlatButton(
          child: Text('分析局面', style: buttonStyle),
          onPressed: _analysising ? null : analysisPosition,
        ),
        Expanded(child: SizedBox()),
      ]),
    );
  }

  Widget buildFooter() {
    //
    final size = MediaQuery.of(context).size;

    final manualText = Battle.shared.position.manualText;

    if (size.height / size.width > 16 / 9) {
      return buildManualPanel(manualText);
    } else {
      return buildExpandableManaulPanel(manualText);
    }
  }

  Widget buildManualPanel(String text) {
    //
    final manualStyle = TextStyle(
      fontSize: 18,
      color: Properties.darkTextSecondaryColor,
      height: 1.5,
    );

    return Expanded(
      child: Container(
        margin: EdgeInsets.symmetric(vertical: 16),
        child: SingleChildScrollView(child: Text(text, style: manualStyle)),
      ),
    );
  }

  Widget buildExpandableManaulPanel(String text) {
    //
    final manualStyle = TextStyle(fontSize: 18, height: 1.5);

    return Expanded(
      child: IconButton(
        icon: Icon(Icons.expand_less, color: Properties.darkTextPrimaryColor),
        onPressed: () => showDialog(
          context: context,
          barrierDismissible: false,
          builder: (BuildContext context) {
            return AlertDialog(
              title:
                  Text('棋谱', style: TextStyle(color: Properties.primaryColor)),
              content:
                  SingleChildScrollView(child: Text(text, style: manualStyle)),
              actions: <Widget>[
                FlatButton(
                  child: Text('好的'),
                  onPressed: () => Navigator.of(context).pop(),
                ),
              ],
            );
          },
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    //
    calcScreenPaddingH();

    final header = createPageHeader();
    final board = createBoard();
    final operatorBar = createOperatorBar();
    final footer = buildFooter();

    return Scaffold(
      backgroundColor: Properties.darkBackgroundColor,
      body: Column(children: <Widget>[header, board, operatorBar, footer]),
    );
  }

  @override
  void dispose() {
    widget.engine.shutdown();
    super.dispose();
  }
}
