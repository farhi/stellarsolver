/*  MainWindow for SexySolver Tester Application, developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//Includes for this project
#include "structuredefinitions.h"
#include "sexysolver.h"
#include "externalsextractorsolver.h"

//system includes
#include "math.h"

#ifndef _MSC_VER
#include <sys/mman.h>
#endif

//QT Includes
#include <QMainWindow>
#include <QObject>
#include <QWidget>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QProcess>
#include <QPointer>
#include <QScrollBar>
#include <QTime>
#include <QThread>
#include <QLineEdit>

//CFitsio Includes
#include "longnam.h"
#include "fitsio.h"

//KStars related includes
#include "stretch.h"
#include "math.h"
#include "dms.h"
#include "bayer.h"

//Astrometry.net includes
extern "C"{
#include "tic.h"
#include "os-features.h"
#include "fileutils.h"
#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "fitsioutils.h"
#include "blindutils.h"
#include "astrometry/blind.h"
#include "log.h"
#include "engine.h"
#include "gslutils.h"
#include "augment-xylist.h"
#include "anqfits.h"
#include "ioutils.h"
#include "fitsioutils.h"
#include "permutedsort.h"
#include "fitstable.h"
#include "sip_qfits.h"
#include "sip-utils.h"
#include "tabsort.h"
}

namespace Ui {

class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow();
    ~MainWindow();

    void setStarList(QList<Star> starList){stars = starList;};

private:
    Ui::MainWindow *ui;

    QPointer<SexySolver> sexySolver;
    QPointer<ExternalSextractorSolver> extSolver;
    QString fileToSolve;
    QList<Star> stars;
    int selectedStar;

    QString dirPath = QDir::homePath();

    //Options for the External Sextractor and Solver
    bool cleanupTemporaryFiles;
    QString tempPath;
    QStringList indexFilePaths;
    QString sextractorBinaryPath;
    QString confPath;
    QString solverPath;
    QString wcsPath;

    //Parameters for sextracting
    bool calculateHFR;
    Shape apertureShape;
    double kron_fact;
    int subpix;
    float r_min;
    short inflags;
    float magzero;
    float minarea;
    int deblend_thresh;
    float deblend_contrast;
    int clean;
    double clean_param;
    double fwhm;

    //Star Filter Parameters
    bool resort;
    double maxEllipse;
    double removeBrightest;
    double removeDimmest;
    double saturationLimit;

    //Parameters for solving
    bool use_scale = false;
    double fov_low, fov_high;
    QString units;
    bool use_position = false;
    double ra;
    double dec;
    double radius;

    bool inParallel;
    int solverTimeLimit;
    double minwidth;
    double maxwidth;

    bool logToFile;
    QString logFile;
    int logLevel;

    //LogOdds Settings
    double logratio_tosolve;
    double logratio_tokeep;
    double logratio_totune;

    //Data about the image
    bool imageLoaded = false;
    Statistic stats;
    fitsfile *fptr { nullptr };
    QImage rawImage;
    QImage scaledImage;
    int currentWidth;
    int currentHeight;
    double currentZoom;
    int sampling = 2;
    /// Number of channels
    uint8_t m_Channels { 1 };
    /// Generic data image buffer
    uint8_t *m_ImageBuffer { nullptr };
    /// Above buffer size in bytes
    uint32_t m_ImageBufferSize { 0 };
    StretchParams stretchParams;
    BayerParams debayerParams;
    bool checkDebayer();

    QTime solverTimer;
    double elapsed;

    void setupSolutionTable();
    void clearAstrometrySettings();
    void addSextractionToTable();

public slots:

    bool prepareForProcesses();
    void logOutput(QString text);
    void clearAll();
    void resetOptionsToDefaults();

    //These are the functions that run when the bottom buttons are clicked
    bool sextractImage();
    bool solveImage();
    bool classicSolve();
    bool sextractExternally();
    bool sextractAndSolveExternally();
    bool sextractInternally();
    bool sextractAndSolveInternally();
    void abort();

    //These functions are for loading and displaying the image
    bool imageLoad();
    bool loadFits();
    bool loadOtherFormat();
    bool debayer();
    bool debayer_8bit();
    bool debayer_16bit();
    void initDisplayImage();
    void doStretch(QImage *outputImage);
    void clearImageBuffers();

    void zoomIn();
    void zoomOut();
    void autoScale();
    void updateImage();

    //These functions are for the display of stars in the table
    void displayTable();
    void sortStars();
    void starClickedInTable();
    void updateStarTableFromList();

    void mouseMovedOverImage(QPoint location);
    QString getValue(int x, int y);
    void mouseClickedOnStar(QPoint location);
    QRect getStarSizeInImage(Star star);

    //This function is for loading and parsing the options
    bool getSolverOptionsFromFITS();

    //These functions set the settings for the Sextractors and Solvers
    void setSextractorSettings();
    void setSolverSettings();
    void setupExternalSextractorSolver();
    void setupInternalSexySolver();

    //These functions get called when the sextractor or solver finishes
    bool sextractorComplete(int error);
    bool solverComplete(int error);
    bool classicSolverComplete(int error);

    //These functions handle the solution table
    void addSolutionToTable(Solution solution);
    void addClassicSolutionToTable(Solution solution);
    void saveSolutionTable();

};

#endif // MAINWINDOW_H
