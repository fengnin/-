//实现了一个带有自动补全功能的文本编辑器控件 TextEdit，继承自 QPlainTextEdit

#include "textedit.h"
#include <QStringListModel>     // 用于字符串列表模型
#include <QVBoxLayout>          // 垂直布局
#include <QAbstractItemView>    // 抽象项视图（用于补全列表）
#include <QDesktopWidget>       // 桌面小部件
#include <QDebug>               // 调试输出 
#include <QApplication>         // Qt 应用核心

// 构造函数，初始化 TextEdit 对象
TextEdit::TextEdit(QWidget* parent)
    :QPlainTextEdit(parent)     // 调用基类构造函数
{
    qDebug()<<"TextEdit";       // 调试输出，表示对象创建
    QVBoxLayout* layout = new QVBoxLayout(this);        // 创建垂直布局
    layout->setContentsMargins(0,0,0,0);        // 设置布局边距为 0
    this->setLayout(layout);        // 设置布局
    // 连接文本变化信号到槽函数
    connect(this,SIGNAL(textChanged()),this,SLOT(onTextChange()));
    _completer = nullptr;       // 初始化补全器为空
    this->installEventFilter(this);     // 安装事件过滤器
    _isCompletion = 0;          // 补全计数器初始化为 0
    _lastTextLength = 0;        // 记录上一次文本长度
}
// 设置补全列表
void TextEdit::setCompletionList(const QStringList& completionList)
{
    _completionList = completionList;   // 保存补全列表  
    if(_completer)  // 如果补全器已存在
    {
        delete _completer->model();     // 删除旧模型
    }
    else  // 如果补全器不存在
    {
        _completer = new QCompleter(this);      // 创建新补全器
        _completer->setWidget(this);            // 设置补全器关联的控件
        _completer->setCaseSensitivity(Qt::CaseInsensitive);//不区分大小写
        _completer->setCompletionMode(QCompleter::PopupCompletion);//弹出补全
        //_completionList.push_back("@123456");
        // 连接补全激活信号到槽函数
        connect(_completer,SIGNAL(activated(QString)),this,SLOT(insertCompletionText(QString)));
    }
    // 创建新模型并设置给补全器
    QStringListModel* model = new QStringListModel(_completionList,_completer);
    _completer->setModel(model);
}
// 获取纯文本内容
QString TextEdit::ToPlainText()
{
    return this->toPlainText();     // 调用基类方法
}
// 设置纯文本内容
void TextEdit::SetPlainText(QString text)
{
    this->setPlainText(text);       // 调用基类方法
}
// 设置占位文本
void TextEdit::SetPlaceholderText(QString text)
{
    this->setPlaceholderText(text);     // 调用基类方法
}
// 键盘按键事件处理
void TextEdit::keyPressEvent(QKeyEvent *event)
{
    _cursorPositionBeforeChange = this->textCursor().position();    //记录光标位置
    //如果补全框显示时，就将补全框选择内容插入到文本中  // 处理回车键
    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return))
    {
        qDebug()<<"keyPressEvent";      // 调试输出
        if(_completer->popup()->isHidden())
        {
            //如果补全框不显示，就发送信号，到主类发送信息
            qDebug()<<"发送sendmsg";      // 调试输出
            emit sendmsg();     // 发送信号
            return;             
        }
        else  //如果补全列表显示
        {
            // 处理回车键    // 获取当前选中的补全项
            QModelIndex index = _completer->popup()->currentIndex();
            QString completion = index.data().toString();
            insertCompletionText(completion);   // 插入补全文本
            return;
        }
    }


    // 调用父类的 keyPressEvent 处理其他按键
    QPlainTextEdit::keyPressEvent(event);
}
// 事件过滤器，用于处理特定事件
bool TextEdit::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == this)    // 如果事件来自当前对象
    {
        if (event->type() == QEvent::KeyPress)      // 如果是按键事件
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            // 只处理特定按键（如方向键、退格键等）
            if (keyEvent->key() == Qt::Key_Backspace || keyEvent->key() == Qt::Key_Delete)
            {
                // 处理逻辑
                if (_isCompletion)  // 如果有补全内容
                {
                    QTextCursor tc = this->textCursor();    // 获取文本光标
                    int pos = tc.position();        // 获取光标位置
                    // 遍历所有补全范围
                    for (int i = 0; i < _completionDistance.size(); i++)
                    {
                        // 如果光标在补全范围内且按下退格/删除
                        if ((keyEvent->key() == Qt::Key_Backspace || keyEvent->key() == Qt::Key_Delete) &&
                            pos == _completionDistance[i].second)
                        {
                            // 删除补全内容
                            _isCompletion--;    // 减少补全计数
                            tc.setPosition(_completionDistance[i].first);   // 移动到补全开始位置
                            tc.setPosition(_completionDistance[i].second, QTextCursor::KeepAnchor); // 选中补全文本
                            tc.removeSelectedText();    // 删除选中文本
                            this->setTextCursor(tc);    // 设置光标位置
                            //return true; // 拦截事件
                        }
                        // 如果光标在补全范围内但未到末尾
                        else if (pos >= _completionDistance[i].first && pos < _completionDistance[i].second)
                        {
                            // 移动光标到补全内容末尾
                            tc.setPosition(_completionDistance[i].second);
                            this->setTextCursor(tc);    // 设置光标位置
                            //return true; // 拦截事件
                        }
                    }
                }
            }
        }
    }

    // 其他事件交给父类处理
    return QPlainTextEdit::eventFilter(obj, event);
}
// 鼠标按下事件处理
void TextEdit::mousePressEvent(QMouseEvent *event)
{
    _cursorPositionBeforeChange = this->textCursor().position();    //记录光标位置
    if(event->button() == Qt::LeftButton)   // 如果是左键点击
    {
        QTextCursor tc = this->cursorForPosition(event->pos());//获取鼠标光标
        int pos = tc.position();    // 获取光标位置
        // 遍历所有补全范围
        for(int i = 0;i < _completionDistance.size();i++)
        {
            // 如果点击位置在补全范围内
            if(pos >= _completionDistance[i].first && pos <= _completionDistance[i].second)
            {
                //qDebug()<<"光标移出补全内容";
                tc.setPosition(_completionDistance[i].second);  // 移动到补全末尾
                //qDebug()<<"pos : "<<pos;
                //qDebug()<<"_completionDistance[i].second : "<<_completionDistance[i].second;
                this->setTextCursor(tc);        // 设置光标位置
                return;
            }
        }
    }
    // 调用基类处理其他鼠标事件
    QPlainTextEdit::mousePressEvent(event);
}

//弹出补全列表后选中一个选项 调用 -> 将其插入到文本中
void TextEdit::insertCompletionText(const QString& completion)
{
    //插入补全
    //qDebug()<<"completion : "<<completion;
    QTextCursor tc = textCursor();      // 获取当前文本光标
    // 计算需要插入的文本长度（补全文本长度 - 用户已输入的前缀长度）
    int len = completion.length() - _completer->completionPrefix().length();//_completer->completionPrefix()获取用户已经输入的前缀
    //qDebug()<<"_completer->completionPrefix() : "<<_completer->completionPrefix();
    tc.movePosition(QTextCursor::Left);     // 光标左移
    tc.movePosition(QTextCursor::EndOfWord);    // 移动到单词末尾
    //qDebug()<<"len : "<<len;
    //qDebug()<<"completion.right(len) : "<<completion.right(len);
    tc.insertText(completion.right(len));       // 插入补全文本的后半部分

    _isCompletion++;    // 增加补全计数

    //设置高光
    int start = tc.position() - completion.length();        // 高亮开始位置
    int end = tc.position();        // 高亮结束位置
    highLightCompletion(start,end); // 高亮显示补全文本
    _completionDistance.push_back({start,end + 1});         // 记录补全范围
    hideCompletionPopup();          // 隐藏补全列表
}
// 文本变化时的处理
void TextEdit::onTextChange()
{
    //qDebug()<<"onTextChange";
    if(_completer == nullptr)       // 如果补全器不存在
        return;
    if(_completer->completionCount() == 0)      // 如果补全列表为空
    {
        //qDebug()<<"补齐列表为空";
        return;
    }
    QString curtext = ToPlainText();        // 获取当前文本
    bool isShow = false;                    // 是否显示补全列表的标志

    // 动态更新 _completionDistance
    int delta = curtext.length() - _lastTextLength; // 计算文本长度变化
    _lastTextLength = curtext.length(); // 更新上一次的文本长度
    // 更新补全范围
    if (delta != 0)
    {
        // 遍历 _completionDistance，更新位置信息
        for (int i = 0; i < _completionDistance.size(); i++)
        {
            if (_cursorPositionBeforeChange < _completionDistance[i].first)
            {
                // 如果插入/删除位置在当前补全内容之前，调整当前补全内容的位置
                _completionDistance[i].first += delta;  // 调整开始位置
                _completionDistance[i].second += delta; // 调整结束位置
            }   
        }
    }
    //qDebug()<<"curtext : "<<curtext;
    // 查找最后一个 '@' 符号的位置
    int index = curtext.lastIndexOf('@');
    if(index == -1) // 如果没有 '@'
    {
        hideCompletionPopup();  // 隐藏补全列表
        return; // 直接返回
    }
    QString prefix = curtext.mid(index);//取出@后面的字符串
    _completer->setCompletionPrefix(prefix);    // 设置补全前缀
    QAbstractItemView* view = _completer->popup();  // 获取补全列表视图
    view->setCurrentIndex(_completer->completionModel()->index(0,0));   // 设置默认选中项
    if(curtext.endsWith('@'))//如果存在@
    {
        QTextCursor tc = textCursor();  // 获取文本光标
        QRect tcRect = cursorRect(tc);  // 获取光标矩形
        // 设置补全列表弹出位置
        QRect popupRect(QPoint(tcRect.bottomRight().x(),tcRect.bottomRight().y() - 100),QSize(200,100));
        _completer->complete(popupRect);
        //qDebug()<<"存在@";
        showCompletionPopup();
        //qDebug()<<"_completionList : "<<_completionList;
    }
    else//当末尾不是@判断是不是列表中的前缀 是的话不关闭补全列表 不是关闭
    {
        //qDebug()<<"prefix : "<<prefix;
        //qDebug()<<"_completionList : "<<_completionList;
        // 检查当前前缀是否在补全列表中
        for(int i = 0;i < _completionList.size();i++)
        {
            if(_completionList[i].contains(prefix))     // 如果包含前缀
                isShow = true;      // 设置显示标志
        }
        if(isShow)      // 如果需要显示
        {
            //qDebug()<<"showCompletionPopup";
            showCompletionPopup();  // 显示补全列表
        }
        else
        {
            //qDebug()<<"hideCompletionPopup";
            hideCompletionPopup();  // 隐藏补全列表
        }
    }
}
// 显示补全列表
void TextEdit::showCompletionPopup()
{
    _completer->popup()->show();
}
// 隐藏补全列表
void TextEdit::hideCompletionPopup()
{
    _completer->popup()->hide();
}
// 高亮显示补全文本
void TextEdit::highLightCompletion(int start, int end)
{
    //qDebug()<<"start : "<<start;
    //qDebug()<<"end : "<<end;
    QTextCursor tc = textCursor();      // 获取文本光标
    tc.setPosition(start);      // 移动到开始位置
    tc.setPosition(end,QTextCursor::KeepAnchor);    // 选中到结束位置

    QTextCharFormat tf = tc.charFormat();   // 获取当前字符格式
    QTextCharFormat defaultFT = tf; // 保存默认格式
    tf.setForeground(Qt::white);    // 设置前景色为白色
    tf.setBackground(QBrush(QColor(0, 160, 233)));  // 设置背景色为蓝色
    tc.setCharFormat(tf);       // 应用格式
    tc.clearSelection();        // 清除选中
    tc.setCharFormat(defaultFT);    // 恢复默认格式
    tc.insertText(" ");         // 插入一个空格
    this->setTextCursor(tc);    // 设置光标位置
    // QTextCharFormat defaultFT;
    // tc.setCharFormat(defaultFT);
    // tc.clearSelection();
}

void TextEdit::removeCompletion()
{

}

