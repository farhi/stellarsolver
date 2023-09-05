// An executable to solve-plate images (determine RA/DEC location from visible stars)
//
// Build with:
// mkdir build
// cd build
// cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTER=OFF -DBUILD_DEMOS=ON .. 
// make -j 4
//
// Get index files from <http://data.astrometry.net/>
// or install     astrometry-data-tycho2 (300 MB)
// and optionally astrometry-data-2mass (39 GB)

#include <QApplication>

//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "ssolverutils/fileio.h"

#ifndef EXIT_FAILURE
#define EXIT_FAILURE -1
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

void print_usage(char *pgmname)
{ /* Show program help. pgmname = argv[0] */
  printf( "Usage: %s [options] file1 file2 ...\n", pgmname);
  printf( "This tool determines RA/DEC location from visible stars (solve-plate)\n");
  printf( "Version: %s\n", StellarSolver_BUILD_TS);
  printf( "Options:\n");
  printf( "  -Idir      Add 'dir' to the list of locations holding Astrometry star catalog index files.\n");
  printf( "             Any environment variable ASTROMETRY_INDEX_FILES will also be added.\n"); 
  printf( "  --help -h  Display this help and version.\n");
  printf( "The program return-code is the number of actually processed images.\n");
  exit(EXIT_SUCCESS);
} // print_usage

// from stellarsolver.cpp
void addPathToListIfExists(QStringList *list, QString path)
{
    if(list)
    {
        if(QFileInfo::exists(path))
            list->append(path);
    }
}

int main(int argc, char *argv[])
{
    int i;
    int imageFilesCounter=0;
    QStringList catalogDirectories;
    QStringList imageFiles;
    
    catalogDirectories = StellarSolver::getDefaultIndexFolderPaths();
    
    // Parse options -----------------------------------------------------------
    for(i = 1; i < argc; i++)
    {
      if(!strncmp("-I", argv[i], 2) && strlen(argv[i]) > 2)
      {  // add a directory to hold star catalog index files
        addPathToListIfExists(&catalogDirectories, QString(argv[i]+2));
      }
      else if(argv[i][0] != '-') 
      {
        // does not start with '-' -> add a file to process
        addPathToListIfExists(&imageFiles, QString(argv[i]));
      }
      else print_usage(argv[0]);  
    }
    // Add env  ASTROMETRY_INDEX_FILES
    if (getenv("ASTROMETRY_INDEX_FILES"))
      addPathToListIfExists(&catalogDirectories, QString(getenv("ASTROMETRY_INDEX_FILES")));
    
    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    fileio imageLoader;
    
    // loop on images ----------------------------------------------------------
    for(int i = 0; i < imageFiles.count(); i++)
    {
      const QString &currentImage = imageFiles[i];
      if(!imageLoader.loadImage(currentImage))
      {
          printf("Error in loading image %s\n", currentImage.toStdString().c_str());
          break;
      }
      FITSImage::Statistic stats = imageLoader.getStats();
      uint8_t *imageBuffer = imageLoader.getImageBuffer();
      
      StellarSolver stellarSolver(stats, imageBuffer);
      stellarSolver.setIndexFolderPaths(catalogDirectories);

      printf("Starting to solve %s\n", currentImage.toStdString().c_str());
      fflush( stdout );

      if(!stellarSolver.solve())
      {
          printf("Plate Solve Failed %s\n", currentImage.toStdString().c_str());
          break;
      }

      FITSImage::Solution solution = stellarSolver.getSolution();
      printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

      printf("Field center: (RA,Dec) = (%f, %f) deg.\n", solution.ra, solution.dec);
      printf("Field size: %f x %f arcminutes\n", solution.fieldWidth, solution.fieldHeight);
      printf("Pixel Scale: %f\"\n", solution.pixscale);
      printf("Field rotation angle: up is %f degrees E of N\n", solution.orientation);
      printf("Field parity: %s\n", FITSImage::getParityText(solution.parity).toUtf8().data());
      fflush( stdout );
      imageFilesCounter++;

      stellarSolver.setParameterProfile(SSolver::Parameters::ALL_STARS);

      if(!stellarSolver.extract(true))
      {
          printf("Solver Star extraction Failed %s", currentImage.toStdString().c_str());
          break;
      }

      QList<FITSImage::Star> starList = stellarSolver.getStarList();
      printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
      printf("Stars found: %u\n", starList.count());
      for(int i=0; i < starList.count(); i++)
      {
          FITSImage::Star star = starList.at(i);
          char *ra = StellarSolver::raString(star.ra).toUtf8().data();
          char *dec = StellarSolver::decString(star.dec).toUtf8().data();
          printf("Star #%u: (%f x, %f y), (ra: %s,dec: %s), mag: %f, peak: %f, hfr: %f \n", i, star.x, star.y, ra , dec, star.mag, star.peak, star.HFR);
      }
    } // loop on images
    
    return imageFilesCounter;
}
