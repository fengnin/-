//这是实现了带自动补全功能的文本编辑器


#ifndef TEXTEDIT_H
#define TEXTEDIT_H
#include <QPlainTextEdit>   // 基础文本编辑器
#include <QCompleter>       // 自动补全功能
#include <QVector>          // 存储补全位置信息
class TextEdit : public QPlainTextEdit
{
    Q_OBJECT
signals:
    void sendmsg();     // 信号：触发消息发送（如按下回车键）
public:
    TextEdit(QWidget* parent = nullptr);
    void setCompletionList(const QStringList&); // 设置补全候选列表
    QString ToPlainText();//获取文本编辑器中的字符数
    void SetPlainText(QString);//用于当按发送键后清空文本编辑器中的内容
    void SetPlaceholderText(QString);//设置默认提示语，当文本编辑器中没有任何字符时触发，是qt内部QPlainTextEdit自动触发
protected:
    void keyPressEvent(QKeyEvent *e) override;//处理键盘的事件
    bool eventFilter(QObject* obj,QEvent* event) override; // 事件过滤器
    void mousePressEvent(QMouseEvent* e) override;      // 鼠标点击事件
private slots:
    void insertCompletionText(const QString&);//用于连接completer的activated弹出补全框后选中插入选中的文本
    void onTextChange();//用于连接edit编辑器变化时 触发处理文本判断是不是@
    void showCompletionPopup();//显示补全框
    void hideCompletionPopup();//隐藏
    void highLightCompletion(int start,int end);//设置补全高亮
    void removeCompletion();//移除补全内容
private:
    QCompleter* _completer;     // 自动补全核心对象
    //QPlainTextEdit* edit;
    QStringList _completionList;    // 补全候选词列表
    QVector<QPair<int,int>> _completionDistance;     // 记录补全文本的位置范围
    int _isCompletion;//记录当前文本框中有多少个补全文本
    int _lastTextLength;//记录上次文本的长度
    int _cursorPositionBeforeChange;//文本变化前的光标位置
};

#endif // TEXTEDIT_H
