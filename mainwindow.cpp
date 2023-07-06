
#include <QtWidgets>

#include "mainwindow.h"

#include <osmium/io/any_output.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/tags/taglist.hpp>
#include <osmium/tags/tags_filter.hpp>
#include <osmium/handler/object_relations.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/index/multimap/sparse_mem_array.hpp>
#include <osmium/relations/relations_manager.hpp>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>

using map_type = osmium::index::multimap::SparseMemArray<osmium::unsigned_object_id_type, osmium::unsigned_object_id_type>;

class ParsManager : public osmium::relations::RelationsManager<ParsManager, true, true, true> {

public:
    bool new_relation(const osmium::Relation& relation) noexcept {
        return relation.tags().has_tag("route", "tram");
    }

    bool new_member(const osmium::Relation& /*relation*/, const osmium::RelationMember& /*member*/, std::size_t /*n*/) noexcept {
        return true;
    }

    void complete_relation(const osmium::Relation& relation) {
        // Iterate over all members
        for (const auto& member : relation.members()) {
            // member.ref() will be 0 for all members you are not interested
            // in. The objects for those members are not available.
            if (member.ref() != 0) {
                // Get the member object
                const osmium::OSMObject* obj = this->get_member_object(member);

                // If you know which type you have you can also use any of these:
                const osmium::Node* node         = this->get_member_node(member.ref());
                const osmium::Way* way           = this->get_member_way(member.ref());
                const osmium::Relation* relation = this->get_member_relation(member.ref());
            }
        }
    }


};

MainWindow::MainWindow()
{
    textEdit = new QPlainTextEdit;
    setCentralWidget(textEdit);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    readSettings();

    connect(textEdit->document(), SIGNAL(contentsChanged()),
            this, SLOT(documentWasModified()));

    setCurrentFile("");
    setUnifiedTitleAndToolBarOnMac(true);
}



void MainWindow::closeEvent(QCloseEvent *event)

{
    if (maybeSave()) {
        writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::newFile()

{
    if (maybeSave()) {
        textEdit->clear();
        setCurrentFile("");
    }
}

void MainWindow::open()

{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this);
        if (!fileName.isEmpty())
            loadFile(fileName);
    }
}

bool MainWindow::exportTramsDo()

{

    QString fileName = QFileDialog::getOpenFileName(this);//Диалог выбора открываемого файла
    if (!fileName.isEmpty())
        loadFile(fileName);

    std::string input_file_name = fileName.toStdString();
    QString fileName2 = QFileDialog::getSaveFileName(this);//Диалог выбора выходного файла или создания нового
    if (fileName2.isEmpty())
        return false;

    std::string output_file_name = fileName2.toStdString();

    try {
        int noloopcut = 0;
            // Инициализация ридера
            osmium::io::File input_file{input_file_name};
            osmium::io::Reader reader{input_file_name, osmium::osm_entity_bits::all};
            osmium::io::Reader reader2{input_file_name, osmium::osm_entity_bits::all};
            //osmium::io::Reader reader2{input_file_name, osmium::osm_entity_bits::relation}; //Только отношения
           // osmium::io::Reader reader{"input.osm.pbf", osmium::osm_entity_bits::way};//Позволяет
            // считывать только сами пути, игнорируя ненужные в данный момент детали

            osmium::io::Header header = reader.header();//Берем заголовок, но не меняем его
            //header.set("generator", "osmium_convert");

            osmium::io::Writer writer{output_file_name};

            //Фильтруем только трамвайные пути по тегам
            osmium::TagsFilter filter1{false};

            ParsManager mngr;//без аргументов, как в доках

            osmium::relations::read_relations(input_file, mngr);



            filter1.add_rule(true, "railway", "tram");//Трамвайный путь
            filter1.add_rule(true, "railway", "tram_crossing");// Пересечение трамвайных путей
            filter1.add_rule(true, "railway", "tram_level_crossing");// Пересечение путей на разных уровнях?
            filter1.add_rule(true, "route", "tram");// Маршруты трамвая
            filter1.add_rule(true, "tram", "yes");// Остановки трамвая
            filter1.add_rule(true, "railway", "tram_stop");// Остановки трамвая 2, потому что кому вообще нужны стандарты?
            filter1.add_rule(true, "light_rail", "*");// Маршруты трамвая 2, потому что кому вообще нужны стандарты?
            filter1.add_rule(true, "disused:railway", "tram");//Заброшенные пути
            filter1.add_rule(true, "disused", "tram");//Заброшенные пути
            //filter1.add_rule(true, "building", "yes");//Домики, чтобы было похоже на карту

            // Получаем все объекты данного типа.
            while (osmium::memory::Buffer buffer = reader.read()) { // NOLINT(bugprone-use-after-move) Bug in clang-tidy https://bugs.llvm.org/show_bug.cgi?id=36516
                /*for (const auto& object : buffer.select<osmium::OSMObject>()) {// Выше описан какой-то баг чего-то низкоуровневого
                    if (osmium::tags::match_any_of(object.tags(), filter1)) {
                        writer(object);
                    }
                }*/
                for (const auto& nod : buffer.select<osmium::Node>()) {
                    if (osmium::tags::match_any_of(nod.tags(), filter1)) {
                        writer(nod);
                    }
                }

                for (const auto& wei : buffer.select<osmium::Way>()) {
                    if (osmium::tags::match_any_of(wei.tags(), filter1)) {
                        writer(wei);
                    }
                }

                for (const auto& rltn : buffer.select<osmium::Way>()) {
                    if (osmium::tags::match_any_of(rltn.tags(), filter1)) {
                        writer(rltn);
                    }
                }
                noloopcut++;
            }



            for(int i = 0; i < noloopcut; i++) {
                osmium::memory::Buffer buffer = reader2.read();
                for (const auto& object : buffer.select<osmium::OSMObject>()) {
                    if (osmium::tags::match_any_of(object.tags(), filter1)) {
                        osmium::apply(reader, mngr.handler());
                        writer(object);                             //TODO Can not read from reader when in status 'closed', 'eof', or 'error'
                                                                    //https://osmcode.org/libosmium/manual.html#working-with-relations
                    }
                }
            }

            // Жестко прибиваем IO на считывание и запись. Если есть проблема,
            // она обнаружится на этом этапе.
            // destructors must not throw
            writer.close();
            reader.close();
            reader2.close();

        } catch (const std::exception& e) {
            // All exceptions used by the Osmium library derive from std::exception.
        //Стандартное исключение
            std::cerr << e.what() << '\n';
            return 1;
        }






    return false;
}

bool MainWindow::save()

{
    if (curFile.isEmpty()) {
        return saveAs();
    } else {
        return saveFile(curFile);
    }
}

bool MainWindow::saveAs()

{
    QString fileName = QFileDialog::getSaveFileName(this);
    if (fileName.isEmpty())
        return false;

    return saveFile(fileName);
}

void MainWindow::about()

{
   QMessageBox::about(this, tr("О приложении"),
            tr("<b>OSMParser</b> позволяет выгружать из файлов "
               "формата OpenStreetMap (.XML, .PBF и т.д.)"
               "данные о Ж/Д путях и добавлять их в базу данных"));
}

void MainWindow::documentWasModified()

{
    setWindowModified(textEdit->document()->isModified());
}

void MainWindow::createActions()

{
    newAct = new QAction(QIcon(":/images/new.png"), tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    newAct->setStatusTip(tr("Create a new file"));
    connect(newAct, SIGNAL(triggered()), this, SLOT(newFile()));

    openAct = new QAction(QIcon(":/images/open.png"), tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    saveAct = new QAction(QIcon(":/images/save.png"), tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the document to disk"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

    exportTramsAct = new QAction(QIcon(":/images/save.png"), tr("&Эскпорт трамв. путей"), this);
    exportTramsAct->setShortcuts(QKeySequence::Save);
    exportTramsAct->setStatusTip(tr("Экспорт трамвайных путей"));
    connect(exportTramsAct, SIGNAL(triggered()), this, SLOT(exportTramsDo()));

    saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    saveAsAct->setStatusTip(tr("Save the document under a new name"));
    connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    cutAct = new QAction(QIcon(":/images/cut.png"), tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Cut);
    cutAct->setStatusTip(tr("Cut the current selection's contents to the "
                            "clipboard"));
    connect(cutAct, SIGNAL(triggered()), textEdit, SLOT(cut()));

    copyAct = new QAction(QIcon(":/images/copy.png"), tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    copyAct->setStatusTip(tr("Copy the current selection's contents to the "
                             "clipboard"));
    connect(copyAct, SIGNAL(triggered()), textEdit, SLOT(copy()));

    pasteAct = new QAction(QIcon(":/images/paste.png"), tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
                              "selection"));
    connect(pasteAct, SIGNAL(triggered()), textEdit, SLOT(paste()));

    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    aboutQtAct = new QAction(tr("About &Qt"), this);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
    connect(aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

    cutAct->setEnabled(false);
    copyAct->setEnabled(false);
    connect(textEdit, SIGNAL(copyAvailable(bool)),
            cutAct, SLOT(setEnabled(bool)));
    connect(textEdit, SIGNAL(copyAvailable(bool)),
            copyAct, SLOT(setEnabled(bool)));
}



void MainWindow::createMenus()

{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(exportTramsAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);
    editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(cutAct);
    editMenu->addAction(copyAct);
    editMenu->addAction(pasteAct);

    menuBar()->addSeparator();

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);
}

void MainWindow::createToolBars()
{
    fileToolBar = addToolBar(tr("File"));
    fileToolBar->addAction(newAct);
    fileToolBar->addAction(openAct);
    fileToolBar->addAction(saveAct);
    editToolBar = addToolBar(tr("Edit"));
    editToolBar->addAction(cutAct);
    editToolBar->addAction(copyAct);
    editToolBar->addAction(pasteAct);
}

void MainWindow::createStatusBar()

{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::readSettings()

{
    QSettings settings("QtProject", "Application Example");
    QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
    QSize size = settings.value("size", QSize(400, 400)).toSize();
    resize(size);
    move(pos);
}

void MainWindow::writeSettings()

{
    QSettings settings("QtProject", "Application Example");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
}

bool MainWindow::maybeSave()

{
    if (textEdit->document()->isModified()) {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning(this, tr("Application"),
                     tr("The document has been modified.\n"
                        "Do you want to save your changes?"),
                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Save)
            return save();
        else if (ret == QMessageBox::Cancel)
            return false;
    }
    return true;
}

void MainWindow::loadFile(const QString &fileName)

{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return;
    }
/*
    QTextStream in(&file);
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    textEdit->setPlainText(in.readAll());
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif
*/
    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File loaded"), 2000);
}

bool MainWindow::saveFile(const QString &fileName)

{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return false;
    }

    QTextStream out(&file);
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    out << textEdit->toPlainText();
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

bool MainWindow::exportTrams(const QString &fileName)

{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return false;
    }

    QTextStream out(&file);
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    out << textEdit->toPlainText();
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

void MainWindow::setCurrentFile(const QString &fileName)

{
    curFile = fileName;
    textEdit->document()->setModified(false);
    setWindowModified(false);

    QString shownName = curFile;
    if (curFile.isEmpty())
        shownName = "untitled.txt";
    setWindowFilePath(shownName);
}

QString MainWindow::strippedName(const QString &fullFileName)

{
    return QFileInfo(fullFileName).fileName();
}


