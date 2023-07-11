
#include <QtWidgets>

#include "mainwindow.h"

#include <osmium/io/any_output.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/tags/taglist.hpp>
#include <osmium/tags/tags_filter.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/box.hpp>
#include <osmium/handler/object_relations.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/index/multimap/sparse_mem_array.hpp>
#include <osmium/relations/relations_manager.hpp>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <limits>

template <class FloatType>
int safeFloatToInt(const FloatType &num) {
   //check if float fits into integer
   if ( std::numeric_limits<int>::digits < std::numeric_limits<FloatType>::digits) {
      // check if float is smaller than max int
      if( (num < static_cast<FloatType>( std::numeric_limits<int>::max())) &&
          (num > static_cast<FloatType>( std::numeric_limits<int>::min())) ) {
         return static_cast<int>(num); //safe to cast
      } else {
        std::cerr << "Unsafe conversion of value:" << num << endl;
        //NaN is not defined for int return the largest int value
        return std::numeric_limits<int>::max();
      }
   } else {
      //It is safe to cast
      return static_cast<int>(num);
   }
}

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

                // Работаю с данными здесь
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
    //createToolBars();
    createStatusBar();

    readSettings();

    connect(textEdit->document(), SIGNAL(contentsChanged()),
            this, SLOT(documentWasModified()));

    setCurrentFile("");
    setUnifiedTitleAndToolBarOnMac(true);
}



void MainWindow::closeEvent(QCloseEvent *event)
{
        event->accept();
}




void Tgf(std::string input_file, std::string output_file){
    try {
        // Initialize Reader
        osmium::io::Reader reader{input_file};

        // Get header from input file and change the "generator" setting to
        // ourselves.
        osmium::io::Header header = reader.header();
        header.set("generator", "osmium_convert");

        // Initialize Writer using the header from above and tell it that it
        // is allowed to overwrite a possibly existing file.
        osmium::io::Writer writer{output_file, header, osmium::io::overwrite::allow};

        // Match highway=primary or highway=secondary
        osmium::TagsFilter filter1{false};
        filter1.add_rule(true, "highway", "primary");
        filter1.add_rule(true, "highway", "secondary");

        // Match oneway=yes
        osmium::TagsFilter filter2{false};
        filter2.add_rule(true, "oneway", "yes");

        // Get all object matching both filters
        while (osmium::memory::Buffer buffer = reader.read()) { // NOLINT(bugprone-use-after-move) Bug in clang-tidy https://bugs.llvm.org/show_bug.cgi?id=36516
            for (const auto& object : buffer.select<osmium::OSMObject>()) {
                if (osmium::tags::match_any_of(object.tags(), filter1) &&
                    osmium::tags::match_any_of(object.tags(), filter2)) {
                    writer(object);
                }
            }
        }

        // Explicitly close the writer and reader. Will throw an exception if
        // there is a problem. If you wait for the destructor to close the writer
        // and reader, you will not notice the problem, because destructors must
        // not throw.
        writer.close();
        reader.close();
    } catch (const std::exception& e) {
        // All exceptions used by the Osmium library derive from std::exception.
        std::cerr << e.what() << '\n';
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
            osmium::io::Reader reader{input_file_name, osmium::osm_entity_bits::relation};
            osmium::io::Reader reader1{input_file_name, osmium::osm_entity_bits::way};
            osmium::io::Reader reader2{input_file_name, osmium::osm_entity_bits::node};

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



            //Создаем контейнер (set) для хранения имен (id) узлов (nodes)
            // встреченных при первом обходе путей
            std::set<osmium::object_id_type> w_nodes;

            //Создаем контейнер (set) для хранения имен (id) путей (ways)
            // встреченных при первом обходе отношений
            std::set<osmium::object_id_type> r_ways;

             // Получаем все объекты данного типа.
            while (osmium::memory::Buffer buffer = reader.read()) {

                for (const auto& rltn : buffer.select<osmium::Relation>()) {
                    if (osmium::tags::match_any_of(rltn.tags(), filter1)) {
                        writer(rltn);
                        for (const osmium::RelationMember& nr : rltn.members()) {
                            //std::cout << "ref=" << nr.ref() << " location=" << nr.location() << '\n';
                            if(nr.type()==osmium::item_type::node){
                                w_nodes.insert(nr.ref());
                            }
                            if(nr.type()==osmium::item_type::way){
                                r_ways.insert(nr.ref());
                            }
                        }
                    }
                }
                noloopcut++;
            }
            reader.close();

            std::cout << "w_nodes count=" << w_nodes.size() << " r_ways =" << r_ways.size() << '\n';

            while (osmium::memory::Buffer buffer = reader1.read()) {
               for (const auto& weis : buffer.select<osmium::Way>()) {
                   if (r_ways.count(weis.id())||osmium::tags::match_any_of(weis.tags(), filter1)) {
                       writer(weis);
                       for (const osmium::NodeRef& nr : weis.nodes()) {
                           //std::cout << "ref=" << nr.ref() << " location=" << nr.location() << '\n';
                         //  if(nr.x() > targ_lon1 && nr.x() < targ_lon2 &&
                         //          nr.y() > targ_lat1 && nr.y() < targ_lat2){
                           w_nodes.insert(nr.ref());
                           //std::cout << "loc=" << nr.location();
                       //}
                           //std::cout << " way loc= " << nr.x() << " " << nr.y() << " ";
                   }

               }
            }
            }
            reader1.close();

            while (osmium::memory::Buffer buffer = reader2.read()) {
               for (const auto& nod : buffer.select<osmium::Node>()) {
                   if (w_nodes.count(nod.id())||osmium::tags::match_any_of(nod.tags(), filter1)) {
                       int targ_lon_int = safeFloatToInt(nod.location().lon()*10000000);
                       int targ_lat_int = safeFloatToInt(nod.location().lat()*10000000);
                       if(targ_lon_int > targ_lon1 && targ_lon_int < targ_lon2 &&
                               targ_lat_int > targ_lat1 && targ_lat_int < targ_lat2){
                                writer(nod);


                            }
                       std::cout << " node loc= " << targ_lon_int << " " << targ_lat_int << " \n";
                       }
               }
            }

            // Жестко прибиваем IO на считывание и запись. Если есть проблема,
            // она обнаружится на этом этапе.
            // destructors must not throw
            writer.close();
            reader2.close();

        } catch (const std::exception& e) {
            // All exceptions used by the Osmium library derive from std::exception.
        //Стандартное исключение
            std::cerr << e.what() << '\n';
            return 1;
        }
    return false;
}
void MainWindow::documentWasModified()

{
    setWindowModified(textEdit->document()->isModified());
}

void MainWindow::createActions()

{

    exportTramsAct = new QAction(QIcon(":/images/save.png"), tr("&Эскпорт трамв. путей"), this);
    exportTramsAct->setShortcuts(QKeySequence::Save);
    exportTramsAct->setStatusTip(tr("Экспорт трамвайных путей"));
    connect(exportTramsAct, SIGNAL(triggered()), this, SLOT(exportTramsDo()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

}



void MainWindow::createMenus()

{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(exportTramsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);
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


