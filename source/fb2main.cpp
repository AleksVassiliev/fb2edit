#include <QtGui>
#include <QtDebug>
#include <QTreeView>
#include <QWebInspector>
#include <QWebFrame>

#include "fb2main.hpp"
#include "fb2code.hpp"
#include "fb2read.hpp"
#include "fb2tree.hpp"
#include "fb2view.hpp"
#include "fb2head.hpp"

#define FB2DELETE(p) { if ( (p) != NULL ) { delete p; p = NULL; } }

Fb2MainWindow::Fb2MainWindow()
{
    init();
    setCurrentFile();
    viewText();
    textEdit->load(":blank.fb2");
}

Fb2MainWindow::Fb2MainWindow(const QString &filename, ViewMode mode)
{
    init();
    setCurrentFile(filename);
    if (mode == FB2) {
        viewText();
        textEdit->load(filename);
    } else {
        viewCode();
        loadXML(filename);
    }
}

void Fb2MainWindow::init()
{
    connect(qApp, SIGNAL(logMessage(QString)), SLOT(logMessage(QString)));

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(":icon.ico"));

    isUntitled = true;

    createActions();
    createStatusBar();

    textEdit = NULL;
    noteEdit = NULL;
    codeEdit = NULL;
    treeView = NULL;
    headTree = NULL;
    toolEdit = NULL;
    messageEdit = NULL;

    readSettings();

    setUnifiedTitleAndToolBarOnMac(true);
}

void Fb2MainWindow::logMessage(const QString &message)
{
    if (!messageEdit) {
        messageEdit = new QTextEdit(this);
        connect(messageEdit, SIGNAL(destroyed()), SLOT(logDestroyed()));
        QDockWidget * dock = new QDockWidget(tr("Message log"), this);
        dock->setAttribute(Qt::WA_DeleteOnClose);
        dock->setFeatures(QDockWidget::AllDockWidgetFeatures);
        dock->setWidget(messageEdit);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        messageEdit->setMaximumHeight(80);
    }
    messageEdit->append(message);
}

void Fb2MainWindow::logShowed()
{
    messageEdit->setMaximumHeight(QWIDGETSIZE_MAX);
}

void Fb2MainWindow::logDestroyed()
{
    messageEdit = NULL;
}

void Fb2MainWindow::treeActivated(const QModelIndex &index)
{
    if (!treeView) return;
    Fb2TreeModel *model = dynamic_cast<Fb2TreeModel*>(treeView->model());
    if (model) model->select(index);
}

void Fb2MainWindow::treeDestroyed()
{
    treeView = NULL;
    dockTree = NULL;
}

bool Fb2MainWindow::loadXML(const QString &filename)
{
    if (!filename.isEmpty()) {
        QFile file(filename);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            codeEdit->clear();
            return codeEdit->read(&file);
        }
    }
    return false;
}

void Fb2MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void Fb2MainWindow::fileNew()
{
    Fb2MainWindow *other = new Fb2MainWindow;
    other->move(x() + 40, y() + 40);
    other->show();
}

void Fb2MainWindow::fileOpen()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open file"), QString(), "Fiction book files (*.fb2)");
    if (filename.isEmpty()) return;

    Fb2MainWindow * existing = findFb2MainWindow(filename);
    if (existing) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }

    if (textEdit) {
        if (isUntitled && !isWindowModified()) {
            setCurrentFile(filename);
            textEdit->load(filename);
        } else {
            Fb2MainWindow * other = new Fb2MainWindow(filename, FB2);
            other->move(x() + 40, y() + 40);
            other->show();
        }
    } else if (codeEdit) {
        if (isUntitled && !isWindowModified()) {
            setCurrentFile(filename);
            loadXML(filename);
        } else {
            Fb2MainWindow * other = new Fb2MainWindow(filename, XML);
            other->move(x() + 40, y() + 40);
            other->show();
        }
    }
}

bool Fb2MainWindow::fileSave()
{
    if (isUntitled) {
        return fileSaveAs();
    } else {
        return saveFile(curFile);
    }
}

bool Fb2MainWindow::fileSaveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As..."), curFile);
    if (fileName.isEmpty()) return false;
    return saveFile(fileName);
}

void Fb2MainWindow::about()
{
    QMessageBox::about(this, tr("About fb2edit"),
        tr("The <b>fb2edit</b> is application for editing FB2-files."));
}

void Fb2MainWindow::documentWasModified()
{
    if (isWindowModified()) return;
    QFileInfo info = windowFilePath();
    QString title = info.fileName();
    title += QString("[*]");
    title += QString(" - ") += qApp->applicationName();
    setWindowTitle(title);
    setWindowModified(true);
    if (codeEdit) disconnect(codeEdit, SIGNAL(textChanged()), this, SLOT(documentWasModified()));
    if (textEdit) disconnect(textEdit->page(), SIGNAL(contentsChanged()), this, SLOT(documentWasModified()));
}

QIcon Fb2MainWindow::icon(const QString &name)
{
    QIcon icon;
    icon.addFile(QString(":/24x24/%1.png").arg(name), QSize(24,24));
    icon.addFile(QString(":/16x24/%1.png").arg(name), QSize(16,16));
    return QIcon::fromTheme(name, icon);
}

void Fb2MainWindow::createActions()
{
    QAction * act;
    QMenu * menu;
    QToolBar * tool;
    QList<QAction*> actions;

    menu = menuBar()->addMenu(tr("&File"));
    tool = addToolBar(tr("File"));
    tool->setMovable(false);

    act = new QAction(icon("document-new"), tr("&New"), this);
    act->setPriority(QAction::LowPriority);
    act->setShortcuts(QKeySequence::New);
    act->setStatusTip(tr("Create a new file"));
    connect(act, SIGNAL(triggered()), this, SLOT(fileNew()));
    menu->addAction(act);
    tool->addAction(act);

    act = new QAction(icon("document-open"), tr("&Open..."), this);
    act->setShortcuts(QKeySequence::Open);
    act->setStatusTip(tr("Open an existing file"));
    connect(act, SIGNAL(triggered()), this, SLOT(fileOpen()));
    menu->addAction(act);
    tool->addAction(act);

    act = new QAction(icon("document-save"), tr("&Save"), this);
    act->setShortcuts(QKeySequence::Save);
    act->setStatusTip(tr("Save the document to disk"));
    connect(act, SIGNAL(triggered()), this, SLOT(fileSave()));
    menu->addAction(act);
    tool->addAction(act);

    act = new QAction(icon("document-save-as"), tr("Save &As..."), this);
    act->setShortcuts(QKeySequence::SaveAs);
    act->setStatusTip(tr("Save the document under a new name"));
    connect(act, SIGNAL(triggered()), this, SLOT(fileSaveAs()));
    menu->addAction(act);

    menu->addSeparator();

    act = new QAction(icon("window-close"), tr("&Close"), this);
    act->setShortcuts(QKeySequence::Close);
    act->setStatusTip(tr("Close this window"));
    connect(act, SIGNAL(triggered()), this, SLOT(close()));
    menu->addAction(act);

    act = new QAction(icon("application-exit"), tr("E&xit"), this);
    act->setShortcuts(QKeySequence::Quit);
    act->setStatusTip(tr("Exit the application"));
    connect(act, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));
    menu->addAction(act);

    menuEdit = menu = menuBar()->addMenu(tr("&Edit"));

    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(clipboardDataChanged()));

    actionUndo = act = new QAction(icon("edit-undo"), tr("&Undo"), this);
    act->setPriority(QAction::LowPriority);
    act->setShortcut(QKeySequence::Undo);
    act->setEnabled(false);
    menu->addAction(act);

    actionRedo = act = new QAction(icon("edit-redo"), tr("&Redo"), this);
    act->setPriority(QAction::LowPriority);
    act->setShortcut(QKeySequence::Redo);
    act->setEnabled(false);
    menu->addAction(act);

    menu->addSeparator();

    actionCut = act = new QAction(icon("edit-cut"), tr("Cu&t"), this);
    act->setPriority(QAction::LowPriority);
    act->setShortcuts(QKeySequence::Cut);
    act->setStatusTip(tr("Cut the current selection's contents to the clipboard"));
    act->setEnabled(false);
    menu->addAction(act);

    actionCopy = act = new QAction(icon("edit-copy"), tr("&Copy"), this);
    act->setPriority(QAction::LowPriority);
    act->setShortcuts(QKeySequence::Copy);
    act->setStatusTip(tr("Copy the current selection's contents to the clipboard"));
    act->setEnabled(false);
    menu->addAction(act);

    actionPaste = act = new QAction(icon("edit-paste"), tr("&Paste"), this);
    act->setPriority(QAction::LowPriority);
    act->setShortcuts(QKeySequence::Paste);
    act->setStatusTip(tr("Paste the clipboard's contents into the current selection"));
    menu->addAction(act);
    clipboardDataChanged();

    menu->addSeparator();

    actionFind = act = new QAction(icon("edit-find"), tr("&Find..."), this);
    act->setShortcuts(QKeySequence::Find);
    menu->addAction(act);

    actionReplace = act = new QAction(icon("edit-find-replace"), tr("&Replace..."), this);
    menu->addAction(act);

    menu->addSeparator();

    actionInsert = act = new QAction(icon("list-add"), tr("&Append"), this);
    act->setPriority(QAction::LowPriority);
    act->setShortcuts(QKeySequence::New);
    menu->addAction(act);

    actionDelete = act = new QAction(icon("list-remove"), tr("&Delete"), this);
    act->setPriority(QAction::LowPriority);
    act->setShortcuts(QKeySequence::Delete);
    menu->addAction(act);

    menu->addSeparator();

    act = new QAction(icon("preferences-desktop"), tr("&Settings"), this);
    act->setShortcuts(QKeySequence::Preferences);
    act->setStatusTip(tr("Application settings"));
    connect(act, SIGNAL(triggered()), SLOT(openSettings()));
    menu->addAction(act);

    menu = menuBar()->addMenu(tr("&Insert", "Main menu"));

    actionImage = act = new QAction(icon("insert-image"), tr("&Image"), this);
    menu->addAction(act);

    actionNote = act = new QAction(icon("insert-text"), tr("&Footnote"), this);
    menu->addAction(act);

    actionLink = act = new QAction(icon("insert-link"), tr("&Hiperlink"), this);
    menu->addAction(act);

    actionBody = act = new QAction(icon("insert-object"), tr("&Body"), this);
    menu->addAction(act);

    menuText = menu = menuBar()->addMenu(tr("Fo&rmat"));

    actionTextBold = act = new QAction(icon("format-text-bold"), tr("&Bold"), this);
    act->setShortcuts(QKeySequence::Bold);
    act->setCheckable(true);
    menu->addAction(act);

    actionTextItalic = act = new QAction(icon("format-text-italic"), tr("&Italic"), this);
    act->setShortcuts(QKeySequence::Italic);
    act->setCheckable(true);
    menu->addAction(act);

    actionTextStrike = act = new QAction(icon("format-text-strikethrough"), tr("&Strikethrough"), this);
    act->setCheckable(true);
    menu->addAction(act);

    actionTextSup = act = new QAction(icon("format-text-superscript"), tr("Su&perscript"), this);
    act->setCheckable(true);
    menu->addAction(act);

    actionTextSub = act = new QAction(icon("format-text-subscript"), tr("Su&bscript"), this);
    act->setCheckable(true);
    menu->addAction(act);

    menuView = menu = menuBar()->addMenu(tr("&View"));

    tool->addSeparator();

    QActionGroup * viewGroup = new QActionGroup(this);

    act = new QAction(tr("&Text"), this);
    act->setCheckable(true);
    act->setChecked(true);
    connect(act, SIGNAL(triggered()), this, SLOT(viewText()));
    viewGroup->addAction(act);
    menu->addAction(act);
    tool->addAction(act);

    act = new QAction(tr("&Head"), this);
    act->setCheckable(true);
    connect(act, SIGNAL(triggered()), this, SLOT(viewHead()));
    viewGroup->addAction(act);
    menu->addAction(act);
    tool->addAction(act);

    act = new QAction(tr("&XML"), this);
    act->setCheckable(true);
    connect(act, SIGNAL(triggered()), this, SLOT(viewCode()));
    viewGroup->addAction(act);
    menu->addAction(act);
    tool->addAction(act);

    menu->addSeparator();

    actionZoomIn = act = new QAction(icon("zoom-in"), tr("Zoom in"), this);
    act->setShortcuts(QKeySequence::ZoomIn);
    menu->addAction(act);

    actionZoomOut = act = new QAction(icon("zoom-out"), tr("Zoom out"), this);
    act->setShortcuts(QKeySequence::ZoomOut);
    menu->addAction(act);

    actionZoomOrig = act = new QAction(icon("zoom-original"), tr("Zoom original"), this);
    menu->addAction(act);

    menu->addSeparator();

    act = new QAction(tr("&Contents"), this);
    connect(act, SIGNAL(triggered()), this, SLOT(viewTree()));
    menu->addAction(act);

    act = new QAction(tr("&Web inspector"), this);
    connect(act, SIGNAL(triggered()), this, SLOT(showInspector()));
    menu->addAction(act);

    menuBar()->addSeparator();
    menu = menuBar()->addMenu(tr("&Help"));

    act = new QAction(icon("help-about"), tr("&About"), this);
    act->setStatusTip(tr("Show the application's About box"));
    connect(act, SIGNAL(triggered()), this, SLOT(about()));
    menu->addAction(act);

    act = new QAction(tr("About &Qt"), this);
    act->setStatusTip(tr("Show the Qt library's About box"));
    connect(act, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    menu->addAction(act);
}

void Fb2MainWindow::openSettings()
{
    QMessageBox::about(this, tr("Settings"),
        tr("The <b>fb2edit</b> is application for editing FB2-files."));
}

void Fb2MainWindow::createTree()
{
    if (treeView) return;
    treeView = new QTreeView(this);
    treeView->setHeaderHidden(true);
    connect(treeView, SIGNAL(activated(QModelIndex)), SLOT(treeActivated(QModelIndex)));
    connect(treeView, SIGNAL(destroyed()), SLOT(treeDestroyed()));
    dockTree = new QDockWidget(tr("Contents"), this);
    dockTree->setAttribute(Qt::WA_DeleteOnClose);
    dockTree->setFeatures(QDockWidget::AllDockWidgetFeatures);
    dockTree->setWidget(treeView);
    addDockWidget(Qt::LeftDockWidgetArea, dockTree);
    treeView->setFocus();
}

void Fb2MainWindow::loadFinished(bool ok)
{
    if (headTree) {
        Fb2HeadModel *model = new Fb2HeadModel(*textEdit, treeView);
        headTree->setModel(model);
        model->expand(headTree);
    }
    if (treeView) {
        Fb2TreeModel *model = new Fb2TreeModel(*textEdit, treeView);
        treeView->setModel(model);
        model->expand(treeView);
    }
}

void Fb2MainWindow::selectionChanged()
{
    actionCut->setEnabled(textEdit->CutEnabled());
    actionCopy->setEnabled(textEdit->CopyEnabled());

    actionTextBold->setChecked(textEdit->BoldChecked());
    actionTextItalic->setChecked(textEdit->ItalicChecked());
    actionTextStrike->setChecked(textEdit->StrikeChecked());
    actionTextSub->setChecked(textEdit->SubChecked());
    actionTextSup->setChecked(textEdit->SupChecked());

    QString script = "\
    (f = function(node){\
        var tag = node.tagName;\
        if (tag == 'BODY') return '';\
        if (tag == 'DIV') tag = node.getAttribute('CLASS');\
        return f(node.parentNode) + '/' + tag;\
    })(document.getSelection().baseNode.parentNode);";

    QString message = textEdit->page()->mainFrame()->evaluateJavaScript(script).toString();
    statusBar()->showMessage(message);
}

void Fb2MainWindow::undoChanged()
{
    actionUndo->setEnabled(textEdit->UndoEnabled());
}

void Fb2MainWindow::redoChanged()
{
    actionRedo->setEnabled(textEdit->RedoEnabled());
}

void Fb2MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void Fb2MainWindow::readSettings()
{
    QSettings settings;
    QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
    QSize size = settings.value("size", QSize(400, 400)).toSize();
    move(pos);
    resize(size);
}

void Fb2MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("pos", pos());
    settings.setValue("size", size());
}

bool Fb2MainWindow::maybeSave()
{
    if (textEdit && textEdit->isModified()) {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning(this, qApp->applicationName(),
                     tr("The document has been modified. Do you want to save your changes?"),
                     QMessageBox::Save | QMessageBox::Discard
		     | QMessageBox::Cancel);
        if (ret == QMessageBox::Save)
            return fileSave();
        else if (ret == QMessageBox::Cancel)
            return false;
    }
    return true;
}

bool Fb2MainWindow::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, qApp->applicationName(), tr("Cannot write file %1: %2.").arg(fileName).arg(file.errorString()));
        return false;
    }
    if (textEdit) return textEdit->save(&file);
    return true;

/*
    QTextStream out(&file);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    out << textEdit->toPlainText();
    QApplication::restoreOverrideCursor();

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File saved"), 2000);
*/
}

void Fb2MainWindow::setCurrentFile(const QString &filename)
{
    static int sequenceNumber = 1;

    QString title;
    isUntitled = filename.isEmpty();
    if (isUntitled) {
        curFile = QString("book%1.fb2").arg(sequenceNumber++);
        title = curFile;
    } else {
        QFileInfo info = filename;
        curFile = info.canonicalFilePath();
        title = info.fileName();
    }
    title += QString(" - ") += qApp->applicationName();

    setWindowModified(false);
    setWindowFilePath(curFile);
    setWindowTitle(title);
}

Fb2MainWindow *Fb2MainWindow::findFb2MainWindow(const QString &fileName)
{
    QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

    foreach (QWidget *widget, qApp->topLevelWidgets()) {
        Fb2MainWindow *mainWin = qobject_cast<Fb2MainWindow *>(widget);
        if (mainWin && mainWin->curFile == canonicalFilePath)
            return mainWin;
    }
    return 0;
}

void Fb2MainWindow::zoomOrig()
{
    if (codeEdit) codeEdit->zoomTo(1);
}

void Fb2MainWindow::checkScintillaUndo()
{
    if (!codeEdit) return;
    actionUndo->setEnabled(codeEdit->isUndoAvailable());
    actionRedo->setEnabled(codeEdit->isRedoAvailable());
}

void Fb2MainWindow::viewCode()
{
    if (codeEdit && centralWidget() == codeEdit) return;

    bool load = false;
    QByteArray xml;
    QList<int> folds;
    if (textEdit) {
        textEdit->save(&xml, &folds);
        load = true;
    }

    FB2DELETE(textEdit);
    FB2DELETE(dockTree);
    FB2DELETE(headTree);

    if (!codeEdit) {
        codeEdit = new Fb2Scintilla;
    }
    if (load) codeEdit->load(xml, folds);
    setCentralWidget(codeEdit);
    codeEdit->setFocus();

    FB2DELETE(toolEdit);
    QToolBar *tool = toolEdit = addToolBar(tr("Edit"));
    tool->addSeparator();
    tool->addAction(actionUndo);
    tool->addAction(actionRedo);
    tool->addSeparator();
    tool->addAction(actionCut);
    tool->addAction(actionCopy);
    tool->addAction(actionPaste);
    tool->addSeparator();
    tool->addAction(actionZoomIn);
    tool->addAction(actionZoomOut);
    tool->addAction(actionZoomOrig);
    tool->setMovable(false);

    connect(codeEdit, SIGNAL(textChanged()), this, SLOT(documentWasModified()));
    connect(codeEdit, SIGNAL(textChanged()), this, SLOT(checkScintillaUndo()));

    connect(codeEdit, SIGNAL(copyAvailable(bool)), actionCut, SLOT(setEnabled(bool)));
    connect(codeEdit, SIGNAL(copyAvailable(bool)), actionCopy, SLOT(setEnabled(bool)));

    connect(actionUndo, SIGNAL(triggered()), codeEdit, SLOT(undo()));
    connect(actionRedo, SIGNAL(triggered()), codeEdit, SLOT(redo()));

    connect(actionCut, SIGNAL(triggered()), codeEdit, SLOT(cut()));
    connect(actionCopy, SIGNAL(triggered()), codeEdit, SLOT(copy()));
    connect(actionPaste, SIGNAL(triggered()), codeEdit, SLOT(paste()));

    connect(actionZoomIn, SIGNAL(triggered()), codeEdit, SLOT(zoomIn()));
    connect(actionZoomOut, SIGNAL(triggered()), codeEdit, SLOT(zoomOut()));
    connect(actionZoomOrig, SIGNAL(triggered()), this, SLOT(zoomOrig()));
}

void Fb2MainWindow::viewText()
{
    if (textEdit && centralWidget() == textEdit) return;
    QString xml;
    if (codeEdit) xml = codeEdit->text();
    FB2DELETE(codeEdit);
    FB2DELETE(headTree);
    if (!textEdit) {
        textEdit = new Fb2WebView(this);
    }
    setCentralWidget(textEdit);
    textEdit->setFocus();
    viewTree();

    connect(textEdit->page(), SIGNAL(contentsChanged()), SLOT(documentWasModified()));
    connect(textEdit->page(), SIGNAL(selectionChanged()), SLOT(selectionChanged()));
    connect(textEdit, SIGNAL(loadFinished(bool)), SLOT(loadFinished(bool)));

    connect(textEdit->pageAction(QWebPage::Undo), SIGNAL(changed()), SLOT(undoChanged()));
    connect(textEdit->pageAction(QWebPage::Redo), SIGNAL(changed()), SLOT(redoChanged()));
    connect(actionUndo, SIGNAL(triggered()), textEdit->pageAction(QWebPage::Undo), SIGNAL(triggered()));
    connect(actionRedo, SIGNAL(triggered()), textEdit->pageAction(QWebPage::Redo), SIGNAL(triggered()));

    connect(actionCut, SIGNAL(triggered()), textEdit->pageAction(QWebPage::Cut), SIGNAL(triggered()));
    connect(actionCopy, SIGNAL(triggered()), textEdit->pageAction(QWebPage::Copy), SIGNAL(triggered()));
    connect(actionPaste, SIGNAL(triggered()), textEdit->pageAction(QWebPage::Paste), SIGNAL(triggered()));

    connect(actionTextBold, SIGNAL(triggered()), textEdit->pageAction(QWebPage::ToggleBold), SIGNAL(triggered()));
    connect(actionTextItalic, SIGNAL(triggered()), textEdit->pageAction(QWebPage::ToggleItalic), SIGNAL(triggered()));
    connect(actionTextStrike, SIGNAL(triggered()), textEdit->pageAction(QWebPage::ToggleStrikethrough), SIGNAL(triggered()));
    connect(actionTextSub, SIGNAL(triggered()), textEdit->pageAction(QWebPage::ToggleSubscript), SIGNAL(triggered()));
    connect(actionTextSup, SIGNAL(triggered()), textEdit->pageAction(QWebPage::ToggleSuperscript), SIGNAL(triggered()));

    connect(actionImage, SIGNAL(triggered()), textEdit, SLOT(insertImage()));

    connect(actionZoomIn, SIGNAL(triggered()), textEdit, SLOT(zoomIn()));
    connect(actionZoomOut, SIGNAL(triggered()), textEdit, SLOT(zoomOut()));
    connect(actionZoomOrig, SIGNAL(triggered()), textEdit, SLOT(zoomOrig()));

    if (!xml.isEmpty()) textEdit->load(curFile, xml);

    FB2DELETE(toolEdit);
    QToolBar *tool = toolEdit = addToolBar(tr("Edit"));
    tool->setMovable(false);
    tool->addSeparator();

    QAction *act;

    act = textEdit->pageAction(QWebPage::Undo);
    act->setIcon(icon("edit-undo"));
    act->setText(tr("&Undo"));
    act->setPriority(QAction::LowPriority);
    act->setShortcut(QKeySequence::Undo);
    tool->addAction(act);

    act = textEdit->pageAction(QWebPage::Redo);
    act->setIcon(icon("edit-redo"));
    act->setText(tr("&Redo"));
    act->setPriority(QAction::LowPriority);
    act->setShortcut(QKeySequence::Redo);
    tool->addAction(act);

    tool->addSeparator();

    act = textEdit->pageAction(QWebPage::Cut);
    act->setIcon(icon("edit-cut"));
    act->setText(tr("Cu&t"));
    act->setPriority(QAction::LowPriority);
    act->setShortcuts(QKeySequence::Cut);
    act->setStatusTip(tr("Cut the current selection's contents to the clipboard"));
    tool->addAction(act);

    act = textEdit->pageAction(QWebPage::Copy);
    act->setIcon(icon("edit-copy"));
    act->setText(tr("&Copy"));
    act->setPriority(QAction::LowPriority);
    act->setShortcuts(QKeySequence::Copy);
    act->setStatusTip(tr("Copy the current selection's contents to the clipboard"));
    tool->addAction(act);

    act = textEdit->pageAction(QWebPage::Paste);
    act->setIcon(icon("edit-paste"));
    act->setText(tr("&Paste"));
    act->setPriority(QAction::LowPriority);
    act->setShortcuts(QKeySequence::Paste);
    act->setStatusTip(tr("Paste the clipboard's contents into the current selection"));
    tool->addAction(act);

    tool->addSeparator();

    act = textEdit->pageAction(QWebPage::ToggleBold);
    act->setIcon(icon("format-text-bold"));
    act->setText(tr("&Bold"));
    act->setShortcuts(QKeySequence::Bold);
    tool->addAction(act);

    act = textEdit->pageAction(QWebPage::ToggleItalic);
    act->setIcon(icon("format-text-italic"));
    act->setText(tr("&Italic"));
    act->setShortcuts(QKeySequence::Italic);
    tool->addAction(act);

    act = textEdit->pageAction(QWebPage::ToggleStrikethrough);
    act->setIcon(icon("format-text-strikethrough"));
    act->setText(tr("&Strikethrough"));
    tool->addAction(act);

    act = textEdit->pageAction(QWebPage::ToggleSuperscript);
    act->setIcon(icon("format-text-superscript"));
    act->setText(tr("Su&perscript"));
    tool->addAction(act);

    act = textEdit->pageAction(QWebPage::ToggleSubscript);
    act->setIcon(icon("format-text-subscript"));
    act->setText(tr("Su&bscript"));
    tool->addAction(act);

    tool->addSeparator();

    tool->addAction(actionImage);
    tool->addAction(actionNote);
    tool->addAction(actionLink);
    tool->addAction(actionBody);

    tool->addSeparator();

    tool->addAction(actionZoomIn);
    tool->addAction(actionZoomOut);
    tool->addAction(actionZoomOrig);
}

void Fb2MainWindow::viewHead()
{
    if (headTree && centralWidget() == headTree) return;

    QString xml;
    if (codeEdit) xml = codeEdit->text();

    FB2DELETE(dockTree);
    FB2DELETE(codeEdit);
    FB2DELETE(toolEdit);

    if (!headTree) {
        headTree = new QTreeView(this);
        headTree->header()->setDefaultSectionSize(200);
    }
    if (textEdit) {
        this->setFocus();
        textEdit->setParent(NULL);
        setCentralWidget(headTree);
        textEdit->setParent(this);
        Fb2HeadModel *model = new Fb2HeadModel(*textEdit, treeView);
        headTree->setModel(model);
        model->expand(headTree);
    } else {
        textEdit = new Fb2WebView(this);
        connect(textEdit, SIGNAL(loadFinished(bool)), SLOT(loadFinished(bool)));
        setCentralWidget(headTree);
    }
    headTree->setFocus();

    if (!xml.isEmpty()) textEdit->load(curFile, xml);

    if (textEdit) {
        actionUndo->disconnect();
        actionRedo->disconnect();

        actionCut->disconnect();
        actionCopy->disconnect();
        actionPaste->disconnect();

        actionTextBold->disconnect();
        actionTextItalic->disconnect();
        actionTextStrike->disconnect();
        actionTextSub->disconnect();
        actionTextSup->disconnect();
    }

    FB2DELETE(toolEdit);
    QToolBar *tool = toolEdit = addToolBar(tr("Edit"));
    tool->addSeparator();
    tool->addAction(actionInsert);
    tool->addAction(actionDelete);
    tool->setMovable(false);
}

void Fb2MainWindow::viewTree()
{
    if (centralWidget() != textEdit) return;
    if (treeView == NULL) createTree();
    loadFinished(true);
}

void Fb2MainWindow::clipboardDataChanged()
{
    if (const QMimeData *md = QApplication::clipboard()->mimeData()) {
        actionPaste->setEnabled(md->hasText());
    }
}

void Fb2MainWindow::showInspector()
{
    if (!textEdit) return;
    QWebInspector * inspector = new QWebInspector();
    inspector->setPage(textEdit->page());
    inspector->show();
}

